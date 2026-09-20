// Ported from Source, but modified for GoldSrc

#include <vgui/IVGui.h>
#include <vgui/ISystem.h>
#include "CDialogGameInfo.h"
#include "CVACBannedDialog.h"
#include "gameui/gameui_viewport.h"
#include "../CServerBrowser.h"
#include "CDialogServerPassword.h"

static const long RETRY_TIME = 10000;		// refresh server every 10 seconds
static const long CHALLENGE_ENTRIES = 1024;

bool QueryLessFunc( const struct challenge_s &item1, const struct challenge_s &item2 )
{
	// compare port then ip
	if ( item1.addr.GetConnectionPort() < item2.addr.GetConnectionPort() )
		return true;
	else if ( item1.addr.GetConnectionPort() > item2.addr.GetConnectionPort() )
		return false;

	int ip1 = item1.addr.GetIP();
	int ip2 = item2.addr.GetIP();

	return ip1 < ip2;
}


CDialogGameInfo::CDialogGameInfo( vgui2::Panel *parent, int nIP, int iPort, unsigned short unPort ) : 
	Frame(parent, "DialogGameInfo"),
	m_CallbackPersonaStateChange( this, &CDialogGameInfo::OnPersonaStateChange )
{
	SetBounds(0, 0, 512, 512);
	SetMinimumSize(GetScaledValue(416), GetScaledValue(340));
	SetDeleteSelfOnClose(true);
	SetProportional(true);
	m_bConnecting = false;
	m_bServerFull = false;
	m_bShowAutoRetryToggle = false;
	m_bServerNotResponding = false;
	m_bShowingExtendedOptions = false;
	m_SteamIDFriend = 0;
	m_hPingQuery = HSERVERQUERY_INVALID;
	m_hPlayersQuery = HSERVERQUERY_INVALID;
	m_bPlayerListUpdatePending = false;
	m_bClosing = false;

	m_szPassword[0] = 0;

	m_pConnectButton = new vgui2::Button(this, "Connect", "#ServerBrowser_JoinGame");
	m_pCloseButton = new vgui2::Button(this, "Close", "#ServerBrowser_Close");
	m_pRefreshButton = new vgui2::Button(this, "Refresh", "#ServerBrowser_Refresh");
	m_pInfoLabel = new vgui2::Label(this, "InfoLabel", "");
	m_pAutoRetry = new vgui2::ToggleButton(this, "AutoRetry", "#ServerBrowser_AutoRetry");
	m_pAutoRetry->AddActionSignalTarget(this);

	m_pAutoRetryAlert = new vgui2::RadioButton(this, "AutoRetryAlert", "#ServerBrowser_AlertMeWhenSlotOpens");
	m_pAutoRetryJoin = new vgui2::RadioButton(this, "AutoRetryJoin", "#ServerBrowser_JoinWhenSlotOpens");
	m_pPlayerList = new vgui2::ListPanel(this, "PlayerList");
	m_pPlayerList->AddColumnHeader(0, "PlayerName", "#ServerBrowser_PlayerName", 156);
	m_pPlayerList->AddColumnHeader(1, "Time", "#ServerBrowser_Time", 64);

	m_pPlayerList->SetSortFunc(1, &PlayerTimeColumnSortFunc);

	// set the defaults for sorting
	// hack, need to make this more explicit functions in ListPanel
	PostMessage(m_pPlayerList, new KeyValues("SetSortColumn", "column", 2));
	PostMessage(m_pPlayerList, new KeyValues("SetSortColumn", "column", 1));

	m_pAutoRetryAlert->SetSelected(true);

	m_pConnectButton->SetCommand(new KeyValues("Connect"));
	m_pCloseButton->SetCommand(new KeyValues("Close"));
	m_pRefreshButton->SetCommand(new KeyValues("Refresh"));

	m_iRequestRetry = 0;

	// create a new server to watch
	memset(&m_Server, 0, sizeof(m_Server) );
	m_Server.m_NetAdr.Init( nIP, iPort, unPort );

	// refresh immediately
	RequestInfo();

	// let us be ticked every frame
	vgui2::ivgui()->AddTickSignal(this->GetVPanel());

	LoadControlSettings( "Servers/DialogGameInfo.res" );
	RegisterControlSettingsFile( "Servers/DialogGameInfo_SinglePlayer.res" );
	RegisterControlSettingsFile( "Servers/DialogGameInfo_AutoRetry.res" );
	MoveToCenterOfScreen();

	m_pPasswordDialog = nullptr;
}


