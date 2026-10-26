// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_shotgun_doublebarrel.h"

LINK_ENTITY_TO_CLASS( weapon_doublebarrel, CWeaponShotgunDoubleBarrel );
LINK_ENTITY_TO_CLASS( weapon_dbarrel, CWeaponShotgunDoubleBarrel );
PRECACHE_WEAPON_REGISTER( weapon_doublebarrel );

void CWeaponShotgunDoubleBarrel::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_doublebarrel" );
	Precache();
	SET_MODEL(ENT(pev), "models/w_doublebarrel.mdl");
	DefaultSpawn();
}

void CWeaponShotgunDoubleBarrel::Precache(void)
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL("models/v_doublebarrel.mdl");
	PRECACHE_MODEL("models/w_doublebarrel.mdl");
	PRECACHE_MODEL("models/p_doublebarrel.mdl");

	PRECACHE_MODEL("models/shotgunshell.mdl");

	PRECACHE_SOUND("weapons/doublebarrel/dryfire.wav");
	PRECACHE_SOUND("weapons/doublebarrel/fire.wav");
	PRECACHE_SOUND("weapons/doublebarrel/open.wav");
	PRECACHE_SOUND("weapons/doublebarrel/out.wav");
	PRECACHE_SOUND("weapons/doublebarrel/unload_1.wav");
	PRECACHE_SOUND("weapons/doublebarrel/unload_2.wav");
	PRECACHE_SOUND("weapons/doublebarrel/load1.wav");
	PRECACHE_SOUND("weapons/doublebarrel/load2.wav");
	PRECACHE_SOUND("weapons/doublebarrel/close.wav");

	m_nEventPrimary = PRECACHE_EVENT(1, "events/dbarrel.sc");
	m_nEventSecondary = PRECACHE_EVENT(1, "events/dbarrel_reload.sc");
}

int CWeaponShotgunDoubleBarrel::AddToPlayer(CBasePlayer *pPlayer)
{
	if (BaseClass::AddToPlayer(pPlayer))
	{
		BaseClass::SendWeaponPickup(pPlayer);
		return TRUE;
	}
	return FALSE;
}

float CWeaponShotgunDoubleBarrel::Deploy()
{
	DoDeploy( "models/v_doublebarrel.mdl", "models/p_doublebarrel.mdl", ANIM_DBARREL_DRAW, "dbarrel" );
	return GetAnimationTime( 21, 36 );
}

float CWeaponShotgunDoubleBarrel::DoHolsterAnimation()
{
	SendWeaponAnim( ANIM_DBARREL_HOLSTER );
	return GetAnimationTime( 11, 50 );
}

float CWeaponShotgunDoubleBarrel::DoWeaponUnload()
{
	SendWeaponAnim( ANIM_DBARREL_UNLOAD );
	AddWeaponSound( "weapons/doublebarrel/open.wav", 1, ATTN_NORM, GetAnimationTime( 12, 20 ) );
	AddWeaponSound( "weapons/doublebarrel/unload_1.wav", 1, ATTN_NORM, GetAnimationTime( 29, 20 ) );
	AddWeaponSound( "weapons/doublebarrel/unload_2.wav", 1, ATTN_NORM, GetAnimationTime( 31, 20 ) );
	AddWeaponSound( "weapons/doublebarrel/load1.wav", 1, ATTN_NORM, GetAnimationTime( 53, 20 ) );
	AddWeaponSound( "weapons/doublebarrel/load2.wav", 1, ATTN_NORM, GetAnimationTime( 61, 20 ) );
	AddWeaponSound( "weapons/doublebarrel/close.wav", 1, ATTN_NORM, GetAnimationTime( 72, 20 ) );
	return GetAnimationTime( 89, 20 );
}

