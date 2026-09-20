// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "CDialogServerPassword.h"
#include <tier1/KeyValues.h>

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogServerPassword::CDialogServerPassword(vgui2::Panel *parent) : vgui2::Frame( parent, "DialogServerPassword" )
{
	SetProportional( true );
	SetSize( 320, 240 );
	SetDeleteSelfOnClose( true );
	SetSizeable( false );

	m_pInfoLabel = new vgui2::Label( this, "InfoLabel", "#ServerBrowser_ServerRequiresPassword" );
	m_pGameLabel = new vgui2::Label( this, "GameLabel", "<game label>" );
	m_pPasswordEntry = new vgui2::TextEntry( this, "PasswordEntry" );
	m_pConnectButton = new vgui2::Button( this, "ConnectButton", "#ServerBrowser_Connect" );
	m_pPasswordEntry->SetTextHidden( true );

	LoadControlSettings( "Servers/DialogServerPassword.res" );

	SetTitle( "#ServerBrowser_ServerRequiresPasswordTitle", true );

	// set our initial position in the middle of the workspace
	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Activates the dialog and sets the server name
//-----------------------------------------------------------------------------
void CDialogServerPassword::Activate( const char *serverName )
{
	m_pGameLabel->SetText( serverName );
	m_pConnectButton->SetAsDefaultButton( true );
	m_pPasswordEntry->RequestFocus();
	BaseClass::Activate();
	MoveToFront();
}

//-----------------------------------------------------------------------------
// Purpose: Handles command events from the dialog controls
// Input  : *command - The command string
//-----------------------------------------------------------------------------
void CDialogServerPassword::OnCommand( const char *command )
{
	bool bClose = false;

	if ( !Q_stricmp( command, "Connect" ) )
	{
		KeyValues *msg = new KeyValues( "JoinServerWithPassword" );
		char buf[64];
		m_pPasswordEntry->GetText( buf, sizeof(buf)-1);
		msg->SetString( "password", buf );
		PostActionSignal( msg );
		bClose = true;
	}
	else if ( !Q_stricmp( command, "Close" ) )
	{
		KeyValues *msg = new KeyValues( "PasswordDialogClosed" );
		PostActionSignal( msg );
		bClose = true;
	}
	else
		BaseClass::OnCommand( command );

	if ( bClose )
		PostMessage( this, new KeyValues( "Close" ) );
}
