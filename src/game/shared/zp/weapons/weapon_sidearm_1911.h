// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SHARED_WEAPON_SIDEARM_SIG_H
#define SHARED_WEAPON_SIDEARM_SIG_H

#include "CWeaponBase.h"

class CWeaponSideArm1911 : public CWeaponBase
{
	DECLARE_CLASS_SIMPLE( CWeaponSideArm1911, CWeaponBase );

public:
	ZPWeaponID GetWeaponID() override { return WEAPON_1911; }
	bool IsAutomaticWeapon() const override { return false; }
	const char *GetEmptySound() const override { return "weapons/1911/dryfire.wav"; }
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