// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include "hud.h"
#include "parsemsg.h"
#include "cl_util.h"
#include "cl_voice_status.h"
#include "zp_crosshair.h"
#include "vgui/client_viewport.h"
#include "hud/ammo.h"
#include "hud/speedometer.h"

#include "event_api.h"

ConVar hud_dyncrosshair_static( "hud_dyncrosshair_static", "0", FCVAR_BHL_ARCHIVE );
ConVar hud_dyncrosshair_length( "hud_dyncrosshair_length", "5", FCVAR_BHL_ARCHIVE );
ConVar hud_dyncrosshair_span( "hud_dyncrosshair_span", "5", FCVAR_BHL_ARCHIVE );
ConVar hud_dyncrosshair_size("hud_dyncrosshair_size", "2", FCVAR_BHL_ARCHIVE);

DEFINE_HUD_ELEM( CHudZPCrossHair );

CHudZPCrossHair::CHudZPCrossHair()
    : vgui2::Panel(NULL, "CHudZPCrossHair")
{
	SetParent( g_pViewport );
	SetScheme( "ClientScheme" );

	m_FirstDynUpdate = false;
	m_mCBlend = m_mCBlendTime = 0;
}

void CHudZPCrossHair::Init()
{
	BaseHudClass::Init();

	// For our label & icon panels to be drawn correctly, we need to be the size of the screen
	int w, t;
	vgui2::surface()->GetScreenSize( w, t );
	SetWide( w );
	SetTall( t );
}

void CHudZPCrossHair::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor( Color(0, 0, 0, 0) );
}

bool CHudZPCrossHair::IsAllowedToDraw()
{
	if ( gHUD.m_RoundState < ZP::RoundState::RoundState_RoundHasBegunPost ) return false;
	if ( g_pViewport->IsVGUIVisible( MENU_TEAM ) ) return false;
	if ( g_pViewport->IsVGUIVisible( MENU_MOTD ) ) return false;
	if ( gEngfuncs.GetLocalPlayer()->index <= 0 ) return false;
	CPlayerInfo *localplayer = GetPlayerInfo( gEngfuncs.GetLocalPlayer()->index );
	if ( !localplayer->IsConnected() ) return false;
	if ( localplayer->GetTeamNumber() == ZP::TEAM_ZOMBIE ) return false;
	return hud_draw.GetBool();
}

void CHudZPCrossHair::Paint()
{
	if ( !IsAllowedToDraw() ) return;
	// Draw our crosshair
	DrawDynCrosshair();
}

void CHudZPCrossHair::DrawDynCrosshair()
{
	// If in zoom, don't draw
	if ( gHUD.m_iFOV < 90 ) return;

	// Our bounds
	int boundX, boundY, w, h;
	GetBounds( boundX, boundY, w, h );

	int a = 255;
	int r, g, b, x, y;
	r = g = b = x = y = 0;
	g = 255;
	int screen_wide = w;
	int screen_tall = h;
	int wide_half = (screen_wide / 2);
	int tall_half = (screen_tall / 2);
	int size = hud_dyncrosshair_size.GetInt();
	DynamicCrosshairTarget baseTarget = GetCurrentWeapon();

	// Is the player ducking?
	bool bIsDucking = ( gEngfuncs.pEventAPI->EV_LocalPlayerDucking() > 0 );
	int playerspeed = CHudSpeedometer::Get()->GetSpeed();
	float nSpannerIncreaseBy = (playerspeed / 25);
	int iExtraSpan = static_cast<int>( nSpannerIncreaseBy );

	for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
	{
		if ( baseTarget.spanner[i] == 0 ) continue;
		if ( bIsDucking && baseTarget.spanner[i] > 4 )
			baseTarget.spanner[i] = 4;
		if ( iExtraSpan > 0 )
			baseTarget.spanner[i] += iExtraSpan;
	}

	DynamicCrosshairTarget dyndest;

	// If static, then don't do any dynamic stuff
	bool bDynamic = hud_dyncrosshair_static.GetInt() <= 0 ? true : false;
	if ( bDynamic )
	{
		DynamicCrosshairTarget target = baseTarget;
		for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
		{
			if ( target.spanner[i] > 0 )
				target.spanner[i] += m_DynamicCrosshairTarget.spanner[i];
			if ( target.length[i] > 0 )
				target.length[i] += m_DynamicCrosshairTarget.length[i];
		}
		dyndest = BlendTo( target, baseTarget, m_mCBlend, m_mCBlendTime );
	}
	else
	{
		for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
		{
			dyndest.spanner[i] = hud_dyncrosshair_span.GetInt();
			dyndest.length[i] = hud_dyncrosshair_length.GetInt();
		}
	}

	// DEBUG
	//gEngfuncs.Con_NPrintf(11, "\t == DynamicCrosshairTarget == \n");
	//gEngfuncs.Con_NPrintf(12, "\ttarget.spanner >> [%i]\n", dyndest.spanner);
	//gEngfuncs.Con_NPrintf(13, "\ttarget.length >> [%i]\n", dyndest.length);

	bool bDrawCross = true;
	bool bDrawCircle = false;

	switch ( dyndest.type )
	{
		case DynamicCrossHairTypes_t::k_eCrossHairCircleOnly:
		{
		    bDrawCross = false;
		    bDrawCircle = true;
		}
		break;

		case DynamicCrossHairTypes_t::k_eCrossHairWithCircle: bDrawCircle = true; break;
	}

	if ( bDrawCross )
	{
		// Left
		y = tall_half - (size / 2);
		x = wide_half - dyndest.spanner[DynamicCrossHairAxis_t::k_eDynLeft];
		FillRGBA( x, y, -dyndest.length[DynamicCrossHairAxis_t::k_eDynLeft], size, r, g, b, a );

		// Right
		x = wide_half + dyndest.spanner[DynamicCrossHairAxis_t::k_eDynRight];
		FillRGBA( x, y, dyndest.length[DynamicCrossHairAxis_t::k_eDynRight], size, r, g, b, a );

		// Top
		y = tall_half - dyndest.spanner[DynamicCrossHairAxis_t::k_eDynTop];
		x = wide_half - (size / 2);
		FillRGBA( x, y, size, -dyndest.length[DynamicCrossHairAxis_t::k_eDynTop], r, g, b, a );

		// Bottom
		y = tall_half + dyndest.spanner[DynamicCrossHairAxis_t::k_eDynBottom];
		FillRGBA( x, y, size, dyndest.length[DynamicCrossHairAxis_t::k_eDynBottom], r, g, b, a );
	}

	if ( bDrawCircle )
	{
		// TODO: Draw a circle here
	}
}

