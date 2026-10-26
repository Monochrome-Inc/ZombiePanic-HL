// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_explosive_molotov.h"
#include "gamerules.h"

LINK_ENTITY_TO_CLASS( weapon_molotov, CWeaponExplosiveMolotov );
PRECACHE_WEAPON_REGISTER( weapon_molotov );

void CWeaponExplosiveMolotov::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_molotov.mdl");
	DefaultSpawn();
}

int CWeaponExplosiveMolotov::AddToPlayer(CBasePlayer *pPlayer)
{
	if ( BaseClass::AddToPlayer( pPlayer ) )
	{
		BaseClass::SendWeaponPickup( pPlayer );
		return TRUE;
	}
	return FALSE;
}

int CWeaponExplosiveMolotov::AddDuplicate( CBasePlayerItem *pOriginal )
{
	CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>( pOriginal );
	if ( pWeapon->m_iClip < pWeapon->iMaxClip() )
	{
		pWeapon->m_iClip += 1;
		EMIT_SOUND( ENT(pev), CHAN_ITEM, "items/ammo_pickup.wav", 1, ATTN_NORM );
		return TRUE;
	}
	return FALSE;
}

void CWeaponExplosiveMolotov::Precache(void)
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL("sprites/flame1.spr");
	PRECACHE_MODEL("sprites/flame2.spr");
	PRECACHE_MODEL("models/w_molotov.mdl");
	PRECACHE_MODEL("models/w_molotov_thrown.mdl");
	PRECACHE_MODEL("models/v_molotov.mdl");
	PRECACHE_MODEL("models/p_molotov.mdl");
	PRECACHE_SOUND("weapons/molotov/fuse.wav");
	PRECACHE_SOUND("weapons/molotov/break.wav");
	PRECACHE_SOUND("weapons/molotov/burning.wav");
	PRECACHE_SOUND("weapons/lighter01.wav");
	PRECACHE_SOUND("weapons/lighter02.wav");
}

float CWeaponExplosiveMolotov::Deploy()
{
	DoDeploy( "models/v_molotov.mdl", "models/p_molotov.mdl", ANIM_THROW_EXPLOSIVES_DRAW, "crowbar" );
	return GetAnimationTime( 26, 30 );
}

BOOL CWeaponExplosiveMolotov::CanHolster(void)
{
	// If not primed, we can holster
	if ( (m_flStartThrow == 0) ) return true;
	// It's primed, only allow this after we played the animation.
	return (m_flCanHolster - gpGlobals->time <= 0);
}

float CWeaponExplosiveMolotov::DoHolsterAnimation()
{
	// We threw it, and got nothing left.
	if ( m_flReleaseThrow > 0 && !m_iClip )
	{
#ifndef CLIENT_DLL
		DestroyAndSwitchToNextBestWeapon();
		return 0.0f;
#endif
		DestroyItem();
		return 0.0f;
	}
	SendWeaponAnim( ANIM_THROW_EXPLOSIVES_HOLSTER );
	return GetAnimationTime( 8, 20 );
}

void CWeaponExplosiveMolotov::PrimaryAttack()
{
	if (!m_flStartThrow && m_iClip > 0)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		AddWeaponSound( "weapons/lighter01.wav", 1, ATTN_NORM, GetAnimationTime( 2, 25 ) );
		AddWeaponSound( "weapons/molotov/fuse.wav", 1, ATTN_NORM, GetAnimationTime( 24, 25 ) );
		AddWeaponSound( "weapons/lighter02.wav", 1, ATTN_NORM, GetAnimationTime( 25, 25 ) );
		SendWeaponAnim( ANIM_THROW_EXPLOSIVES_PINPULL );
		m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime( 34, 25 );
		m_flCanHolster = gpGlobals->time + 2.0f;
	}
}

void CWeaponExplosiveMolotov::SecondaryAttack()
{
	if (!m_flStartThrow && m_iClip > 0)
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		AddWeaponSound( "weapons/lighter01.wav", 1, ATTN_NORM, GetAnimationTime( 4, 25 ) );
		AddWeaponSound( "weapons/molotov/fuse.wav", 1, ATTN_NORM, GetAnimationTime( 24, 25 ) );
		AddWeaponSound( "weapons/lighter02.wav", 1, ATTN_NORM, GetAnimationTime( 24, 25 ) );
		SendWeaponAnim( ANIM_THROW_EXPLOSIVES_PINPULL2 );
		m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime( 35, 25 );
		m_flCanHolster = gpGlobals->time + 2.0f;
		m_bDoSecondaryAttack = true;
	}
}

void CWeaponExplosiveMolotov::WeaponIdle(void)
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		m_flReleaseThrow = gpGlobals->time;

	if ( m_flTimeWeaponIdle > GetWeaponTimerBase() )
		return;

	if (m_flStartThrow)
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if (angThrow.x < 0)
			angThrow.x = -10 + angThrow.x * ((90 - 10) / 90.0);
		else
			angThrow.x = -10 + angThrow.x * ((90 + 10) / 90.0);

		float flVel = m_bDoSecondaryAttack ? 280 : 800;

		UTIL_MakeVectors(angThrow);

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;
		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		CGrenade::ShootContact( m_pPlayer->pev, vecSrc, vecThrow, "models/w_molotov_thrown.mdl", CGrenade::CONTACT_TYPE::TYPE_MOLOTOV );

		SendWeaponAnim( m_bDoSecondaryAttack ? ANIM_THROW_EXPLOSIVES_THROW2 : ANIM_THROW_EXPLOSIVES_THROW );

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		float flTime = m_bDoSecondaryAttack ? GetAnimationTime( 13, 30 ) : GetAnimationTime( 6, 10 );
		m_flReleaseThrow = 1;
		m_flStartThrow = 0;
		m_flTimeWeaponIdle = GetWeaponTimerBase() + flTime;
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle + 0.1f;
		m_bDoSecondaryAttack = false;

		m_iClip--;
		return;
	}
	else if (m_flReleaseThrow > 0)
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( m_iClip > 0 )
			DoDeployAnimation();
		else
		{
#ifndef CLIENT_DLL
			DestroyAndSwitchToNextBestWeapon();
			return;
#endif
			DestroyItem();
			return;
		}

		m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetWeaponTimerBase() + GetAnimationTime( 26, 30 );
		m_flReleaseThrow = -1;
		return;
	}

	if (m_iClip)
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);
		if (flRand <= 0.75)
		{
			iAnim = ANIM_THROW_EXPLOSIVES_IDLE;
			m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime( 50, 6 );
		}
		else
		{
			iAnim = ANIM_THROW_EXPLOSIVES_FIDGET;
			m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime( 117, 16 );
		}

		SendWeaponAnim(iAnim);
	}
}
