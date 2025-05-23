// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef HUD_ROUNDSTATE_H
#define HUD_ROUNDSTATE_H
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include "../../hud/base.h"
#include "zp/zp_shared.h"

class CHudRoundState : public CHudElemBase<CHudRoundState>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE(CHudRoundState, vgui2::Panel);

	CHudRoundState();

	virtual bool IsAllowedToDraw();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	int MsgFunc_RoundState(const char *pszName, int iSize, void *pbuf);

private:
	CPanelAnimationVarAliasType( int, m_iWaitingForPlayersYPos, "WaitingForPlayer_yPos", "100", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iWaitingForPlayersTall, "WaitingForPlayer_Tall", "30", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iTextWaitingForPlayersTall, "WaitingForPlayer_TextTall", "20", "proportional_int" );
	CPanelAnimationStringVar( 32, m_szWaitingForPlayersFont, "WaitingForPlayers_Font", "Default" );

	vgui2::Label *m_pText;
};

#endif
