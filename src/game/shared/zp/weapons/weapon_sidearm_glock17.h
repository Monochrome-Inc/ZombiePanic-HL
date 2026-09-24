// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SHARED_WEAPON_SIDEARM_GLOCK17_H
#define SHARED_WEAPON_SIDEARM_GLOCK17_H

#include "CWeaponBase.h"

class CWeaponSideArmGlock17 : public CWeaponBase
{
	DECLARE_CLASS_SIMPLE( CWeaponSideArmGlock17, CWeaponBase );

public:
	ZPWeaponID GetWeaponID() override { return WEAPON_GLOCK17; }
	bool IsAutomaticWeapon() const override { return false; }
	const char *GetEmptySound() const override { return "weapons/glock17/dryfire.wav"; }
	float DoHolsterAnimation() override;
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	float Deploy();
	float DoWeaponUnload();
	void Reload( void );
	void PrimaryAttack( void );
	void WeaponIdle( void );
};

#endif