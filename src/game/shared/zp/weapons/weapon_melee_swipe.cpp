// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_melee_swipe.h"
#ifndef CLIENT_DLL
#include "game.h"
#include "gamerules.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_swipe, CWeaponMeleeSwipe );
PRECACHE_WEAPON_REGISTER( weapon_swipe );

void CWeaponMeleeSwipe::Spawn(void)
{
	BaseClass::Spawn();
	pev->team = ZP::TEAM_ZOMBIE;
}

void CWeaponMeleeSwipe::Precache(void)
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL("models/v_swipe.mdl");
	PRECACHE_MODEL("models/w_swipe.mdl");
	PRECACHE_MODEL("models/p_swipe.mdl");
	PRECACHE_SOUND("weapons/melee/zarm/strike1.wav");
	PRECACHE_SOUND("weapons/melee/zarm/strike2.wav");
	PRECACHE_SOUND("weapons/melee/zarm/strike3.wav");
	PRECACHE_SOUND("weapons/melee/zarm/miss1.wav");
	PRECACHE_SOUND("weapons/melee/zarm/miss2.wav");
}

void CWeaponMeleeSwipe::DoWeaponSoundFromAttack( MeleeAttackType attackType, bool bHitWorld )
{
	const char *szSoundFile = nullptr;
	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0: szSoundFile = "weapons/melee/zarm/strike1.wav"; break;
		case 1: szSoundFile = "weapons/melee/zarm/strike2.wav"; break;
		case 2: szSoundFile = "weapons/melee/zarm/strike3.wav"; break;
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

void CWeaponMeleeSwipe::DoWeaponSoundFromMiss( MeleeAttackType attackType )
{
	const char *szSoundFile = nullptr;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
		case 0: szSoundFile = "weapons/melee/zarm/miss1.wav"; break;
		case 1: szSoundFile = "weapons/melee/zarm/miss2.wav"; break;
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

float CWeaponMeleeSwipe::Deploy()
{
	BaseClass::Deploy();
	return GetAnimationTime( 31, 35 );
}

float CWeaponMeleeSwipe::DoHolsterAnimation()
{
	BaseClass::DoHolsterAnimation();
	return GetAnimationTime( 11, 35 );
}

float CWeaponMeleeSwipe::DoWeaponIdleAnimation( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_MELEE_IDLE1: return GetAnimationTime( 30, 10 );
		case ANIM_MELEE_IDLE2: return GetAnimationTime( 15, 15 );
	    case ANIM_MELEE_IDLE3: return GetAnimationTime( 16, 15 );
	}
	return 1.0f;
}

float CWeaponMeleeSwipe::GetAttackAnimationTime( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_MELEE_ATTACK1MISS: return GetAnimationTime( 26, 25 );
		case ANIM_MELEE_ATTACK2MISS: return GetAnimationTime( 21, 32 );
		case ANIM_MELEE_ATTACK3MISS: return GetAnimationTime( 16, 22 );
	    case ANIM_MELEE_ATTACK1HIT: return GetAnimationTime( 26, 25 );
	    case ANIM_MELEE_ATTACK2HIT: return GetAnimationTime( 21, 32 );
	    case ANIM_MELEE_ATTACK3HIT: return GetAnimationTime( 16, 22 );
	}
	return 1.0f;
}
