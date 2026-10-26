// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_melee_walkstick.h"
#ifndef CLIENT_DLL
#include "game.h"
#include "gamerules.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_walkstick, CWeaponMeleeWalkStick );
PRECACHE_WEAPON_REGISTER( weapon_walkstick );

void CWeaponMeleeWalkStick::Precache( void )
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL( "models/v_walkstick.mdl" );
	PRECACHE_MODEL( "models/w_walkstick.mdl" );
	PRECACHE_MODEL( "models/p_walkstick.mdl" );
	PRECACHE_SOUND( "weapons/melee/walkstick/hit1.wav" );
	PRECACHE_SOUND( "weapons/melee/walkstick/hit2.wav" );
	PRECACHE_SOUND( "weapons/melee/walkstick/hitbod1.wav" );
	PRECACHE_SOUND( "weapons/melee/walkstick/hitbod2.wav" );
	PRECACHE_SOUND( "weapons/melee/walkstick/hitbod3.wav" );
}

void CWeaponMeleeWalkStick::DoWeaponSoundFromAttack( MeleeAttackType attackType, bool bHitWorld )
{
	const char *szSoundFile = nullptr;
	if ( bHitWorld )
	{
		switch ( RANDOM_LONG( 0, 1 ) )
		{
			case 0: szSoundFile = "weapons/melee/walkstick/hit1.wav"; break;
			case 1: szSoundFile = "weapons/melee/walkstick/hit1.wav"; break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0: szSoundFile = "weapons/melee/walkstick/hitbod1.wav"; break;
			case 1: szSoundFile = "weapons/melee/walkstick/hitbod2.wav"; break;
			case 2: szSoundFile = "weapons/melee/walkstick/hitbod3.wav"; break;
		}
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

void CWeaponMeleeWalkStick::DoWeaponSoundFromMiss( MeleeAttackType attackType )
{
	EmitWeaponSound( "weapons/melee/crowbar/miss.wav", CHAN_ITEM, 1, ATTN_NORM );
}

float CWeaponMeleeWalkStick::Deploy()
{
	BaseClass::Deploy();
	return GetAnimationTime( 24, 50 );
}

float CWeaponMeleeWalkStick::DoHolsterAnimation()
{
	BaseClass::DoHolsterAnimation();
	return GetAnimationTime( 15, 50 );
}

float CWeaponMeleeWalkStick::DoWeaponIdleAnimation( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_MELEE_IDLE1: return GetAnimationTime( 33, 12 );
		case ANIM_MELEE_IDLE2: return GetAnimationTime( 30, 12 );
	    case ANIM_MELEE_IDLE3: return GetAnimationTime( 80, 12 );
	}
	return 1.0f;
}

float CWeaponMeleeWalkStick::GetAttackAnimationTime( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_MELEE_ATTACK1MISS: return GetAnimationTime( 16, 30 );
		case ANIM_MELEE_ATTACK2MISS: return GetAnimationTime( 20, 30 );
		case ANIM_MELEE_ATTACK3MISS: return GetAnimationTime( 18, 30 );
	    case ANIM_MELEE_ATTACK1HIT: return GetAnimationTime( 9, 30 );
	    case ANIM_MELEE_ATTACK2HIT: return GetAnimationTime( 13, 30 );
	    case ANIM_MELEE_ATTACK3HIT: return GetAnimationTime( 14, 30 );
	}
	return 1.0f;
}
