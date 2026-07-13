/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"

#include <string.h>
#include <stdio.h>

#include <vgui/ISurface.h>

#include "ammo.h"
#include "ammohistory.h"
#include "crosshair.h"
#include "menu.h"
#include "vgui/client_viewport.h"
#include "zp/zp_shared.h"

ConVar hud_fastswitch("hud_fastswitch", "0", FCVAR_ARCHIVE, "Controls whether or not weapons can be selected in one keypress");
ConVar hud_weaponslot_corner_hug("hud_weaponslot_corner_hug", "0", FCVAR_BHL_ARCHIVE, "Hug the weapon slots to the left of the screen");
extern ConVar cl_cross_zoom;
extern bool HasRoundBegun();

WEAPON *gpActiveSel; // NULL means off, 1 means just the menu bar, otherwise
    // this points to the active weapon menu item
WEAPON *gpLastSel; // Last weapon menu selection

client_sprite_t *GetSpriteFromList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

WeaponsResource gWR;

// Used by zp_ammobank.cpp
int g_ActiveAmmoIndex = 0;

int g_weaponselect = 0;

void HudAmmo_ClearWeaponSelectionInputState()
{
	gpActiveSel = NULL;
	gpLastSel = NULL;
	g_weaponselect = 0;
	gHUD.m_iKeyBits &= ~IN_ATTACK;
}

#define SET_WEAPON_SELECTION_XPOS 180
#define SET_WEAPON_SELECTION_YPOS 20

// Debugging for weapon slots
#define DEBUG_WEAPON_SLOTS 0

void WeaponsResource::LoadAllWeaponSprites(void)
{
	for (int i = 0; i < LAST_WEAPON_ID; i++)
	{
		if (rgWeapons[i].iId)
			LoadWeaponSprites(&rgWeapons[i]);
	}
}

int WeaponsResource::CountAmmo(int iId)
{
	if (iId < 0)
		return 0;

	return riAmmo[iId];
}

int WeaponsResource::HasAmmo(WEAPON *p)
{
	if (!p)
		return FALSE;

	// We always return true if the weapon is valid.
	// Because we want to drop the weapon if we have no ammo etc.
	return TRUE;
}

void WeaponsResource::UpdateWeaponData( int iID, WeaponData nData )
{
	if ( iID < WEAPON_NONE || iID >= LAST_WEAPON_ID ) return;
	V_strcpy_safe( rgWeapons[iID].szName, GetWeaponInfo( (ZPWeaponID)iID ).szWeapon );
	rgWeapons[iID].iAmmoType = GetAmmoByName( nData.Ammo1 ).AmmoType;
	rgWeapons[iID].iAmmo2Type = GetAmmoByName( nData.Ammo2 ).AmmoType;
	rgWeapons[iID].iMax1 = GetAmmoByName( nData.Ammo1 ).MaxCarry;
	rgWeapons[iID].iMax2 = GetAmmoByName( nData.Ammo2 ).MaxCarry;
	rgWeapons[iID].iFlags = nData.Flags;
	rgWeapons[iID].iMaxClip = nData.MaxClip;
	rgWeapons[iID].iClip = nData.MaxClip;
	rgWeapons[iID].iWeight = (int)nData.Weight;
	rgWeapons[iID].bDoubleSlot = nData.DoubleSlots;
	// Make sure we load the sprites
	LoadWeaponSprites(&rgWeapons[iID]);
}

void WeaponsResource::UpdatedWeaponSlotPos( int iID, int iNewSlotPos )
{
	if ( iID < WEAPON_NONE || iID >= LAST_WEAPON_ID ) return;
	rgWeapons[iID].iSlot = iNewSlotPos;
}

void WeaponsResource::UpdateWeaponVisibility( int iID, bool bState )
{
	if ( iID < WEAPON_NONE || iID >= LAST_WEAPON_ID ) return;
	rgWeapons[iID].bHasWeapon = bState;
}

void WeaponsResource::UpdateWeaponClip(int iID, int iClip)
{
	if (iID < WEAPON_NONE || iID >= LAST_WEAPON_ID) return;
	rgWeapons[iID].iClip = iClip;
}

void WeaponsResource::LoadWeaponSprites(WEAPON *pWeapon)
{
	int i = 0;
	int iRes = gHUD.m_iRes;
	char sz[256];
	if ( !pWeapon ) return;
	if ( pWeapon->szName[0] == 0 ) return;

	WeaponData wpnDat = GetWeaponSlotInfo( pWeapon->szName );
	pWeapon->hInactive = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_WEAPON] );
	pWeapon->hActive = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_WEAPON_SELECTED] );
	pWeapon->hAmmo = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_AMMO1] );
	pWeapon->hAmmo2 = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_AMMO2] );
	pWeapon->hCrosshair = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_CROSSHAIR] );
	pWeapon->hAutoaim = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_CROSSHAIR_AUTO] );
	pWeapon->hZoomedCrosshair = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_CROSSHAIR_ZOOM] );
	pWeapon->hZoomedAutoaim = gHUD.GetRegisteredIcon( wpnDat.Icons[ICON_CROSSHAIR_ZOOMAUTO] );
}

