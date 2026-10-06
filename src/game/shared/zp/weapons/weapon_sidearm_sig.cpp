// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_sidearm_sig.h"

LINK_ENTITY_TO_CLASS(weapon_sig, CWeaponSideArmSig);
LINK_ENTITY_TO_CLASS(weapon_9mmhandgun, CWeaponSideArmSig); // Only used by old custom maps, don't remove this
PRECACHE_WEAPON_REGISTER( weapon_sig );


float CWeaponSideArmSig::DoHolsterAnimation()
{
	SendWeaponAnim( IsEmpty() ? ANIM_SIG_HOLSTER_EMPTY : ANIM_SIG_HOLSTER );
	return GetAnimationTime( 16, 40 );
}

float CWeaponSideArmSig::DoWeaponUnload()
{
	SendWeaponAnim( ANIM_SIG_UNLOAD );
	AddWeaponSound( "weapons/sig/clipout_unload.wav", 1, ATTN_NORM, GetAnimationTime( 20, 32 ) );
	AddWeaponSound( "weapons/sig/clipin.wav", 1, ATTN_NORM, GetAnimationTime( 57, 32 ) );
	AddWeaponSound( "weapons/sig/slideback.wav", 1, ATTN_NORM, GetAnimationTime( 82, 32 ) );
	return GetAnimationTime( 111, 35 );
}

void CWeaponSideArmSig::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_sig" );
	Precache();
	SET_MODEL(ENT(pev), "models/w_sig.mdl");
	DefaultSpawn();
}

void CWeaponSideArmSig::Precache(void)
{
	PRECACHE_MODEL("models/v_sig.mdl");
	PRECACHE_MODEL("models/w_sig.mdl");
	PRECACHE_MODEL("models/p_sig.mdl");

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
	DoDeploy( "models/v_sig.mdl", "models/p_sig.mdl", IsEmpty() ? ANIM_SIG_DRAW_EMPTY : ANIM_SIG_DRAW, "onehanded" );
	return GetAnimationTime( 31, 40 );
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

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GetAnimationTime( 21, 36 );
}

void CWeaponSideArmSig::Reload(void)
{
	if (m_pPlayer->ammo_9mm <= 0)
		return;
	int nFrame = IsEmpty() ? 91 : 81;
	int nFPS = 38;

	int iResult = DefaultReload( IsEmpty() ? ANIM_SIG_RELOAD_EMPTY : ANIM_SIG_RELOAD, GetAnimationTime( nFrame, nFPS ) );
	if ( iResult )
	{
		AddWeaponSound( "weapons/sig/clipout.wav", 1, ATTN_NORM, GetAnimationTime( 20, nFPS ) );
		AddWeaponSound( "weapons/sig/clipin.wav", 1, ATTN_NORM, GetAnimationTime( 57, nFPS ) );
		if ( IsEmpty() )
			AddWeaponSound( "weapons/sig/slideforward.wav", 1, ATTN_NORM, GetAnimationTime( 76, nFPS ) );
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
		iAnim = IsEmpty() ? ANIM_SIG_IDLE1_EMPTY : ANIM_SIG_IDLE1;
		flTime = GetAnimationTime( 81, 25 );
		break;

	default:
	case 1:
		iAnim = IsEmpty() ? ANIM_SIG_IDLE2_EMPTY : ANIM_SIG_IDLE2;
		flTime = GetAnimationTime( 81, 10 );
		break;

	case 2:
		iAnim = IsEmpty() ? ANIM_SIG_IDLE3_EMPTY : ANIM_SIG_IDLE3;
		flTime = GetAnimationTime( 91, 30 );
		break;
	}

	SendWeaponAnim(iAnim);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flTime;
}
