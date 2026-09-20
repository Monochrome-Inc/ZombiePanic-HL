// Ported from Source, but modified for GoldSrc

#ifndef SERVER_BROWSER_DIALOG_GAMEINFO_H
#define SERVER_BROWSER_DIALOG_GAMEINFO_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/RadioButton.h>
#include "steam/steam_api.h"
#include "tier1/netadr.h"

struct challenge_s
{
	servernetadr_t addr;
	int challenge;
};

class CDialogGameInfo : public vgui2::Frame,
	public ISteamMatchmakingPlayersResponse,
	public ISteamMatchmakingPingResponse,
	public ISteamMatchmakingRulesResponse
{
	DECLARE_CLASS_SIMPLE( CDialogGameInfo, vgui2::Frame ); 

public:
	CDialogGameInfo( vgui2::Panel *pParent, int nIP, int iPort, unsigned short unPort );
	~CDialogGameInfo();

	void ShowModal( const char *szTitle );

	void ChangeAddress( int nIP, int iPort, unsigned short unPort );
	void SetFriend( uint64 ulSteamIDFriend );
	uint64 GetAssociatedFriend();

	// forces the dialog to attempt to connect to the server
	void Connect();

	// implementation of IServerRefreshResponse interface
	// called when the server has successfully responded
	virtual void ServerResponded( gameserveritem_t &server );

	// called when a server response has timed out
	virtual void ServerFailedToRespond();

	// on individual player added
	virtual void AddPlayerToList( const char *szPlayerName, int iScore, float flPlayTime );
	virtual void PlayersFailedToRespond() {}
	virtual void PlayersRefreshComplete() { m_hPlayersQuery = HSERVERQUERY_INVALID; }

	// called when the current refresh list is complete
	virtual void RefreshComplete( EMatchMakingServerResponse nServerResponse );

	// player list received
	virtual void ClearPlayerList();
	virtual void SendPlayerQuery( uint32 unIP, uint16 usQueryPort );

	// on getting server rules
	virtual void RulesResponded( const char *pchRule, const char *pchValue );
	virtual void RulesFailedToRespond();
	virtual void RulesRefreshComplete();

	bool IsAlreadyClosing() const { return m_bClosing; }
	void Close() override;
	void FastClose();

protected:
	// message handlers
	MESSAGE_FUNC( OnConnect, "Connect" );
	MESSAGE_FUNC( OnRefresh, "Refresh" );
	MESSAGE_FUNC_PTR( OnButtonToggled, "ButtonToggled", panel );
	MESSAGE_FUNC_PTR( OnRadioButtonChecked, "RadioButtonChecked", panel )
	{
		OnButtonToggled( panel );
	}

	// response from the get password dialog
	MESSAGE_FUNC( OnPasswordDialogClosed, "PasswordDialogClosed" );
	MESSAGE_FUNC_CHARPTR( OnJoinServerWithPassword, "JoinServerWithPassword", password );
	MESSAGE_FUNC_INT_INT( OnConnectToGame, "ConnectedToGame", ip, port );

	// vgui overrides
	virtual void OnTick();
	virtual void PerformLayout();

private:
	STEAM_CALLBACK( CDialogGameInfo, OnPersonaStateChange, PersonaStateChange_t, m_CallbackPersonaStateChange );

	long m_iRequestRetry;	// time at which to retry the request
	static int PlayerTimeColumnSortFunc( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );

	// methods
	void RequestInfo();
	void ConnectToServer();
	void ShowAutoRetryOptions( bool bState );
	void ApplyConnectCommand( const gameserveritem_t &server );

	vgui2::Button *m_pConnectButton;
	vgui2::Button *m_pCloseButton;
	vgui2::Button *m_pRefreshButton;
	vgui2::Label *m_pInfoLabel;
	vgui2::ToggleButton *m_pAutoRetry;
	vgui2::RadioButton *m_pAutoRetryAlert;
	vgui2::RadioButton *m_pAutoRetryJoin;
	vgui2::ListPanel *m_pPlayerList;
	vgui2::Frame *m_pPasswordDialog;

	enum { PING_TIMES_MAX = 4 };

	// true if we should try connect to the server when it refreshes
	bool m_bConnecting;

	// password, if entered
	char m_szPassword[64];

	// state
	bool m_bServerNotResponding;
	bool m_bServerFull;
	bool m_bShowAutoRetryToggle;
	bool m_bShowingExtendedOptions;
	uint64 m_SteamIDFriend;

	gameserveritem_t m_Server;
	HServerQuery m_hPingQuery;
	HServerQuery m_hPlayersQuery;
	HServerQuery m_hRulesQuery;
	bool m_bPlayerListUpdatePending;
	bool m_bClosing;
};

#endif