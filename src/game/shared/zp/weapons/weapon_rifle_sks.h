// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SHARED_WEAPON_RIFLE_SKS_H
#define SHARED_WEAPON_RIFLE_SKS_H

#include "CWeaponBaseSingleAction.h"
#include "CWeaponBaseMelee.h"

class CWeaponRifleSKS : public CWeaponBaseSingleAction, public IMeleeBaseShared
{
	DECLARE_CLASS_SIMPLE( CWeaponRifleSKS, CWeaponBaseSingleAction );

public:
	ZPWeaponID GetWeaponID() override { return WEAPON_SKS; }
	bool IsAutomaticWeapon() const override { return false; }
	bool PumpIsRequired() const override { return false; }
	const char *GetEmptySound() const override { return "weapons/556ar/dryfire.wav"; }
	float DoHolsterAnimation() override;
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	float Deploy();
	float DoWeaponUnload();
	void OnRequestedAnimation( SingleActionAnimReq act );
	void OnWeaponPrimaryAttack();
	void Reload( void ) override;

	// Melee
	void WeaponIdle() override;
	void SecondaryAttack( void ) override;
	void DoWeaponSoundFromAttack( MeleeAttackType attackType, bool bHitWorld ) override;
	void DoWeaponSoundFromMiss( MeleeAttackType attackType ) override;
	float GetAttackAnimationTime( int iAnim ) override;
	int DoMeleeAnimation( CWeaponBase *pWeapon ) override;
};

#endif