int giBucketHeight, giBucketWidth, giABHeight, giABWidth; // Ammo Bar width and height

HSPRITE ghsprBuckets; // Sprite for top row of weapons menu

// width of ammo fonts
#define AMMO_SMALL_WIDTH 10
#define AMMO_LARGE_WIDTH 20

#define HISTORY_DRAW_TIME "5"

DEFINE_HUD_ELEM(CHudAmmo);

void CHudAmmo::Init()
{
	BaseHudClass::Init();

	HookMessage<&CHudAmmo::MsgFunc_CurWeapon>("CurWeapon"); // Current weapon and clip
	HookMessage<&CHudAmmo::MsgFunc_WeaponList>("WeaponList"); // new weapon type
	HookMessage<&CHudAmmo::MsgFunc_AmmoPickup>("AmmoPickup"); // flashes an ammo pickup record
	HookMessage<&CHudAmmo::MsgFunc_WeapPickup>("WeapPickup"); // flashes a weapon pickup record
	HookMessage<&CHudAmmo::MsgFunc_ItemPickup>("ItemPickup");
	HookMessage<&CHudAmmo::MsgFunc_HideWeapon>("HideWeapon"); // hides the weapon, ammo, and crosshair displays temporarily
	HookMessage<&CHudAmmo::MsgFunc_AmmoX>("AmmoX"); // update known ammo type's count

	HookCommand<&CHudAmmo::UserCmd_Slot1>("slot1");
	HookCommand<&CHudAmmo::UserCmd_Slot2>("slot2");
	HookCommand<&CHudAmmo::UserCmd_Slot3>("slot3");
	HookCommand<&CHudAmmo::UserCmd_Slot4>("slot4");
	HookCommand<&CHudAmmo::UserCmd_Slot5>("slot5");
	HookCommand<&CHudAmmo::UserCmd_Slot6>("slot6");
	HookCommand<&CHudAmmo::UserCmd_Slot7>("slot7");
	HookCommand<&CHudAmmo::UserCmd_Slot8>("slot8");
	HookCommand<&CHudAmmo::UserCmd_Slot9>("slot9");
	HookCommand<&CHudAmmo::UserCmd_Slot10>("slot10");
	HookCommand<&CHudAmmo::UserCmd_Close>("cancelselect");
	//HookCommand<&CHudAmmo::UserCmd_NextWeapon>("invnext");
	//HookCommand<&CHudAmmo::UserCmd_PrevWeapon>("invprev");

	Reset();

	m_iFlags |= HUD_ACTIVE; //!!!

	gWR.Init();
	gHR.Init();
};

void CHudAmmo::Reset(void)
{
	m_fFade = 0;
	g_ActiveAmmoIndex = -1;
	m_pWeapon = NULL;
	m_fOnTarget = false;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay = 0;

	gWR.Reset();
	gHR.Reset();

	//	VidInit();

	m_iMaxSlot = 4;
}

void CHudAmmo::VidInit()
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex("bucket1");
	m_HUD_selection = gHUD.GetSpriteIndex("selection");

	ghsprBuckets = gHUD.GetSprite(m_HUD_bucket0);
	giBucketWidth = gHUD.GetSpriteRect(m_HUD_bucket0).right - gHUD.GetSpriteRect(m_HUD_bucket0).left;
	giBucketHeight = gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top;

	gHR.iHistoryGap = max(gHR.iHistoryGap, gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top);

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	giABWidth = SPR_RES_SCALED(20);
	giABHeight = SPR_RES_SCALED(4);
}

//
// Think:
//  Used for selection of weapon menu item.
//
void CHudAmmo::Think(void)
{
	if (gHUD.m_iFOV != m_iLastFOV)
		m_iLastFOV = gHUD.m_iFOV;

	if (gHUD.m_fPlayerDead)
		return;

	if (gHUD.m_iWeaponBits != gWR.iOldWeaponBits)
	{
		gWR.iOldWeaponBits = gHUD.m_iWeaponBits;

		for (int i = 0; i < LAST_WEAPON_ID; i++)
		{
			WEAPON *p = gWR.GetWeapon(i);
			if (p)
			{
				if (gHUD.m_iWeaponBits & (1 << i))
					gWR.PickupWeapon(p, i);
				else
					gWR.DropWeapon(p, i);
			}
		}
	}

	if (!gpActiveSel)
		return;

	// has the player selected one?
	if (gHUD.m_iKeyBits & IN_ATTACK || hud_fastswitch.GetInt() == 2)
	{
		if ( !HasRoundBegun() )
		{
			HudAmmo_ClearWeaponSelectionInputState();
			return;
		}

		if (gpActiveSel != (WEAPON *)1)
		{
			ServerCmd(gpActiveSel->szName);
			g_weaponselect = gpActiveSel->iId;
		}

		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		PlaySound("common/wpn_select.wav", 1);
	}
}

