// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SHARED_WEAPON_BASE_MELEE_H
#define SHARED_WEAPON_BASE_MELEE_H

#include "CWeaponBase.h"

#define MELEE_SND_BODYHIT_VOLUME 128
#define MELEE_SND_WALLHIT_VOLUME 512

#define MELEE_MAX_ATTACKMAP_TRACES 2
#define MELEE_MAX_ATTACKMAP_TRACER_SETS 2

class IMeleeBaseShared
{
public:
	void LoadMeleeConfigFile( ZPWeaponID nWepID );

	int GetMeleeDamageTypeFromString( const char *szDmgType ) const;

	enum MeleeAttackType
	{
		MELEE_ATTACK_NONE = 0,	// Default state
		MELEE_ATTACK_LIGHT,		// Primary attack
		MELEE_ATTACK_HEAVY		// Secondary attack
	};
	
	enum WhatDidWeHit
	{
		HIT_NOTHING = 0,
		HIT_WORLD,
		HIT_ENTITY
	};

	bool DidMeleeAttackHit( CWeaponBase *pWeapon, MeleeAttackType attackTrace, bool &bHitWorld );
	WhatDidWeHit DoAttackTrace( CWeaponBase *pWeapon, MeleeAttackType attackType, int iTracerSet, bool bDoHullTrace );
	void DoMeleeAttack( CWeaponBase *pWeapon );
	virtual int DoMeleeAnimation( CWeaponBase *pWeapon );

	bool IsAttackInProgress() const { return ( n_meleeAttackType != MELEE_ATTACK_NONE ); }
	bool IsInHeavyAttack() const { return ( n_meleeAttackType == MELEE_ATTACK_HEAVY ); }
	bool IsInLightAttack() const { return ( n_meleeAttackType == MELEE_ATTACK_LIGHT ); }

	// Melee models, uses crowbar model by default
	virtual const char *GetMeleeWorldModel() const { return "models/w_crowbar.mdl"; }
	virtual const char *GetMeleeViewModel() const { return "models/v_crowbar.mdl"; }
	virtual const char *GetMeleePlayerModel() const { return "models/p_crowbar.mdl"; }
	virtual const char *GetMeleePlayerExt() const { return "crowbar"; }
	virtual Bullet GetBulletType() const { return BULLET_PLAYER_CROWBAR; }

	float GetMeleeAttackRange( MeleeAttackType attackType ) const;
	float GetMeleeAttackDamage( MeleeAttackType attackType ) const;
	float GetMeleeAttackAnimationTime( MeleeAttackType attackType ) const;
	int GetMeleeDamageType( MeleeAttackType attackType ) const;

	virtual void DoWeaponSoundFromAttack( MeleeAttackType attackType, bool bHitWorld ) = 0;
	virtual void DoWeaponSoundFromMiss( MeleeAttackType attackType ) = 0;
	virtual float GetAttackAnimationTime( int iAnim ) = 0;

	void ResetMeleeState() { n_meleeAttackType = MELEE_ATTACK_NONE; }

protected:
	MeleeAttackType n_meleeAttackType;
	int m_iAttackTracerCount; // Amount of tracers per attack

	// Melee attack tracer info
	struct MeleeAttackTrace
	{
		int iDmgType;
		int iAmount;
		float flAnimTime;
		float flAnimTimeHold; // Only used for heavy attacks for now.
		float flRange;
		float flDamage;
		Vector vecStart[MELEE_MAX_ATTACKMAP_TRACER_SETS];
		Vector vecEnd[MELEE_MAX_ATTACKMAP_TRACER_SETS];
	};
	MeleeAttackTrace m_attackTracers[MELEE_MAX_ATTACKMAP_TRACES];

	// Attack tracer
	TraceResult m_trHit;

	// Entities hit during current attack
	std::vector<edict_t *> m_hitEntities;

private:
	bool IsEntityAlreadyHit( edict_t *pEntity, const std::vector<edict_t *> &hitEntities );
};

class CWeaponBaseMelee : public CWeaponBase, public IMeleeBaseShared
{
	DECLARE_CLASS_SIMPLE( CWeaponBaseMelee, CWeaponBase );
	typedef IMeleeBaseShared MeleeBase;

public:
	bool IsMeleeWeapon() override { return true; }
	virtual bool CanUseHeavyAttack() const { return false; }
	void Spawn( void );
	virtual void Precache( void ) {}
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle();

	// Default deploy and holster animations, make sure you override these.
	virtual float Deploy( void );
	virtual float DoHolsterAnimation();
	virtual float DoWeaponIdleAnimation( int iAnim );
	virtual float GetAttackAnimationTime( int iAnim ) { return GetAnimationTime( 24, 30 ); }
};

#endif