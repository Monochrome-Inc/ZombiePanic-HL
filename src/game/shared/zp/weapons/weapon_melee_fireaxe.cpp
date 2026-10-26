// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_melee_fireaxe.h"
#ifndef CLIENT_DLL
#include "game.h"
#include "gamerules.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_fireaxe, CWeaponMeleeFireaxe );
PRECACHE_WEAPON_REGISTER( weapon_fireaxe );

void CWeaponMeleeFireaxe::Precache( void )
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL("models/v_fireaxe.mdl");
	PRECACHE_MODEL("models/w_fireaxe.mdl");
	PRECACHE_MODEL("models/p_fireaxe.mdl");
	PRECACHE_SOUND("weapons/melee/fireaxe/hit1.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hit2.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hitbod1.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hitbod2.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hitbod3.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hit1_heavy.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hit2_heavy.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hitbod1_heavy.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hitbod2_heavy.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/hitbod3_heavy.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/miss1.wav");
	PRECACHE_SOUND("weapons/melee/fireaxe/miss2.wav");
}

void CWeaponMeleeFireaxe::DoWeaponSoundFromAttack( MeleeAttackType attackType, bool bHitWorld )
{
	const char *szSoundFile = nullptr;
	if ( bHitWorld )
	{
		if ( attackType == MELEE_ATTACK_HEAVY )
		{
			switch ( RANDOM_LONG( 0, 1 ) )
			{
				case 0: szSoundFile = "weapons/melee/fireaxe/hit1_heavy.wav"; break;
				case 1: szSoundFile = "weapons/melee/fireaxe/hit2_heavy.wav"; break;
			}
		}
		else
		{
			switch ( RANDOM_LONG( 0, 1 ) )
			{
				case 0: szSoundFile = "weapons/melee/fireaxe/hit1.wav"; break;
				case 1: szSoundFile = "weapons/melee/fireaxe/hit1.wav"; break;
			}
		}
	}
	else
	{
		if (attackType == MELEE_ATTACK_HEAVY)
		{
			switch ( RANDOM_LONG( 0, 2 ) )
			{
				case 0: szSoundFile = "weapons/melee/fireaxe/hitbod1_heavy.wav"; break;
				case 1: szSoundFile = "weapons/melee/fireaxe/hitbod2_heavy.wav"; break;
				case 2: szSoundFile = "weapons/melee/fireaxe/hitbod3_heavy.wav"; break;
			}
		}
		else
		{
			switch ( RANDOM_LONG( 0, 2 ) )
			{
				case 0: szSoundFile = "weapons/melee/fireaxe/hitbod1.wav"; break;
				case 1: szSoundFile = "weapons/melee/fireaxe/hitbod2.wav"; break;
				case 2: szSoundFile = "weapons/melee/fireaxe/hitbod3.wav"; break;
			}
		}
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

void CWeaponMeleeFireaxe::DoWeaponSoundFromMiss( MeleeAttackType attackType )
{
	const char *szSoundFile = nullptr;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
		case 0: szSoundFile = "weapons/melee/fireaxe/miss1.wav"; break;
		case 1: szSoundFile = "weapons/melee/fireaxe/miss2.wav"; break;
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

float CWeaponMeleeFireaxe::Deploy()
{
	BaseClass::Deploy();
	return GetAnimationTime( 25, 30 );
}

float CWeaponMeleeFireaxe::DoHolsterAnimation()
{
	BaseClass::DoHolsterAnimation();
	return GetAnimationTime( 17, 40 );
}

float CWeaponMeleeFireaxe::DoWeaponIdleAnimation( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_MELEE_IDLE1: return GetAnimationTime( 26, 30 );
		case ANIM_MELEE_IDLE2: return GetAnimationTime( 70, 30 );
	    case ANIM_MELEE_IDLE3: return GetAnimationTime( 70, 15 );
	}
	return 1.0f;
}

float CWeaponMeleeFireaxe::GetAttackAnimationTime( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_MELEE_HEAVY_HIT: return GetAnimationTime( 15, 22 );
		case ANIM_MELEE_HEAVY_MISS: return GetAnimationTime( 15, 22 );
		case ANIM_MELEE_ATTACK1MISS: return GetAnimationTime( 20, 35 );
		case ANIM_MELEE_ATTACK2MISS: return GetAnimationTime( 21, 30 );
		case ANIM_MELEE_ATTACK3MISS: return GetAnimationTime( 21, 33 );
	    case ANIM_MELEE_ATTACK1HIT: return GetAnimationTime( 20, 35 );
	    case ANIM_MELEE_ATTACK2HIT: return GetAnimationTime( 21, 35 );
	    case ANIM_MELEE_ATTACK3HIT: return GetAnimationTime( 21, 38 );
	}
	return 1.0f;
}