//
// Helper function to return a Ammo pointer from id
//

CHud::RegisteredIcon WeaponsResource::GetAmmoPicFromWeapon(int iAmmoId)
{
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if (rgWeapons[i].iAmmoType == iAmmoId)
			return rgWeapons[i].hAmmo;
		else if (rgWeapons[i].iAmmo2Type == iAmmoId)
			return rgWeapons[i].hAmmo2;
	}
	return CHud::RegisteredIcon();
}

//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
//
int CHudAmmo::MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	gWR.SetAmmo(iIndex, abs(iCount));

	m_fFade = FADE_TIME;

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	// Ammo Index
	AmmoData data = GetAmmoByAmmoID( iIndex );

	// Add ammo to the history
	gHR.AddToHistory(HISTSLOT_AMMO, data.AmmoName, abs(iCount));

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	int iIndex = READ_BYTE();
	int iAssignedSlot = READ_BYTE();
	int iClipFix = READ_BYTE();

	// Update the weapon slot position
	gWR.UpdatedWeaponSlotPos( iIndex, iAssignedSlot );

	// Weapon Index
	WeaponData data = GetWeaponSlotInfo( (ZPWeaponID)iIndex );
	gWR.UpdateWeaponData( iIndex, data );

	// Fixes the clip amount, so it's not maxed out when you pick up a weapon with less ammo in the clip
	gWR.UpdateWeaponClip( iIndex, iClipFix );

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, data.Icons[WeaponDataIcons::ICON_WEAPON] );

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory(HISTSLOT_ITEM, szName);

	return 1;
}

int CHudAmmo::MsgFunc_HideWeapon(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if (gEngfuncs.IsSpectateOnly())
		return 1;

	if (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL))
		gpActiveSel = NULL;

	static wrect_t nullrc;
	SetCrosshair(0, nullrc, 0, 0, 0);

	return 1;
}

bool CHudAmmo::CanDrawAmmo( int iAmmoType, bool &bDrawClip )
{
	bDrawClip = false;
	if ( iAmmoType < 0 ) return false;
	AmmoData data = GetAmmoByAmmoID( iAmmoType );
	if ( data.AmmoType == AMMO_NONE ) return false;
	if ( data.MaxCarry <= 0 )
	{
		if ( data.DrawClip )
			bDrawClip = true;
		return bDrawClip;
	}
	return true;
}

bool CHudAmmo::IsIconValid(CHud::RegisteredIcon icon)
{
	return ( icon.Icon > -1 ) ? true : false;
}

//
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf)
{
	static wrect_t nullrc;

	BEGIN_READ(pbuf, iSize);

	int iState = READ_BYTE();
	int iId = READ_CHAR();
	int iClip = READ_CHAR();
	int iMaxClip = READ_CHAR();

	// detect if we're also on target
	m_fOnTarget = iState > 1;

	if (iId < 1)
	{
		g_ActiveAmmoIndex = -1;
		m_pWeapon = NULL;
		SetCrosshair(0, nullrc, 0, 0, 0);
		return 0;
	}

	if (g_iUser1 != OBS_IN_EYE)
	{
		// Is player dead???
		if ((iId == -1) && (iClip == -1))
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
			return 1;
		}
		gHUD.m_fPlayerDead = FALSE;
	}

	WEAPON *pWeapon = gWR.GetWeapon(iId);

	if (!pWeapon)
		return 0;

	if (iClip < -1)
		pWeapon->iClip = abs(iClip);
	else
		pWeapon->iClip = iClip;

	if (iMaxClip < -1)
		pWeapon->iMaxClip = abs(iMaxClip);
	else
		pWeapon->iMaxClip = iMaxClip;

	if (iState == 0) // we're not the current weapon, so update no more
		return 1;

	m_pWeapon = pWeapon;
	g_ActiveAmmoIndex = m_pWeapon->iAmmoType;

	SetCrosshair( 0, nullrc, 0, 0, 0 );

	m_fFade = FADE_TIME;
	m_iFlags |= HUD_ACTIVE;

	return 1;
}

