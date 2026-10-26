// ============== Copyright (c) 2026 Monochrome Games ============== \\

#include "weapon_misc_nailgun.h"
#include "projectiles/CProjectileNail.h"

#define BOLT_AIR_VELOCITY   2000
#define BOLT_WATER_VELOCITY 1000

LINK_ENTITY_TO_CLASS( weapon_nailgun, CWeaponNailGun );
PRECACHE_WEAPON_REGISTER( weapon_nailgun );

float CWeaponNailGun::DoHolsterAnimation()
{
	SendWeaponAnim( ANIM_BARRICADE_NAILGUN_HOLSTER );
	return GetAnimationTime( 14, 30 );
}

void CWeaponNailGun::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_barricade_nailgun.mdl");
	DefaultSpawn();
}

void CWeaponNailGun::Precache(void)
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL("models/v_barricade.mdl");
	PRECACHE_MODEL("models/w_barricade.mdl");
	PRECACHE_MODEL("models/w_barricade_nailgun.mdl");
	PRECACHE_MODEL("models/p_barricade.mdl");

	PRECACHE_MODEL("models/proj_nail.mdl");

	PRECACHE_SOUND("items/ammo_pickup.wav");

	PRECACHE_SOUND("weapons/nailgun/nailgun_dryfire.wav");
	PRECACHE_SOUND("weapons/nailgun/nailgun_fire.wav");
	PRECACHE_SOUND("weapons/nailgun/nailgun_use1.wav");
	PRECACHE_SOUND("weapons/nailgun/nailgun_use2.wav");
	PRECACHE_SOUND("weapons/nailgun/nailgun_use3.wav");
	PRECACHE_SOUND("weapons/nailgun/proj_hitbody.wav");
	PRECACHE_SOUND("weapons/nailgun/proj_hit.wav");
	PRECACHE_SOUND("weapons/nailgun/proj_hit2.wav");
	PRECACHE_SOUND("weapons/nailgun/proj_hit3.wav");

	m_nEventPrimary = PRECACHE_EVENT(1, "events/nailgun.sc");
}

int CWeaponNailGun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( BaseClass::AddToPlayer( pPlayer ) )
	{
		BaseClass::SendWeaponPickup( pPlayer );
		return TRUE;
	}
	return FALSE;
}

float CWeaponNailGun::Deploy()
{
	DoDeploy( "models/v_barricade.mdl", "models/p_barricade.mdl", ANIM_BARRICADE_NAILGUN_DRAW, "onehanded" );
	return GetAnimationTime( 31, 30 );
}

void CWeaponNailGun::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetWeaponTimerBase() + 0.15f;
		return;
	}

	if ( m_iClip <= 0 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetWeaponTimerBase() + 0.15f;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	SendWeaponAnim( ANIM_BARRICADE_NAILGUN_FIRE );

	EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/nailgun/nailgun_fire.wav", 1.0, ATTN_NORM );

#if !defined( CLIENT_DLL )
	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );

	anglesAim.x = -anglesAim.x;
	Vector vecSrc = m_pPlayer->GetGunPosition() - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;

	CProjectileNail *pBolt = CreateNailProjectile();
	pBolt->pev->origin = vecSrc;
	pBolt->pev->angles = anglesAim;
	pBolt->pev->owner = m_pPlayer->edict();

	float flBoltVelocity = ( m_pPlayer->pev->waterlevel == 3 ) ? BOLT_WATER_VELOCITY : BOLT_AIR_VELOCITY;
	pBolt->pev->velocity = vecDir * flBoltVelocity;
	pBolt->pev->speed = flBoltVelocity;
	pBolt->pev->avelocity.z = 10;
#endif

	PLAYBACK_EVENT_FULL(0, m_pPlayer->edict(), m_nEventPrimary, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetWeaponTimerBase() + PrimaryFireRate();

	if (m_flNextPrimaryAttack < GetWeaponTimerBase())
		m_flNextPrimaryAttack = GetWeaponTimerBase() + PrimaryFireRate();

	m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime( 13, 30 );
}


void CWeaponNailGun::SecondaryAttack()
{
	// Secondary does nothing
}


void CWeaponNailGun::WeaponIdle( void )
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	if ( m_flTimeWeaponIdle > GetWeaponTimerBase() )
		return;

	float flTime;
	int iAnim;
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		iAnim = ANIM_BARRICADE_NAILGUN_IDLE1;
		flTime = GetAnimationTime( 36, 30 );
		break;

	default:
	case 1:
		iAnim = ANIM_BARRICADE_NAILGUN_IDLE2;
		flTime = GetAnimationTime( 50, 30 );
		break;

	case 2:
		iAnim = ANIM_BARRICADE_NAILGUN_IDLE3;
		flTime = GetAnimationTime( 40, 30 );
		break;
	}

	SendWeaponAnim( iAnim );
	m_flTimeWeaponIdle = GetWeaponTimerBase() + flTime;
}
