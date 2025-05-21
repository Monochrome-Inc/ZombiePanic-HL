#include <vgui_controls/Label.h>
#include "client_vgui.h"
#include "cvar_color.h"
#include "options_hud_colors.h"

CHudSubOptionsColors::CHudSubOptionsColors(vgui2::Panel *parent)
    : BaseClass(parent, "HudSubOptionsColors")
{
	SetSize(100, 100); // Silence "parent not sized yet" warning

	m_pColorLabel[0] = new vgui2::Label(this, "ColorLabel", "#ZP_AdvOptions_HUD_Color");
	m_pColorValue[0] = new CCvarColor(this, "ColorValue", "hud_color", "#ZP_AdvOptions_HUD_Color_Title");

	m_pColorLabel[1] = new vgui2::Label(this, "Color1Label", "#ZP_AdvOptions_HUD_Color1");
	m_pColorValue[1] = new CCvarColor(this, "Color1Value", "hud_color1", "#ZP_AdvOptions_HUD_Color1_Title");

	m_pColorLabel[2] = new vgui2::Label(this, "Color2Label", "#ZP_AdvOptions_HUD_Color2");
	m_pColorValue[2] = new CCvarColor(this, "Color2Value", "hud_color2", "#ZP_AdvOptions_HUD_Color2_Title");

	m_pColorLabel[3] = new vgui2::Label(this, "Color3Label", "#ZP_AdvOptions_HUD_Color3");
	m_pColorValue[3] = new CCvarColor(this, "Color3Value", "hud_color3", "#ZP_AdvOptions_HUD_Color3_Title");

	LoadControlSettings(VGUI2_ROOT_DIR "resource/options/HudSubOptionsColors.res");
}

void CHudSubOptionsColors::OnResetData()
{
	m_pColorValue[0]->ResetData();
	m_pColorValue[1]->ResetData();
	m_pColorValue[2]->ResetData();
	m_pColorValue[3]->ResetData();
}

void CHudSubOptionsColors::OnApplyChanges()
{
	m_pColorValue[0]->ApplyChanges();
	m_pColorValue[1]->ApplyChanges();
	m_pColorValue[2]->ApplyChanges();
	m_pColorValue[3]->ApplyChanges();
}