void CHudAmmo::DrawCrosshair()
{
	static wrect_t nullrc;
	if ( m_pWeapon == NULL )
	{
		SetCrosshair( 0, nullrc, 0, 0, 0 );
		return;
	}
	if ( CHudCrosshair::Get()->IsEnabled() ) return;

	int y = ScreenHeight / 2;
	int x = ScreenWidth / 2;
	Color COLOR_WHITE( 255, 255, 255, 255 );

	if ( gHUD.m_iFOV >= 90 )
	{
		if ( IsIconValid( m_pWeapon->hAutoaim ) && m_fOnTarget )
			DrawCrosshair( m_pWeapon->hAutoaim, x, y, COLOR_WHITE );
		else
			DrawCrosshair( m_pWeapon->hCrosshair, x, y, COLOR_WHITE );
	}
	else
	{
		if ( m_fOnTarget && IsIconValid( m_pWeapon->hZoomedAutoaim ) )
			DrawCrosshair( m_pWeapon->hZoomedAutoaim, x, y, COLOR_WHITE );
		else
			DrawCrosshair( m_pWeapon->hZoomedCrosshair, x, y, COLOR_WHITE );
	}
}

void CHudAmmo::DrawCrosshair( CHud::RegisteredIcon icon, int x, int y, Color clr )
{
	int yOffset = icon.Tall / 2;
	int xOffset = icon.Wide / 2;
	int xpos = x - xOffset;
	int ypos = y - yOffset;
	vgui2::surface()->DrawSetTexture( icon.Icon );
	vgui2::surface()->DrawSetColor( clr );
	vgui2::surface()->DrawTexturedRect( xpos, ypos, xpos + icon.Wide, ypos + icon.Tall );
}

void CHudAmmo::DrawBackgroundSlot( const Color &clr, int x, int y, CHud::RegisteredIcon icon )
{
	gEngfuncs.pfnFillRGBA( x, y, icon.Wide, icon.Tall, clr.r(), clr.g(), clr.b(), clr.a() );
}

void CHudAmmo::DrawBackgroundSlot(const Color &clr, int x, int y, int wide, int tall)
{
	gEngfuncs.pfnFillRGBA( x, y, wide, tall, clr.r(), clr.g(), clr.b(), clr.a() );
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);

	WEAPON Weapon;

	V_strcpy_safe(Weapon.szName, READ_STRING());
	Weapon.iAmmoType = (int)READ_CHAR();
	Weapon.iMax1 = GetAmmoByAmmoID( Weapon.iAmmoType ).MaxCarry;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2 = GetAmmoByAmmoID( Weapon.iAmmo2Type ).MaxCarry;

	// Changed from iSlot to m_iAssignedSlotPosition on the server side.
	Weapon.iSlot = -1;// was READ_CHAR();
	// Unused
	Weapon.iSlotPos = -1;// was READ_CHAR();
	Weapon.iId = READ_CHAR();
	Weapon.iFlags = READ_BYTE();
	Weapon.iWeight = READ_BYTE();
	Weapon.bDoubleSlot = ( READ_BYTE() >= 1 );
	Weapon.bHasWeapon = false;
	Weapon.iClip = 0;
	Weapon.iMaxClip = 0;

	//if (Weapon.iId < 0 || Weapon.iId >= MAX_WEAPONS)
	//	return 0;

	//if (Weapon.iSlot < 0 || Weapon.iSlot >= MAX_WEAPON_SLOTS + 1)
	//	return 0;

	//if (Weapon.iSlotPos < 0 || Weapon.iSlotPos >= MAX_WEAPON_POSITIONS + 1)
	//	return 0;

	if (Weapon.iAmmoType < -1 || Weapon.iAmmoType >= ZPAmmoTypes::AMMO_MAX)
		return 0;

	if (Weapon.iAmmo2Type < -1 || Weapon.iAmmo2Type >= ZPAmmoTypes::AMMO_MAX)
		return 0;

	if (Weapon.iAmmoType >= 0 && Weapon.iMax1 == 0)
		return 0;

	if (Weapon.iAmmo2Type >= 0 && Weapon.iMax2 == 0)
		return 0;

	gWR.AddWeapon(&Weapon);

	if (Weapon.iSlot > m_iMaxSlot && Weapon.iSlot < MAX_WEAPON_SLOTS)
		m_iMaxSlot = Weapon.iSlot;

	return 1;
}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// Slot button pressed
void CHudAmmo::SlotInput(int iSlot)
{
	// Let the Viewport use it first, for menus
	if (g_pViewport && g_pViewport->SlotInput(iSlot))
		return;

	if ( !HasRoundBegun() )
	{
		HudAmmo_ClearWeaponSelectionInputState();
		return;
	}

	// Tell the server we selected this weapon slot
	char szCmd[16];
	Q_snprintf(szCmd, sizeof(szCmd), "slot %d", iSlot );
	ServerCmd(szCmd);
}

