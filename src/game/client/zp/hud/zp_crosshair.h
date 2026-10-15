// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef HUD_ZP_CROSSHAIR_H
#define HUD_ZP_CROSSHAIR_H
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include "../../hud/base.h"
#include "zp/zp_shared.h"

#define DYNAMIC_UPDATE_MAX_SPAN   20
#define DYNAMIC_UPDATE_MAX_LENGTH 15

class CHudZPCrossHair : public CHudElemBase<CHudZPCrossHair>, public vgui2::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudZPCrossHair, vgui2::Panel );

	CHudZPCrossHair();

	void Init();
	virtual bool IsAllowedToDraw();
	virtual void Paint();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	// Grab the current weapon crosshair data
	DynamicCrosshairTarget GetCurrentWeapon() const;

	DynamicCrosshairTarget BlendTo( const DynamicCrosshairTarget &blend_from, const DynamicCrosshairTarget &blend_to, float &flLastTimeCheck, float blend_time );
	void BlendMA( const DynamicCrosshairTarget &start, float scale, const DynamicCrosshairTarget &direction, DynamicCrosshairTarget &dest );
	void BlendScale( const DynamicCrosshairTarget& in, float scale, DynamicCrosshairTarget& result );
	void UpdateDynamicCrosshair( int spanner, int length, float dyntime );

	void DrawDynCrosshair();

private:
	DynamicCrosshairTarget m_DynamicCrosshairTarget;

	bool m_FirstDynUpdate;

	float m_mCBlend;
	float m_mCBlendTime;
};

#endif
