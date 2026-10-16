// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef MUSIC_UI_H
#define MUSIC_UI_H
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include "../../hud/base.h"

class CMusicUI : public CHudElemBase<CMusicUI>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CMusicUI, vgui2::Panel );

	CMusicUI();

	void Init() override;
	void VidInit() override;

	virtual bool IsAllowedToDraw();
	virtual void PaintBackground() {}
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	void NewTrackPlaying();
	void HideUI();
	void DrawUI();

private:

	vgui2::Label			*m_pTextTitle;
	vgui2::Label			*m_pTextTitleBG1;
	vgui2::Label			*m_pTextTitleBG2;
	float					m_flDrawTime;
	Color					m_bgColor;
};


#endif