void CHudAmmo::UserCmd_Slot1(void)
{
	SlotInput(0);
}

void CHudAmmo::UserCmd_Slot2(void)
{
	SlotInput(1);
}

void CHudAmmo::UserCmd_Slot3(void)
{
	SlotInput(2);
}

void CHudAmmo::UserCmd_Slot4(void)
{
	SlotInput(3);
}

void CHudAmmo::UserCmd_Slot5(void)
{
	SlotInput(4);
}

void CHudAmmo::UserCmd_Slot6(void)
{
	SlotInput(5);
}

void CHudAmmo::UserCmd_Slot7(void)
{
	SlotInput(6);
}

void CHudAmmo::UserCmd_Slot8(void)
{
	SlotInput(7);
}

void CHudAmmo::UserCmd_Slot9(void)
{
	SlotInput(8);
}

void CHudAmmo::UserCmd_Slot10(void)
{
	SlotInput(9);
}

void CHudAmmo::UserCmd_Close(void)
{
	/*
	if (gpActiveSel)
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		PlaySound("common/wpn_hudoff.wav", 1);
	}
	else
	*/
	gEngfuncs.pfnClientCmd("escape");
}

//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------

void CHudAmmo::Draw(float flTime)
{
	int x, y, r, g, b;
	float a;
	int AmmoWidth;

	// If the user is observing in free roam mode, don't draw.
	if ( g_iUser1 && g_iUser1 != OBS_IN_EYE ) return;

	if (!(gHUD.m_iWeaponBits & (1 << (WEAPON_SUIT))))
		return;

	if ((gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)))
		return;

	// Draw Weapon Menu
	// DrawWList has been removed, because we don't use the old HL1 weapon menu anymore.
	// We now use a new one designed for Zombie Panic.
	DrawWeaponSlots();

	// Draw our crosshair
	DrawCrosshair();

	if (!(m_iFlags & HUD_ACTIVE))
		return;

	if (!m_pWeapon)
		return;

	WEAPON *pw = m_pWeapon; // shorthand

	AmmoWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;

	if (!hud_dim.GetBool())
	{
		a = MIN_ALPHA + ALPHA_AMMO_MAX;
	}
	else if (m_fFade > 0)
	{
		// Fade the ammo number back to dim
		m_fFade -= (gHUD.m_flTimeDelta * 20);
		if (m_fFade <= 0)
			m_fFade = 0;
		a = MIN_ALPHA + (m_fFade / FADE_TIME) * ALPHA_AMMO_FLASH;
	}
	else
	{
		a = MIN_ALPHA;
	}

	float alphaDim = a;

	a *= gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	ScaleColors(r, g, b, a);

	// SPR_Draw Ammo
	if ((pw->iAmmoType < 0) && (pw->iAmmo2Type < 0))
		return;

	int iFlags = DHN_DRAWZERO; // draw 0 values

	// Does this weapon have a clip?
	y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2;
	y += (int)(gHUD.m_iFontHeight * 0.2f);

	// Does weapon have any ammo at all?
	bool bDrawClip;
	if ( CanDrawAmmo( m_pWeapon->iAmmoType, bDrawClip ) )
	{
		int iIconWidth = m_pWeapon->hAmmo.Wide;

		if ( bDrawClip )
		{
			a = alphaDim * gHUD.GetHudTransparency();
			gHUD.GetHudAmmoColor( pw->iClip, pw->iMaxClip, r, g, b );
			ScaleColors(r, g, b, a);

			// SPR_Draw a bullets only line
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber( x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b );
		}
		else if ( pw->iClip >= 0 )
		{
			a = alphaDim * gHUD.GetHudTransparency();
			gHUD.GetHudAmmoColor(pw->iClip, pw->iMaxClip, r, g, b);
			ScaleColors(r, g, b, a);

			// room for the number and the '|' and the current ammo
			x = ScreenWidth - (8 * AmmoWidth) - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b);

			wrect_t rc;
			rc.top = 0;
			rc.left = 0;
			rc.right = AmmoWidth;
			rc.bottom = 100;

			int iBarWidth = AmmoWidth / 10;

			x += AmmoWidth / 2;

			// draw the | bar
			FillRGBA(x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a);

			x += iBarWidth + AmmoWidth / 2;

			// GL Seems to need this
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}
		else
		{
			a = alphaDim * gHUD.GetHudTransparency();
			gHUD.GetHudAmmoColor(gWR.CountAmmo(pw->iAmmoType), pw->iMax1, r, g, b);
			ScaleColors(r, g, b, a);

			// SPR_Draw a bullets only line
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}

		// Draw the ammo Icon
		if ( IsIconValid( m_pWeapon->hAmmo ) )
		{
			int iOffset = m_pWeapon->hAmmo.Wide / 8;
			vgui2::surface()->DrawSetTexture( m_pWeapon->hAmmo.Icon );
			vgui2::surface()->DrawSetColor( Color( r, g, b, a ) );
			vgui2::surface()->DrawTexturedRect( x, y - iOffset, x + m_pWeapon->hAmmo.Wide, y - iOffset + m_pWeapon->hAmmo.Tall );
		}
	}

	// Does weapon have seconday ammo?
	if ( CanDrawAmmo( pw->iAmmo2Type, bDrawClip ) )
	{
		int iIconWidth = m_pWeapon->hAmmo2.Wide;

		// Do we have secondary ammo?
		if ((pw->iAmmo2Type != 0) && (gWR.CountAmmo(pw->iAmmo2Type) > 0))
		{
			a = alphaDim * gHUD.GetHudTransparency();
			gHUD.GetHudAmmoColor(pw->iClip, pw->iMaxClip, r, g, b);
			ScaleColors(r, g, b, a);

			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight / 4;
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmo2Type), r, g, b);

			// Draw the ammo Icon
			if ( IsIconValid( m_pWeapon->hAmmo2 ) )
			{
				int iOffset = m_pWeapon->hAmmo2.Wide / 8;
				vgui2::surface()->DrawSetTexture( m_pWeapon->hAmmo2.Icon );
				vgui2::surface()->DrawSetColor( Color( r, g, b, a ) );
				vgui2::surface()->DrawTexturedRect( x, y - iOffset, x + m_pWeapon->hAmmo2.Wide, y - iOffset + m_pWeapon->hAmmo2.Tall );
			}
		}
	}
}

