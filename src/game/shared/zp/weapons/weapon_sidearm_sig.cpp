// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_sidearm_sig.h"

LINK_ENTITY_TO_CLASS(weapon_sig, CWeaponSideArmSig);
LINK_ENTITY_TO_CLASS(weapon_9mmhandgun, CWeaponSideArmSig); // Only used by old custom maps, don't remove this
PRECACHE_WEAPON_REGISTER( weapon_sig );


float CWeaponSideArmSig::DoHolsterAnimation()
{
	SendWeaponAnim( IsEmpty() ? ANIM_PISTOL_HOLSTER_EMPTY : ANIM_PISTOL_HOLSTER );
	return GetAnimationTime( 24, 60 );
}

float CWeaponSideArmSig::DoWeaponUnload()
{
	SendWeaponAnim( ANIM_PISTOL_UNLOAD );
	AddWeaponSound( "weapons/sig/clipout_unload.wav", 1, ATTN_NORM, GetAnimationTime( 8, 32 ) );
	AddWeaponSound( "weapons/sig/clipin.wav", 1, ATTN_NORM, GetAnimationTime( 34, 32 ) );
	AddWeaponSound( "weapons/sig/slideback.wav", 1, ATTN_NORM, GetAnimationTime( 53, 32 ) );
	return GetAnimationTime( 87, 32 );
}

void CWeaponSideArmSig::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_sig" );
	Precache();
	SET_MODEL(ENT(pev), "models/w_9mmhandgun.mdl");
	DefaultSpawn();
}

void CWeaponSideArmSig::Precache(void)
{
	PRECACHE_MODEL("models/v_9mmhandgun.mdl");
	PRECACHE_MODEL("models/w_9mmhandgun.mdl");
	PRECACHE_MODEL("models/p_9mmhandgun.mdl");

	PRECACHE_MODEL("models/shell.mdl"); // brass shell

	PRECACHE_SOUND("items/ammo_pickup.wav");

	PRECACHE_SOUND("weapons/sig/dryfire.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/fire.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/clipin.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/clipout.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/clipout_unload.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/slideforward.wav"); //handgun
	PRECACHE_SOUND("weapons/sig/slideback.wav"); //handgun

	m_nEventPrimary = PRECACHE_EVENT(1, "events/sig.sc");
}

int CWeaponSideArmSig::AddToPlayer(CBasePlayer *pPlayer)
{
	if (BaseClass::AddToPlayer(pPlayer))
	{
		BaseClass::SendWeaponPickup(pPlayer);
		return TRUE;
	}
	return FALSE;
}

float CWeaponSideArmSig::Deploy()
{
	DoDeploy( "models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", IsEmpty() ? ANIM_PISTOL_DRAW_EMPTY : ANIM_PISTOL_DRAW, "onehanded" );
	return GetAnimationTime( 26, 40 );
}

void CWeaponSideArmSig::PrimaryAttack(void)
{
	if ( IsEmpty() )
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2;
		}
		return;
	}

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;

#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	// silenced
	if (pev->body == 1)
	{
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	}
	else
	{
		// non-silenced
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	}

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming;

	vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	Vector vecDir;
	vecDir = m_pPlayer->FireBulletsPlayer(iBullets(), vecSrc, vecAiming, Vector(PrimaryWeaponSpread(), PrimaryWeaponSpread(), PrimaryWeaponSpread()), 8192, BULLET_PLAYER_SIG, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed);

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_nEventPrimary, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, IsEmpty() ? 1 : 0, 0);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + PrimaryFireRate();

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GetAnimationTime( 14, 30 );
}

void CWeaponSideArmSig::Reload(void)
{
	if (m_pPlayer->ammo_9mm <= 0)
		return;
	int nFrame = IsEmpty() ? 64 : 60;
	int nFPS = IsEmpty() ? 30 : 38;

	int iResult = DefaultReload( IsEmpty() ? ANIM_PISTOL_RELOAD_EMPTY : ANIM_PISTOL_RELOAD, GetAnimationTime( nFrame, nFPS ) );
	if ( iResult )
	{
		AddWeaponSound( "weapons/sig/clipout.wav", 1, ATTN_NORM, GetAnimationTime( 8, nFPS ) );
		AddWeaponSound( "weapons/sig/clipin.wav", 1, ATTN_NORM, GetAnimationTime( 34, nFPS ) );
		if ( IsEmpty() )
			AddWeaponSound( "weapons/sig/slideforward.wav", 1, ATTN_NORM, GetAnimationTime( 43, nFPS ) );
	}
}

void CWeaponSideArmSig::WeaponIdle(void)
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	float flTime;
	int iAnim;
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		iAnim = IsEmpty() ? ANIM_PISTOL_IDLE1_EMPTY : ANIM_PISTOL_IDLE1;
		flTime = GetAnimationTime( 41, 10 );
		break;

	default:
	case 1:
		iAnim = IsEmpty() ? ANIM_PISTOL_IDLE2_EMPTY : ANIM_PISTOL_IDLE2;
		flTime = GetAnimationTime( 41, 10 );
		break;

	case 2:
		iAnim = IsEmpty() ? ANIM_PISTOL_IDLE3_EMPTY : ANIM_PISTOL_IDLE3;
		flTime = GetAnimationTime( 41, 30 );
		break;
	}

	SendWeaponAnim(iAnim);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flTime;
}