CDialogGameInfo::~CDialogGameInfo()
{
	if ( !GetSteamAPI()->SteamMatchmakingServers() )
		return;

	if ( m_hPingQuery != HSERVERQUERY_INVALID )
		GetSteamAPI()->SteamMatchmakingServers()->CancelServerQuery( m_hPingQuery );
	if ( m_hPlayersQuery != HSERVERQUERY_INVALID )
		GetSteamAPI()->SteamMatchmakingServers()->CancelServerQuery( m_hPlayersQuery );
	if ( m_hRulesQuery != HSERVERQUERY_INVALID )
		GetSteamAPI()->SteamMatchmakingServers()->CancelServerQuery( m_hRulesQuery );
}


void CDialogGameInfo::SendPlayerQuery( uint32 unIP, uint16 usQueryPort )
{
	if ( !GetSteamAPI()->SteamMatchmakingServers() )
		return;

	if ( m_hPlayersQuery != HSERVERQUERY_INVALID )
		GetSteamAPI()->SteamMatchmakingServers()->CancelServerQuery( m_hPlayersQuery );
	m_hPlayersQuery = GetSteamAPI()->SteamMatchmakingServers()->PlayerDetails( unIP, usQueryPort, this );
	m_bPlayerListUpdatePending = true;
}


void CDialogGameInfo::RulesResponded( const char* pchRule, const char* pchValue )
{
	if (!Q_stricmp(pchRule, "secure"))
		m_Server.m_bSecure = ( atoi( pchValue ) == 1 ) ? true : false;
	else if (!Q_stricmp(pchRule, "password"))
		m_Server.m_bPassword = ( atoi( pchValue ) == 1 ) ? true : false;
	else if (!Q_stricmp(pchRule, "players_bot"))
		m_Server.m_nBotPlayers = atoi( pchValue );
	else if (!Q_stricmp(pchRule, "players"))
		m_Server.m_nPlayers = atoi( pchValue );
	else if (!Q_stricmp(pchRule, "players_max"))
		m_Server.m_nMaxPlayers = atoi( pchValue );
	else if (!Q_stricmp(pchRule, "hostname"))
		m_Server.SetName( pchValue );
	else if (!Q_stricmp(pchRule, "map"))
		Q_snprintf( m_Server.m_szMap, sizeof( m_Server.m_szMap ), "%s", pchValue );
	else if (!Q_stricmp(pchRule, "gamedesc"))
		Q_snprintf( m_Server.m_szGameDescription, sizeof( m_Server.m_szGameDescription ), "%s", pchValue );

	InvalidateLayout();
	Repaint();
}


void CDialogGameInfo::RulesFailedToRespond()
{
	m_bServerNotResponding = true;
}


void CDialogGameInfo::RulesRefreshComplete()
{
	m_hRulesQuery = HSERVERQUERY_INVALID;
	m_bServerNotResponding = false;
}


void CDialogGameInfo::Close()
{
	m_bClosing = true;
	BaseClass::Close();
}


void CDialogGameInfo::FastClose()
{
	SetFadeEffectDisableOverride( true );
	OnClose();
}


void CDialogGameInfo::ShowModal( const char *szTitle )
{
	SetTitle( "#ServerBrowser_GameInfoWithNameTitle", true );
	SetDialogVariable( "game", szTitle );
	RequestInfo();
	Activate();
}


void CDialogGameInfo::ChangeAddress( int serverIP, int queryPort, unsigned short connectionPort )
{
	memset( &m_Server, 0x0, sizeof(m_Server) );

	m_Server.m_NetAdr.Init( serverIP, queryPort, connectionPort );

	// remember the dialogs position so we can keep it the same
	int x, y;
	GetPos( x, y );

	// see if we need to change dialog state
	if ( !m_Server.m_NetAdr.GetIP() || !m_Server.m_NetAdr.GetQueryPort() )
	{
		// not in a server, load the simple settings dialog
		SetMinimumSize(0, 0);
		SetSizeable( false );
		LoadControlSettings( "Servers/DialogGameInfo_SinglePlayer.res" );
	}
	else
	{
		// moving from a single-player game -> multiplayer, reset dialog
		SetMinimumSize(GetScaledValue(416), GetScaledValue(340));
		SetSizeable( true );
		LoadControlSettings( "Servers/DialogGameInfo.res" );
	}
	SetPos( x, y );

	// Start refresh immediately
	m_iRequestRetry = 0;
	RequestInfo();
	InvalidateLayout();
}