//
// Draws the ammo bar on the hud
//
int DrawBar(int x, int y, int width, int height, float f)
{
	int r, g, b;
	float a;

	if (f < 0)
		f = 0;
	if (f > 1)
		f = 1;

	if (f)
	{
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if (w <= 0)
			w = 1;
		a = 255 * gHUD.GetHudTransparency();
		UnpackRGB(r, g, b, RGB_GREENISH);
		FillRGBA(x, y, w, height, r, g, b, a);
		x += w;
		width -= w;
	}

	a = 128 * gHUD.GetHudTransparency();
	gHUD.GetHudColor(HudPart::Common, 0, r, g, b);
	FillRGBA(x, y, width, height, r, g, b, a);

	return (x + width);
}

void DrawAmmoBar(WEAPON *p, int x, int y, int width, int height)
{
	if (!p)
		return;

	if (p->iAmmoType != -1)
	{
		if (!gWR.CountAmmo(p->iAmmoType))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType) / (float)p->iMax1;

		x = DrawBar(x, y, width, height, f);

		// Do we have secondary ammo too?

		if (p->iAmmo2Type != -1)
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type) / (float)p->iMax2;

			x += 5; //!!!

			DrawBar(x, y, width, height, f);
		}
	}
}

#if DEBUG_WEAPON_SLOTS
#include "con_nprint.h"
static int DEBUG_PRINT_POS = 5;
static void DebugPrintValue( Color clr, const char *fmt, ... )
{
	va_list argptr;
	char string[1024];
	va_start( argptr, fmt );
	Q_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );
	con_nprint_s sInfo;
	sInfo.index = DEBUG_PRINT_POS;
	sInfo.color[0] = clr.r() / 255;
	sInfo.color[1] = clr.g() / 255;
	sInfo.color[2] = clr.b() / 255;
	sInfo.time_to_live = 1.0;
	gEngfuncs.Con_NXPrintf( &sInfo, "%s", string );
	DEBUG_PRINT_POS++;
}
#endif

