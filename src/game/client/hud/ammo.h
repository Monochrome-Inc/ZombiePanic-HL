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

#ifndef HUD_AMMO_H
#define HUD_AMMO_H
#include "base.h"

#define WEAPON_FLAGS_SELECTONEMPTY 1

#define WEAPON_IS_ONTARGET 0x40

struct WEAPON
{
	char szName[MAX_WEAPON_NAME];
	int iAmmoType;
	int iAmmo2Type;
	int iMax1;
	int iMax2;
	int iSlot = -1;
	int iSlotPos;
	int iFlags;
	int iId;
	int iClip;
	int iMaxClip;
	int iWeight;
	bool bDoubleSlot;
	bool bHasWeapon = false;

	int iCount; // # of itesm in plist

	CHud::RegisteredIcon hActive;
	CHud::RegisteredIcon hInactive;
	CHud::RegisteredIcon hAmmo;
	CHud::RegisteredIcon hAmmo2;
	CHud::RegisteredIcon hCrosshair;
	CHud::RegisteredIcon hAutoaim;
	CHud::RegisteredIcon hZoomedCrosshair;
	CHud::RegisteredIcon hZoomedAutoaim;
};

typedef int AMMO;

class CHudAmmo : public CHudElemBase<CHudAmmo>
{
public:
	void Init();
	void VidInit();
	void Draw(float flTime);
	void Think();
	void Reset();
	void DrawWeaponSlots();
	void DrawWeaponSlot( WEAPON *pWeapon, int slot, int x, int y, int wide, int tall );
	void DrawCrosshair();
	void DrawCrosshair(CHud::RegisteredIcon icon, int x, int y, Color clr);
	void DrawBackgroundSlot(const Color &clr, int x, int y, CHud::RegisteredIcon icon);
	void DrawBackgroundSlot(const Color &clr, int x, int y, int wide, int tall);
	int MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_WeaponList(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_AmmoPickup(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_WeapPickup(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_ItemPickup(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_HideWeapon(const char *pszName, int iSize, void *pbuf);

	bool CanDrawAmmo( int iAmmoType, bool &bDrawClip );

	bool IsIconValid(CHud::RegisteredIcon icon);

	int GetMaxSlot() { return m_iMaxSlot; }
	void SlotInput(int iSlot);
	void UserCmd_Slot1();
	void UserCmd_Slot2();
	void UserCmd_Slot3();
	void UserCmd_Slot4();
	void UserCmd_Slot5();
	void UserCmd_Slot6();
	void UserCmd_Slot7();
	void UserCmd_Slot8();
	void UserCmd_Slot9();
	void UserCmd_Slot10();
	void UserCmd_Close();

	WEAPON *GetActiveWeapon() { return m_pWeapon; }

private:
	float m_fFade;
	WEAPON *m_pWeapon;
	bool m_fOnTarget;
	int m_HUD_bucket0;
	int m_HUD_selection;
	int m_iMaxSlot; // There are 5 (0-4) slots by default and they can extend to 6. This will be used to draw additional weapon bucket(s) on a hud.
	int m_iLastFOV = 0;

	friend class CHudCrosshair;
};

#endif
