// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SERVER_BROWSER_DIALOG_SERVERPASSWORD_H
#define SERVER_BROWSER_DIALOG_SERVERPASSWORD_H

#include <vgui_controls/Frame.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>

class CDialogGameInfo;

class CDialogServerPassword : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE( CDialogServerPassword, vgui2::Frame ); 

public:
	CDialogServerPassword( vgui2::Panel *pParent );
	void Activate( const char *pszServerName );

protected:
	virtual void OnCommand( const char *szCommand );

private:
	vgui2::Label *m_pInfoLabel;
	vgui2::Label *m_pGameLabel;
	vgui2::TextEntry *m_pPasswordEntry;
	vgui2::Button *m_pConnectButton;
};

#endif