void CHudAmmo::DrawWeaponSlots()
{
	bool bHardcore = ( gHUD.m_GameMode == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	int iMaxSlots = bHardcore ? MAX_WEAPON_SLOTS_HARDCORE : MAX_WEAPON_SLOTS;
	if ( bHardcore )
	{
		// We use weapon bits to determine if we have a backpack
		if (gHUD.m_iWeaponBits & (1 << WEAPON_BACKPACK))
			iMaxSlots += BACKPACK_EXTRA_SLOTS;
	}

	static int WEAPON_SLOT_WIDE = 150;
	static int WEAPON_SLOT_TALL = 80;
	static int WEAPON_SLOTS_XPOS = 80;
	static int WEAPON_SLOTS_YPOS = ScreenHeight - 5;

	int x, y, wide, tall;
	switch ( gHUD.m_iRes )
	{
		default:
		case 320:
		case 640:
	    case 1280:
			x = WEAPON_SLOTS_XPOS;
			wide = WEAPON_SLOT_WIDE;
			tall = WEAPON_SLOT_TALL;
		break;
	    case 2560:
			x = (WEAPON_SLOTS_XPOS * 2);
			wide = (WEAPON_SLOT_WIDE * 2);
			tall = (WEAPON_SLOT_TALL * 2);
		break;
	}

	if ( hud_weaponslot_corner_hug.GetBool() )
		x = 5;

	y = WEAPON_SLOTS_YPOS - tall;
#if DEBUG_WEAPON_SLOTS
	DEBUG_PRINT_POS = 5;
#endif

	// Check if we have a double slot weapon in this slot
	bool bHasDoubleSlot = false;

	for ( int i = 0; i < iMaxSlots; i++ )
	{
		// Skip this slot, since the previous weapon used 2 slots
		if ( bHasDoubleSlot )
		{
			bHasDoubleSlot = false;
			continue;
		}

		// Get the weapon in this slot
		WEAPON *pWeapon = gWR.GetWeaponBySlot( i );
		if ( pWeapon && pWeapon->bHasWeapon )
			bHasDoubleSlot = pWeapon->bDoubleSlot;

		// Set our width
		int draw_wide = bHasDoubleSlot ? ( wide * 2 ) : wide;

		// Draw the slot
		DrawWeaponSlot( pWeapon, i, x, y, draw_wide, tall );

		// Move over
		x += draw_wide + 5;
	}
}

void CHudAmmo::DrawWeaponSlot( WEAPON *pWeapon, int slot, int x, int y, int wide, int tall )
{
	bool bSelected = false;
	if ( m_pWeapon && pWeapon )
		bSelected = ( m_pWeapon == pWeapon );
	int r, g, b, a;
	a = 220 * gHUD.GetHudTransparency();
	r = g = b = 20 * gHUD.GetHudTransparency();
	ScaleColors( r, g, b, a );

	// Draw Background
	Color ItemBG( r, g, b, a );
	DrawBackgroundSlot( ItemBG, x, y, wide, tall );

	// Draw the weapon icon
	if ( pWeapon && pWeapon->bHasWeapon && pWeapon->hActive.Texture[0] != '\0' )
	{
		r = g = b = 255 * gHUD.GetHudTransparency();
		ItemBG.SetColor( r, g, b, 255 );
		vgui2::surface()->DrawSetTexture( pWeapon->hActive.Icon );
		vgui2::surface()->DrawSetColor( ItemBG );
		vgui2::surface()->DrawTexturedRect( x, y, x + wide, y + tall );
	}

	// Draw Slot Number
	r = g = b = 255 * gHUD.GetHudTransparency();
	if ( bSelected )
		a = 220 * gHUD.GetHudTransparency();
	else
		a = 120 * gHUD.GetHudTransparency();
	ScaleColors( r, g, b, a );
	Color TextClr( r, g, b, a );
	gHUD.DrawHudNumberSmall( x + 4, y + 4, slot + 1, r, g, b );

	// Draw Ammo Count at the bottom
	if ( pWeapon && pWeapon->bHasWeapon && pWeapon->iAmmoType > 0 )
	{
		// Doesn't look that good, so it's disabled for now
#if 0
		int smallfontHeight = gHUD.GetSpriteRect(gHUD.m_HUD_number_small_0).GetHeight();
		int smallfontWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_small_0).GetWidth();
		// Let's draw the background first, so it's more visible
		r = g = b = 10;
		a = 220 * gHUD.GetHudTransparency();
		ScaleColors( r, g, b, 220 );
		ItemBG.SetColor( r, g, b, a );
		DrawBackgroundSlot( ItemBG, x, y + tall - smallfontHeight, wide, smallfontHeight );

		// Now let's draw the remaining ammo count on the bottom right
		int iAmmo = pWeapon->iClip;
		if ( iAmmo < 0 ) iAmmo = 0;

		// First, we drawthe clip amount before the |
		r = g = b = 255 * gHUD.GetHudTransparency();
		if ( bSelected )
			a = 220 * gHUD.GetHudTransparency();
		else
			a = 120 * gHUD.GetHudTransparency();
		ScaleColors( r, g, b, a );
		int xAmmoPos = gHUD.DrawHudNumberSmall( x + wide - smallfontWidth - ( smallfontWidth * 4 ), y + tall - smallfontHeight - 2, iAmmo, r, g, b );

		// Draw the | bar
		r = g = b = 255 * gHUD.GetHudTransparency();
		if ( bSelected )
			a = 200 * gHUD.GetHudTransparency();
		else
			a = 100 * gHUD.GetHudTransparency();
		ScaleColors( r, g, b, a );
		int iBarWidth = 2;
		int halffontWidth = smallfontWidth / 2;
		xAmmoPos += halffontWidth;
		FillRGBA( xAmmoPos, y + tall - smallfontHeight - 2, iBarWidth, smallfontHeight, r, g, b, a );

		// Now we draw the remaining ammo after the |
		iAmmo = gWR.CountAmmo( pWeapon->iAmmoType );
		if ( iAmmo < 0 ) iAmmo = 0;

		r = g = b = 255 * gHUD.GetHudTransparency();
		if ( bSelected )
			a = 220 * gHUD.GetHudTransparency();
		else
			a = 120 * gHUD.GetHudTransparency();
		ScaleColors( r, g, b, a );

		xAmmoPos += halffontWidth + (iBarWidth * 2);
		gHUD.DrawHudNumberSmall( xAmmoPos, y + tall - smallfontHeight - 2, iAmmo, r, g, b );
#else
		int smallfontHeight = gHUD.GetSpriteRect(gHUD.m_HUD_number_small_0).GetHeight();
		int smallfontWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_small_0).GetWidth();
		r = g = b = 10;
		a = 220 * gHUD.GetHudTransparency();
		ScaleColors( r, g, b, 220 );

		// Draw the background first, so it's more visible
		ItemBG.SetColor( r, g, b, a );
		if ( !gHUD.IsRainBowHUD() )
			DrawBackgroundSlot( ItemBG, x, y + tall - smallfontHeight, wide, smallfontHeight );

		int iAmmo = pWeapon->iClip;// gWR.CountAmmo( pWeapon->iAmmoType );
		if ( iAmmo < 0 ) iAmmo = 0;

		r = g = b = 255 * gHUD.GetHudTransparency();
		if ( bSelected )
			a = 220 * gHUD.GetHudTransparency();
		else
			a = 120 * gHUD.GetHudTransparency();

		// Let's draw it at the bottom right, but with some minimal offset,
		// so that it won't touch the border.
		ScaleColors( r, g, b, a );
		int AmmoWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_small_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_small_0).left;
		int CrossWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
		int xAmmoPos = x + wide - (smallfontWidth + CrossWidth + AmmoWidth / 2);

		gHUD.DrawHudNumberSmall( xAmmoPos, y + tall - smallfontHeight - 2, DHN_3DIGITS | DHN_DRAWZERO, iAmmo, r, g, b );
