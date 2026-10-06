// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_sidearm_fafo.h"

LINK_ENTITY_TO_CLASS(weapon_fafo, CWeaponSideArmFafo);
PRECACHE_WEAPON_REGISTER( weapon_fafo );


float CWeaponSideArmFafo::DoHolsterAnimation()
{
	if ( m_iClip == 0 )
	{
		DeleteMe();
		return 0.0f;
	}
	SendWeaponAnim( ANIM_PISTOL_HOLSTER );
	return GetAnimationTime( 24, 50 );
}

void CWeaponSideArmFafo::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_1911.mdl");
	DefaultSpawn();
}

void CWeaponSideArmFafo::Precache(void)
{
	PRECACHE_MODEL("models/v_1911.mdl");
	PRECACHE_MODEL("models/w_1911.mdl");
	PRECACHE_MODEL("models/p_1911.mdl");

	PRECACHE_MODEL("models/shell.mdl"); // brass shell

	PRECACHE_SOUND("items/ammo_pickup.wav");

	PRECACHE_SOUND("weapons/sig/dryfire.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/fire.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/clipin.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/clipout.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/clipout_unload.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/slideforward.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/slideback.wav"); //handgun

	m_nEventPrimary = PRECACHE_EVENT(1, "events/fafo.sc");
}

int CWeaponSideArmFafo::AddToPlayer(CBasePlayer *pPlayer)
{
	if (BaseClass::AddToPlayer(pPlayer))
	{
		BaseClass::SendWeaponPickup(pPlayer);
		return TRUE;
	}
	return FALSE;
}

float CWeaponSideArmFafo::Deploy()
{
	DoDeploy( "models/v_1911.mdl", "models/p_1911.mdl", ANIM_PISTOL_DRAW, "onehanded" );
	return GetAnimationTime( 26, 30 );
}

void CWeaponSideArmFafo::PrimaryAttack(void)
{
	if ( m_iClip == 0 ) return;

	m_iClip = 0;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;

#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	// Destroyed
	pev->body = 1;

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming;

	// Hurt the player
#ifndef CLIENT_DLL
	Vector vecSpot = vecSrc + Vector( 0, 0, -8 );
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSpot );
	WRITE_BYTE( TE_EXPLOSION ); // This makes a dynamic light and the explosion sprites/sound
	WRITE_COORD( vecSpot.x ); // Send to PAS because of the sound
	WRITE_COORD( vecSpot.y );
	WRITE_COORD( vecSpot.z );
	WRITE_SHORT( g_sModelIndexFireball );
	WRITE_BYTE( 2 ); // scale * 10
	WRITE_BYTE( 15 ); // framerate
	WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();
	if ( m_pPlayer->pev->team == ZP::TEAM_ZOMBIE )
		m_pPlayer->TakeDamage( m_pPlayer->pev, m_pPlayer->pev, m_pPlayer->GetMaxHealth(), DMG_BLAST | DMG_ALWAYSGIB );
	else
		m_pPlayer->TakeDamage( m_pPlayer->pev, m_pPlayer->pev, 5, DMG_BLAST );
#endif

	vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	Vector vecDir;
	vecDir = m_pPlayer->FireBulletsPlayer(iBullets(), vecSrc, vecAiming, Vector(PrimaryWeaponSpread(), PrimaryWeaponSpread(), PrimaryWeaponSpread()), 8192, BULLET_PLAYER_FAFO, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed);

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_nEventPrimary, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, IsEmpty() ? 1 : 0, 0);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + PrimaryFireRate();

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GetAnimationTime( 24, 50 );
}

void CWeaponSideArmFafo::WeaponIdle(void)
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_iClip == 0 )
	{
		DeleteMe();
		return;
	}

	float flTime;
	int iAnim;
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		iAnim = ANIM_PISTOL_IDLE1;
		flTime = GetAnimationTime( 41, 8 );
		break;

	default:
	case 1:
		iAnim = ANIM_PISTOL_IDLE2;
		flTime = GetAnimationTime( 41, 10 );
		break;

	case 2:
		iAnim = ANIM_PISTOL_IDLE3;
		flTime = GetAnimationTime( 41, 20 );
		break;
	}

	SendWeaponAnim(iAnim);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flTime;
}

void CWeaponSideArmFafo::DeleteMe()
{
#ifndef CLIENT_DLL
	if ( m_pPlayer )
		m_pPlayer->WeaponSlotSet( this, false );
#endif
	DestroyItem();
}