void CDialogGameInfo::OnPersonaStateChange( PersonaStateChange_t *pPersonaStateChange )
{
}


void CDialogGameInfo::SetFriend( uint64 ulSteamIDFriend )
{
	// set the title to include the friends name
	SetTitle( "#ServerBrowser_GameInfoWithNameTitle", true );
	SetDialogVariable( "game", GetSteamAPI()->SteamFriends()->GetFriendPersonaName( ulSteamIDFriend ) );
	SetDialogVariable( "friend", GetSteamAPI()->SteamFriends()->GetFriendPersonaName( ulSteamIDFriend ) );

	// store the friend we're associated with
	m_SteamIDFriend = ulSteamIDFriend;

	FriendGameInfo_t fgi;
	if ( GetSteamAPI()->SteamFriends()->GetFriendGamePlayed( ulSteamIDFriend, &fgi ) )
		ChangeAddress( fgi.m_unGameIP, fgi.m_usGamePort, fgi.m_usGamePort );
}


uint64 CDialogGameInfo::GetAssociatedFriend()
{
	return m_SteamIDFriend;
}


void CDialogGameInfo::PerformLayout()
{
	BaseClass::PerformLayout();

	SetControlString( "ServerText", m_Server.GetName() );
	SetControlString( "GameText", m_Server.m_szGameDescription );
	SetControlString( "MapText", m_Server.m_szMap );
	SetControlString( "GameTags", m_Server.m_szGameTags );


	if ( !m_Server.m_bHadSuccessfulResponse )
	{
		SetControlString("SecureText", "");
	}
	else if ( m_Server.m_bSecure )
	{
		SetControlString("SecureText", "#ServerBrowser_Secure");
	}
	else
	{
		SetControlString("SecureText", "#ServerBrowser_NotSecure");
	}

	char buf[128];
	if ( m_Server.m_nMaxPlayers > 0)
	{
		Q_snprintf(buf, sizeof(buf), "%d / %d", m_Server.m_nPlayers, m_Server.m_nMaxPlayers);
	}
	else
	{
		buf[0] = 0;
	}
	SetControlString("PlayersText", buf);

	if ( m_Server.m_NetAdr.GetIP() && m_Server.m_NetAdr.GetQueryPort() )
	{
		SetControlString("ServerIPText", m_Server.m_NetAdr.GetConnectionAddressString() );
		m_pConnectButton->SetEnabled(true);
		if ( m_pAutoRetry->IsSelected() )
		{
			m_pAutoRetryAlert->SetVisible(true);
			m_pAutoRetryJoin->SetVisible(true);
		}
		else
		{
			m_pAutoRetryAlert->SetVisible(false);
			m_pAutoRetryJoin->SetVisible(false);
		}
	}
	else
	{
		SetControlString("ServerIPText", "");
		m_pConnectButton->SetEnabled(false);
	}

	if ( m_Server.m_bHadSuccessfulResponse )
	{
		Q_snprintf(buf, sizeof(buf), "%d", m_Server.m_nPing );
		SetControlString("PingText", buf);
	}
	else
	{
		SetControlString("PingText", "");
	}

	// set the info text
	if ( m_pAutoRetry->IsSelected() )
	{
		if ( m_Server.m_nPlayers < m_Server.m_nMaxPlayers )
		{
			m_pInfoLabel->SetText("#ServerBrowser_PressJoinToConnect");
		}
		else if (m_pAutoRetryJoin->IsSelected())
		{
			m_pInfoLabel->SetText("#ServerBrowser_JoinWhenSlotIsFree");
		}
		else
		{
			m_pInfoLabel->SetText("#ServerBrowser_AlertWhenSlotIsFree");
		}
	}
	else if (m_bServerFull)
	{
		m_pInfoLabel->SetText("#ServerBrowser_CouldNotConnectServerFull");
	}
	else if (m_bServerNotResponding)
	{
		m_pInfoLabel->SetText("#ServerBrowser_ServerNotResponding");
	}
	else
	{
		// clear the status
		m_pInfoLabel->SetText("");
	}

	if ( m_Server.m_bHadSuccessfulResponse && !(m_Server.m_nPlayers + m_Server.m_nBotPlayers) )
	{
		m_pPlayerList->SetEmptyListText("#ServerBrowser_ServerHasNoPlayers");
	}
	else
	{
		m_pPlayerList->SetEmptyListText("#ServerBrowser_ServerNotResponding");
	}

	// auto-retry layout
	m_pAutoRetry->SetVisible(m_bShowAutoRetryToggle);

	Repaint();
}