#endif
	}

	// Draw Border
	if ( bSelected )
	{
		r = 255 * gHUD.GetHudTransparency();
		g = b = 100 * gHUD.GetHudTransparency();
	}
	else
		r = g = b = 100 * gHUD.GetHudTransparency();

	// Make it a bit more visible if selected
	if ( bSelected )
		a = 200 * gHUD.GetHudTransparency();
	else
		a = 100 * gHUD.GetHudTransparency();
	ScaleColors( r, g, b, a );

	FillRGBA( x, y, wide, 1, r, g, b, a ); // Top
	FillRGBA( x, y + tall - 1, wide, 1, r, g, b, a ); // Bottom
	FillRGBA( x, y, 1, tall, r, g, b, a ); // Left
	FillRGBA( x + wide - 1, y, 1, tall, r, g, b, a ); // Right

#if DEBUG_WEAPON_SLOTS
	DebugPrintValue(
		Color(255, 255, 255),
		"%d - %s",
		slot + 1,
		(pWeapon) ? pWeapon->szName : ""
	);
	if ( pWeapon )
	{
		if ( pWeapon->bHasWeapon )
			DebugPrintValue( Color(0, 255, 0), "HasWeapon: TRUE" );
		else
			DebugPrintValue( Color(255, 0, 0), "HasWeapon: FALSE" );
	}
	else
		DebugPrintValue( Color(255, 0, 0), "HasWeapon: FALSE" );
#endif
}

/* =================================
	GetSpriteFromList

Finds and returns the sprite which name starts
with 'pszNameStart' and resolution 'iRes'
in the given sprite list 'pList'
iCount is the number of items in the pList
================================= */
client_sprite_t *GetSpriteFromList(client_sprite_t *pList, const char *pszNameStart, int iRes, int iCount)
{
	if (!pList || iCount <= 0)
		return nullptr;

	int len = strlen(pszNameStart);
	client_sprite_t *p = pList;
	client_sprite_t *pFallback = nullptr; //!< Fallback sprite if can't find for current HUD scale
	while (iCount--)
	{
		if (!strncmp(pszNameStart, p->szName, len))
		{
			if (p->iRes == iRes)
			{
				// Found the requested sprite
				return p;
			}
			else if (p->iRes == HUD_FALLBACK_RES)
			{
				// At least found a fallbcak
				pFallback = p;
			}
		}

		p++;
	}

	// Return the fallback. It may be null but that's fine.
	return pFallback;
}
