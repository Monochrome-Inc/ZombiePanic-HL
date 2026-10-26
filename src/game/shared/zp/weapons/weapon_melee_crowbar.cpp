// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_melee_crowbar.h"
#ifndef CLIENT_DLL
#include "game.h"
#include "gamerules.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_crowbar, CWeaponMeleeCrowbar );
PRECACHE_WEAPON_REGISTER( weapon_crowbar );

void CWeaponMeleeCrowbar::Precache( void )
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL( "models/v_crowbar.mdl" );
	PRECACHE_MODEL( "models/w_crowbar.mdl" );
	PRECACHE_MODEL( "models/p_crowbar.mdl" );
	PRECACHE_SOUND( "weapons/melee/crowbar/hit1.wav" );
	PRECACHE_SOUND( "weapons/melee/crowbar/hit2.wav" );
	PRECACHE_SOUND( "weapons/melee/crowbar/hitbod1.wav" );
	PRECACHE_SOUND( "weapons/melee/crowbar/hitbod2.wav" );
	PRECACHE_SOUND( "weapons/melee/crowbar/hitbod3.wav" );
	PRECACHE_SOUND( "weapons/melee/crowbar/miss.wav" );
}

void CWeaponMeleeCrowbar::DoWeaponSoundFromAttack( MeleeAttackType attackType, bool bHitWorld )
{
	const char *szSoundFile = nullptr;
	if ( bHitWorld )
	{
		switch ( RANDOM_LONG( 0, 1 ) )
		{
			case 0: szSoundFile = "weapons/melee/crowbar/hit1.wav"; break;
			case 1: szSoundFile = "weapons/melee/crowbar/hit1.wav"; break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0: szSoundFile = "weapons/melee/crowbar/hitbod1.wav"; break;
			case 1: szSoundFile = "weapons/melee/crowbar/hitbod2.wav"; break;
			case 2: szSoundFile = "weapons/melee/crowbar/hitbod3.wav"; break;
		}
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

void CWeaponMeleeCrowbar::DoWeaponSoundFromMiss( MeleeAttackType attackType )
{
	EmitWeaponSound( "weapons/melee/crowbar/miss.wav", CHAN_ITEM, 1, ATTN_NORM );
}

float CWeaponMeleeCrowbar::Deploy()
{
	BaseClass::Deploy();
	return GetAnimationTime( 24, 50 );
}

float CWeaponMeleeCrowbar::DoHolsterAnimation()
{
	BaseClass::DoHolsterAnimation();
	return GetAnimationTime( 15, 50 );
}

float CWeaponMeleeCrowbar::DoWeaponIdleAnimation( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_MELEE_IDLE1: return GetAnimationTime( 33, 12 );
		case ANIM_MELEE_IDLE2: return GetAnimationTime( 30, 12 );
	    case ANIM_MELEE_IDLE3: return GetAnimationTime( 80, 12 );
	}
	return 1.0f;
}

float CWeaponMeleeCrowbar::GetAttackAnimationTime( int iAnim )
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