void CDialogGameInfo::Connect()
{
	OnConnect();
}


void CDialogGameInfo::OnConnect()
{
	// flag that we are attempting connection
	m_bConnecting = true;

	// reset state
	m_bServerFull = false;
	m_bServerNotResponding = false;

	InvalidateLayout();

	// need to refresh server before attempting to connect, to make sure there is enough room on the server
	m_iRequestRetry = 0;
	RequestInfo();
}


void CDialogGameInfo::OnConnectToGame( int ip, int port )
{
	// if we just connected to the server we were looking at, close the dialog
	// important so that we don't auto-retry a server that we are already on
	if ( m_Server.m_NetAdr.GetIP() == (uint32)ip && m_Server.m_NetAdr.GetConnectionPort() == (uint16)port )
	{
		// close this dialog
		FastClose();
	}
}


void CDialogGameInfo::OnRefresh()
{
	m_iRequestRetry = 0;
	// re-ask the server for the game info
	RequestInfo();
}


void CDialogGameInfo::OnButtonToggled(Panel *panel)
{
	if (panel == m_pAutoRetry)
	{
		ShowAutoRetryOptions(m_pAutoRetry->IsSelected());
	}

	InvalidateLayout();
}


void CDialogGameInfo::ShowAutoRetryOptions(bool state)
{
	// we need to extend the dialog
	int growSize = 60;
	if (!state)
	{
		growSize = -growSize;
	}

	// alter the dialog size accordingly
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// load a new layout file depending on the state
	SetMinimumSize(GetScaledValue(416), GetScaledValue(340));
	if ( state )
		LoadControlSettings( "Servers/DialogGameInfo_AutoRetry.res" );
	else
		LoadControlSettings( "Servers/DialogGameInfo.res" );

	// restore size and position as 
	// load control settings will override them
	SetBounds( x, y, wide, tall + growSize );

	// restore other properties of the dialog
	PerformLayout();

	m_pAutoRetryAlert->SetSelected( true );
	
	InvalidateLayout();
}


void CDialogGameInfo::RequestInfo()
{
	if ( !GetSteamAPI()->SteamMatchmakingServers() )
		return;

	if ( m_iRequestRetry == 0 )
	{
		// reset the time at which we auto-refresh
		m_iRequestRetry = vgui2::system()->GetTimeMillis() + RETRY_TIME;
		if ( m_hPingQuery != HSERVERQUERY_INVALID )
			GetSteamAPI()->SteamMatchmakingServers()->CancelServerQuery( m_hPingQuery );
		m_hPingQuery = GetSteamAPI()->SteamMatchmakingServers()->PingServer( m_Server.m_NetAdr.GetIP(), m_Server.m_NetAdr.GetQueryPort(), this );
	}
}


void CDialogGameInfo::OnTick()
{
	// check to see if we should perform an auto-refresh
	if ( m_iRequestRetry && m_iRequestRetry < vgui2::system()->GetTimeMillis() )
	{
		m_iRequestRetry = 0;
		RequestInfo();
	}

	// If we have a password dialog, we won't move this to the front.
	if ( m_pPasswordDialog ) return;
	MoveToFront();
}


void CDialogGameInfo::ServerResponded( gameserveritem_t &server )
{
	if ( m_Server.m_NetAdr.GetConnectionPort() && m_Server.m_NetAdr.GetConnectionPort() != server.m_NetAdr.GetConnectionPort() )
		return; // this is not the guy we talked about

	m_hPingQuery = HSERVERQUERY_INVALID;
	m_Server = server;

	if ( m_bConnecting )
	{
		ConnectToServer();
	}
	else if ( m_pAutoRetry->IsSelected() && server.m_nPlayers < server.m_nMaxPlayers )
	{
		// flash this window
		FlashWindow();

		// if it's set, connect right away
		if (m_pAutoRetryJoin->IsSelected())
			ConnectToServer();
	}
	else
		SendPlayerQuery( server.m_NetAdr.GetIP(), server.m_NetAdr.GetQueryPort() );

	m_bServerNotResponding = false;

	InvalidateLayout();
	Repaint();
}