void CWeaponShotgunDoubleBarrel::DoAudioFrame( void )
{
	// Same as our baseclass, but we play an event when we find out.wav
	for ( int x = 0; x < (int)m_vecWeaponSoundData.size(); x++ )
	{
		WeaponSoundData &soundData = m_vecWeaponSoundData[x];
		if ( soundData.Delay - gpGlobals->time <= 0 )
		{
			// Time to play the sound
			if ( m_pPlayer )
			{
				EmitWeaponSound( soundData.File, CHAN_AUTO, soundData.Volume, soundData.Attenuation );
				// If this is the "out.wav" sound, play a shell ejection event. It's a shitty hack, but eh.
				// At least the origin won't be fucked...
				if ( FStrEq( soundData.File, "weapons/doublebarrel/out.wav" ) )
				{
					int flags;
#if defined(CLIENT_WEAPONS)
					flags = FEV_NOTHOST;
#else
					flags = 0;
#endif
					PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_nEventSecondary, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, ( m_iClip == 1 ) ? 1 : 0, 0, 0, 0 );
				}
			}
			// Remove it from the list
			m_vecWeaponSoundData.erase( m_vecWeaponSoundData.begin() + x );
			break; // Only play one sound per frame
		}
	}
}

void CWeaponShotgunDoubleBarrel::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetWeaponTimerBase() + 0.15f;
		return;
	}

	if (m_iClip <= 0)
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

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);
	Vector vecDir;

	vecDir = m_pPlayer->FireBulletsPlayer( iBullets(), vecSrc, vecAiming, GetSpreadVector( PrimaryWeaponSpread() ), 8192, BULLET_PLAYER_DBARREL, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );

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

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

void CWeaponShotgunDoubleBarrel::WeaponIdle( void )
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	float flTime;
	int iAnim;
	switch (RANDOM_LONG(0, 1))
	{
	case 0:
		iAnim = ANIM_DBARREL_IDLE2;
		flTime = GetAnimationTime( 1, 5 );
		break;

	default:
	case 1:
		iAnim = ANIM_DBARREL_IDLE1;
		flTime = GetAnimationTime( 1, 5 );
		break;
	}

	SendWeaponAnim(iAnim);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flTime;
}

void CWeaponShotgunDoubleBarrel::Reload( void )
{
	if ( m_pPlayer->ammo_buckshot <= 0 ) return;
	bool bSingleShell = ( m_iClip == 1 );
	float flDelay = bSingleShell ? GetAnimationTime( 65, 25 ) : GetAnimationTime( 56, 20 );
	if ( DefaultReload( bSingleShell ? ANIM_DBARREL_RELOAD_SINGLE : ANIM_DBARREL_RELOAD, flDelay ) )
	{
		if ( bSingleShell )
		{
			AddWeaponSound( "weapons/doublebarrel/open.wav", 1, ATTN_NORM, GetAnimationTime( 15, 25 ) );
			AddWeaponSound( "weapons/doublebarrel/out.wav", 1, ATTN_NORM, GetAnimationTime( 22, 25 ) );
			AddWeaponSound( "weapons/doublebarrel/load1.wav", 1, ATTN_NORM, GetAnimationTime( 43, 25 ) );
			AddWeaponSound( "weapons/doublebarrel/close.wav", 1, ATTN_NORM, GetAnimationTime( 53, 25 ) );
		}
		else
		{
			AddWeaponSound( "weapons/doublebarrel/open.wav", 1, ATTN_NORM, GetAnimationTime( 8, 20 ) );
			AddWeaponSound( "weapons/doublebarrel/out.wav", 1, ATTN_NORM, GetAnimationTime( 18, 20 ) );
			AddWeaponSound( "weapons/doublebarrel/load1.wav", 1, ATTN_NORM, GetAnimationTime( 28, 20 ) );
			AddWeaponSound( "weapons/doublebarrel/load2.wav", 1, ATTN_NORM, GetAnimationTime( 35, 20 ) );
			AddWeaponSound( "weapons/doublebarrel/close.wav", 1, ATTN_NORM, GetAnimationTime( 45, 20 ) );
		}
	}
}
