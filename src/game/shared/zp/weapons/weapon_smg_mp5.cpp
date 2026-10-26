// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_smg_mp5.h"

LINK_ENTITY_TO_CLASS( weapon_mp5, CWeaponSMGMP5 );
PRECACHE_WEAPON_REGISTER( weapon_mp5 );

float CWeaponSMGMP5::DoHolsterAnimation()
{
	SendWeaponAnim( ANIM_MP5_HOLSTER );
	return GetAnimationTime( 11, 35 );
}

float CWeaponSMGMP5::DoWeaponUnload()
{
	SendWeaponAnim( ANIM_MP5_UNLOAD );
	AddWeaponSound( "weapons/mp5/magout.wav", 1, ATTN_NORM, GetAnimationTime( 5, 25 ) );
	AddWeaponSound( "weapons/mp5/magin.wav", 1, ATTN_NORM, GetAnimationTime( 32, 25 ) );
	AddWeaponSound( "weapons/mp5/slide_forward.wav", 1, ATTN_NORM, GetAnimationTime( 39, 25 ) );
	return GetAnimationTime( 70, 25 );
}

void CWeaponSMGMP5::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_mp5.mdl");
	DefaultSpawn();
}

void CWeaponSMGMP5::Precache(void)
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL("models/v_mp5.mdl");
	PRECACHE_MODEL("models/w_mp5.mdl");
	PRECACHE_MODEL("models/p_mp5.mdl");

	PRECACHE_MODEL("models/shell.mdl"); // brass shellTE_MODEL

	PRECACHE_SOUND("items/ammo_pickup.wav");

	PRECACHE_SOUND("weapons/mp5/dryfire.wav");
	PRECACHE_SOUND("weapons/mp5/fire.wav");
	PRECACHE_SOUND("weapons/mp5/magout.wav");
	PRECACHE_SOUND("weapons/mp5/magin.wav");
	PRECACHE_SOUND("weapons/mp5/slide_back.wav");
	PRECACHE_SOUND("weapons/mp5/slide_forward.wav");

	m_nEventPrimary = PRECACHE_EVENT(1, "events/mp5.sc");
}

int CWeaponSMGMP5::AddToPlayer(CBasePlayer *pPlayer)
{
	if (BaseClass::AddToPlayer(pPlayer))
	{
		BaseClass::SendWeaponPickup(pPlayer);
		return TRUE;
	}
	return FALSE;
}

float CWeaponSMGMP5::Deploy()
{
	DoDeploy( "models/v_mp5.mdl", "models/p_mp5.mdl", ANIM_MP5_DEPLOY, "mp5" );
	return GetAnimationTime( 21, 60 );
}

void CWeaponSMGMP5::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);
	Vector vecDir;

	vecDir = m_pPlayer->FireBulletsPlayer(iBullets(), vecSrc, vecAiming, GetSpreadVector(PrimaryWeaponSpread()), 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed);

	int flags;
#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_nEventPrimary, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + PrimaryFireRate();

	if (m_flNextPrimaryAttack < UTIL_WeaponTimeBase())
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + PrimaryFireRate();

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GetAnimationTime( 13, 30 );
}


void CWeaponSMGMP5::Reload(void)
{
	if ( m_pPlayer->ammo_9mm <= 0 ) return;
	float flDelay = GetAnimationTime( IsEmpty() ? 77 : 48, 20 );
	int iRet = DefaultReload( IsEmpty() ? ANIM_MP5_RELOAD_EMPTY : ANIM_MP5_RELOAD, flDelay );
	if ( !iRet ) return;
	if ( IsEmpty() )
	{
		AddWeaponSound( "weapons/mp5/slide_back.wav", 1, ATTN_NORM, GetAnimationTime( 4, 20 ) );
		AddWeaponSound( "weapons/mp5/magout.wav", 1, ATTN_NORM, GetAnimationTime( 18, 20 ) );
		AddWeaponSound( "weapons/mp5/magin.wav", 1, ATTN_NORM, GetAnimationTime( 38, 20 ) );
		AddWeaponSound( "weapons/mp5/slide_forward.wav", 1, ATTN_NORM, GetAnimationTime( 51, 20 ) );
	}
	else
	{
		AddWeaponSound( "weapons/mp5/magout.wav", 1, ATTN_NORM, GetAnimationTime( 4, 20 ) );
		AddWeaponSound( "weapons/mp5/magin.wav", 1, ATTN_NORM, GetAnimationTime( 24, 20 ) );
	}
}

void CWeaponSMGMP5::WeaponIdle(void)
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	int iAnim;
	float flDelay;
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		iAnim = ANIM_MP5_LONGIDLE;
		flDelay = GetAnimationTime( 25, 12 );
		break;

	default:
	case 1:
		iAnim = ANIM_MP5_IDLE1;
		flDelay = GetAnimationTime( 29, 5 );
		break;
	}

	SendWeaponAnim(iAnim);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flDelay;
}