void CHudZPCrossHair::UpdateDynamicCrosshair( int spanner, int length, float dyntime )
{
	if ( !m_FirstDynUpdate )
	{
		for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
		{
			m_DynamicCrosshairTarget.spanner[i] = spanner;
			m_DynamicCrosshairTarget.length[i] = length;
		}
		m_FirstDynUpdate = true;
	}
	else
	{
		for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
		{
			if ( m_DynamicCrosshairTarget.spanner[i] > 0 )
				m_DynamicCrosshairTarget.spanner[i] += spanner;
			if ( m_DynamicCrosshairTarget.length[i] > 0 )
				m_DynamicCrosshairTarget.length[i] += length;
		}
	}
	for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
	{
		m_DynamicCrosshairTarget.spanner[i] = clamp( m_DynamicCrosshairTarget.spanner[i], 0, DYNAMIC_UPDATE_MAX_SPAN );
		m_DynamicCrosshairTarget.length[i] = clamp( m_DynamicCrosshairTarget.length[i], 0, DYNAMIC_UPDATE_MAX_LENGTH );
	}
	m_mCBlend = gHUD.m_flTime;
	m_mCBlendTime = dyntime;
}

DynamicCrosshairTarget CHudZPCrossHair::GetCurrentWeapon() const
{
	DynamicCrosshairTarget dynCrosshair;

	WEAPON *pActive = CHudAmmo::Get()->GetActiveWeapon();
	// If not active, return default settings.
	if ( !pActive )
		return dynCrosshair;
	WeaponData wpnDat = GetWeaponSlotInfo( pActive->szName );
	return wpnDat.DynCrossHair;
}

DynamicCrosshairTarget CHudZPCrossHair::BlendTo( const DynamicCrosshairTarget& blend_from, const DynamicCrosshairTarget& blend_to, float &flLastTimeCheck, float blend_time )
{
	// Don't update if this is invalid
	if ( !flLastTimeCheck && !blend_time )
	{
		m_FirstDynUpdate = false;
		return blend_to;
	}

	// Clear if blend is over
	if ( (flLastTimeCheck + blend_time) < gHUD.m_flTime )
	{
		flLastTimeCheck = 0;
		m_FirstDynUpdate = false;
		return blend_to;
	}

	// Smooth it out
	float interp = (gHUD.m_flTime - flLastTimeCheck) / blend_time;
	DynamicCrosshairTarget vOut;
	BlendScale( blend_from, (1.0 - interp), vOut );
	BlendMA( vOut, interp, blend_to, vOut );
	vOut.type = blend_to.type;
	return vOut;
}

void CHudZPCrossHair::BlendMA( const DynamicCrosshairTarget& start, float scale, const DynamicCrosshairTarget& direction, DynamicCrosshairTarget& dest )
{
	for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
	{
		dest.length[i] = start.length[i] + direction.length[i] * scale;
		dest.spanner[i] = start.spanner[i] + direction.spanner[i] * scale;
	}
}

void CHudZPCrossHair::BlendScale( const DynamicCrosshairTarget& in, float scale, DynamicCrosshairTarget& result )
{
	for ( size_t i = 0; i < DynamicCrossHairAxis_t::k_eDynMax; i++ )
	{
		result.length[i] = in.length[i] * scale;
		result.spanner[i] = in.spanner[i] * scale;
	}
}