void CDialogGameInfo::ServerFailedToRespond()
{
	// the server didn't respond, mark that in the UI
	// only mark if we haven't ever received a response
	if ( !m_Server.m_bHadSuccessfulResponse )
	{
		m_bServerNotResponding = true;
	}

	InvalidateLayout();
	Repaint();
}


void CDialogGameInfo::ApplyConnectCommand( const gameserveritem_t &server )
{
	char command[ 256 ];
	// set the server password, if any
	if ( m_szPassword[0] )
	{
		Q_snprintf( command, Q_ARRAYSIZE( command ), "password \"%s\"\n", m_szPassword );
		EngineClientCmd( command );
	}
	// send engine command to change servers
	Q_snprintf( command, Q_ARRAYSIZE( command ), "wait;wait;wait;wait;connect %s\n", server.m_NetAdr.GetConnectionAddressString() );
	EngineClientCmd( command );
	FastClose();
	CGameUIViewport::Get()->GetServerBrowser()->Close();
}


void CDialogGameInfo::ConnectToServer()
{
	m_bConnecting = false;

	// check VAC status
	if ( m_Server.m_bSecure && CGameUIViewport::Get()->IsVACBanned() )
	{
		// refuse the user
		CVACBannedDialog *pDlg = new CVACBannedDialog( CGameUIViewport::Get() );
		pDlg->DoModal();
		FastClose();
		return;
	}

	if ( m_pPasswordDialog )
	{
		m_pPasswordDialog->MoveToFront();
		return;
	}

	// check to see if we need a password
	if ( m_Server.m_bPassword && !m_szPassword[0] )
	{
		CDialogServerPassword *box = new CDialogServerPassword( this );
		m_pPasswordDialog = box;
		box->AddActionSignalTarget( this );
		box->Activate( m_Server.GetName() );
		return;
	}

	// check the player count
	if ( m_Server.m_nPlayers >= m_Server.m_nMaxPlayers )
	{
		// mark why we cannot connect
		m_bServerFull = true;
		// give them access to auto-retry options
		m_bShowAutoRetryToggle = true;
		InvalidateLayout();
		return;
	}

	ApplyConnectCommand( m_Server );

	// close this dialog
	PostMessage(this, new KeyValues("Close"));
}


void CDialogGameInfo::RefreshComplete( EMatchMakingServerResponse response )
{
}


void CDialogGameInfo::OnJoinServerWithPassword(const char *password)
{
	m_pPasswordDialog = nullptr;

	// copy out the password
	Q_strncpy(m_szPassword, password, sizeof(m_szPassword));

	// retry connecting to the server again
	OnConnect();
}


void CDialogGameInfo::OnPasswordDialogClosed()
{
	m_pPasswordDialog = nullptr;
}


void CDialogGameInfo::ClearPlayerList()
{
	m_pPlayerList->DeleteAllItems();
	Repaint();
}


void CDialogGameInfo::AddPlayerToList(const char *playerName, int score, float timePlayedSeconds)
{
	if ( m_bPlayerListUpdatePending )
	{
		m_bPlayerListUpdatePending = false;
		m_pPlayerList->RemoveAll();
	}

	KeyValues *player = new KeyValues("player");
	player->SetString("PlayerName", playerName);
	player->SetInt("Score", score);
	player->SetInt("TimeSec", (int)timePlayedSeconds);
	
	// construct a time string
	int seconds = (int)timePlayedSeconds;
	int minutes = seconds / 60;
	int hours = minutes / 60;
	seconds %= 60;
	minutes %= 60;
	char buf[64];
	buf[0] = 0;
	if (hours)
	{
		Q_snprintf(buf, sizeof(buf), "%dh %dm %ds", hours, minutes, seconds);	
	}
	else if (minutes)
	{
		Q_snprintf(buf, sizeof(buf), "%dm %ds", minutes, seconds);	
	}
	else
	{
		Q_snprintf(buf, sizeof(buf), "%ds", seconds);	
	}
	player->SetString("Time", buf);
	
	m_pPlayerList->AddItem(player, 0, false, true);
	player->deleteThis();
}


int CDialogGameInfo::PlayerTimeColumnSortFunc(vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2)
{
	int p1time = p1.kv->GetInt("TimeSec");
	int p2time = p2.kv->GetInt("TimeSec");

	if (p1time > p2time)
		return -1;
	if (p1time < p2time)
		return 1;

	return 0;
}
