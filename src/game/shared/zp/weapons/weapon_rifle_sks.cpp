// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_rifle_sks.h"

LINK_ENTITY_TO_CLASS( weapon_sks, CWeaponRifleSKS );
PRECACHE_WEAPON_REGISTER( weapon_sks );


float CWeaponRifleSKS::DoHolsterAnimation()
{
	SendWeaponAnim( IsEmpty() ? ANIM_SKS_HOLSTER_EMPTY : ANIM_SKS_HOLSTER );
	return GetAnimationTime( 21, 30 );
}

float CWeaponRifleSKS::DoWeaponUnload()
{
	SendWeaponAnim( ANIM_SKS_UNLOAD );
	AddWeaponSound( "weapons/556ar/magout_unload.wav", 1, ATTN_NORM, GetAnimationTime( 15, 20 ) );
	AddWeaponSound( "weapons/556ar/magin_unload.wav", 1, ATTN_NORM, GetAnimationTime( 28, 20 ) );
	return GetAnimationTime( 153, 30 );
}

void CWeaponRifleSKS::Spawn()
{
	Precache();
	SET_MODEL( ENT(pev), "models/w_sks.mdl" );
	DefaultSpawn();
	LoadMeleeConfigFile( GetWeaponID() );
}

void CWeaponRifleSKS::Precache(void)
{
	PRECACHE_MODEL("models/v_sks.mdl");
	PRECACHE_MODEL("models/w_sks.mdl");
	PRECACHE_MODEL("models/p_sks.mdl");

	PRECACHE_MODEL("models/shell_rifle.mdl"); // brass shellTE_MODEL

	PRECACHE_SOUND("items/ammo_pickup.wav");

	PRECACHE_SOUND("weapons/sks/fire.wav");
	PRECACHE_SOUND("weapons/556ar/magout.wav");
	PRECACHE_SOUND("weapons/556ar/magin.wav");
	PRECACHE_SOUND("weapons/556ar/magout_unload.wav");
	PRECACHE_SOUND("weapons/556ar/magin_unload.wav");
	PRECACHE_SOUND("weapons/556ar/charge.wav");

	m_nEventPrimary = PRECACHE_EVENT(1, "events/sks.sc");
}

int CWeaponRifleSKS::AddToPlayer(CBasePlayer *pPlayer)
{
	if (BaseClass::AddToPlayer(pPlayer))
	{
		BaseClass::SendWeaponPickup(pPlayer);
		return TRUE;
	}
	return FALSE;
}

float CWeaponRifleSKS::Deploy()
{
	ResetMeleeState();
	DoDeploy( "models/v_sks.mdl", "models/p_sks.mdl", IsEmpty() ? ANIM_SKS_DRAW_EMPTY : ANIM_SKS_DRAW, "556ar" );
	return GetAnimationTime( 33, 35 );
}

void CWeaponRifleSKS::OnWeaponPrimaryAttack()
{
	int flags;
#if defined(CLIENT_WEAPONS)
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);
	Vector vecDir;
	vecDir = m_pPlayer->FireBulletsPlayer(iBullets(), vecSrc, vecAiming, GetSpreadVector( PrimaryWeaponSpread() ), 8192, BULLET_PLAYER_SKS, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed);

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_nEventPrimary, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, m_iClip, 0, 0, 0 );
}

void CWeaponRifleSKS::OnRequestedAnimation( SingleActionAnimReq act )
{
	switch ( act )
	{
		case CWeaponBaseSingleAction::ANIM_IDLE:
		{
		    float flDelay = 1.0f;
		    int nAnimID = 0;
			switch (RANDOM_LONG(0, 1))
			{
				case 0:
				{
			        nAnimID = IsEmpty() ? ANIM_SKS_IDLE_EMPTY : ANIM_SKS_IDLE;
			        flDelay = GetAnimationTime( 30, 25 );
				}
		        break;
				case 1:
				{
			        nAnimID = IsEmpty() ? ANIM_SKS_IDLE_FIDGET_EMPTY : ANIM_SKS_IDLE_FIDGET;
					flDelay = GetAnimationTime( 46, 30 );
				}
		        break;
			}
			SendWeaponAnim( nAnimID );
		    m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flDelay;
		}
		break;
		case CWeaponBaseSingleAction::ANIM_LONGIDLE:
		{
			SendWeaponAnim( IsEmpty() ? ANIM_SKS_IDLE_LONG_EMPTY : ANIM_SKS_IDLE_LONG );
		    m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GetAnimationTime( 86, 12 );
		}
		break;
	    case CWeaponBaseSingleAction::ANIM_PRIMARYATTACK:
		{
		    m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GetAnimationTime( 13, 30 );
		}
		break;
		case CWeaponBaseSingleAction::ANIM_RELOAD_START:
		{
			SendWeaponAnim( ANIM_SKS_RELOAD_START );
			m_pPlayer->SetAnimation( PLAYER_RELOAD_START );
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GetAnimationTime( 48, 30 );
		}
		break;
		case CWeaponBaseSingleAction::ANIM_RELOAD:
		{
		    if (RANDOM_LONG(0, 1))
			    EmitWeaponSound( "weapons/shotgun/reload1.wav", CHAN_ITEM, 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f) );
		    else
			    EmitWeaponSound( "weapons/shotgun/reload2.wav", CHAN_ITEM, 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f) );

		    SendWeaponAnim( ANIM_SKS_RELOAD_LOOP );
			m_pPlayer->SetAnimation( PLAYER_RELOAD );

#if defined( SERVER_DLL )
			m_pPlayer->m_iWeaponKillCount = 0;
#endif

			float flAnimTime = GetAnimationTime( 19, 30 );
		    m_flNextReload = UTIL_WeaponTimeBase() + flAnimTime;
		    m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flAnimTime;
		}
		break;
		case CWeaponBaseSingleAction::ANIM_RELOAD_END:
		{
			SendWeaponAnim( ANIM_SKS_RELOAD_END );
			m_pPlayer->SetAnimation( PLAYER_RELOAD_END );
			EmitWeaponSound( "weapons/shotgun/pump.wav", CHAN_ITEM, 1, ATTN_NORM, 0, 105 );
			float flAnimTime = GetAnimationTime( 34, 30 );
		    m_flNextReload = UTIL_WeaponTimeBase() + flAnimTime;
		    m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flAnimTime;
		}
		break;
	}
}


void CWeaponRifleSKS::Reload( void )
{
	if ( !IsEmpty() )
	{
		ResetMeleeState();
		BaseClass::Reload();
		return;
	}

	if ( m_pPlayer->ammo_556ar <= 0 )
		return;

	float fDelay = GetAnimationTime( 132, 30 );
	if ( DefaultReload( ANIM_SKS_RELOAD_EMPTY, fDelay ) )
	{
		ResetMeleeState();
		AddWeaponSound( "weapons/556ar/magout.wav", 1, ATTN_NORM, 0.13f );
		AddWeaponSound( "weapons/556ar/magin.wav", 1, ATTN_NORM, 1.2f );
		AddWeaponSound( "weapons/556ar/charge.wav", 1, ATTN_NORM, 2.03f );
	}
}

void CWeaponRifleSKS::SecondaryAttack( void )
{
	if ( IsInHeavyAttack() )
	{
		m_pPlayer->SetAnimation( PLAYER_ATTACK2_HOLD );
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		return;
	}
	// Already attacking?
	if ( IsAttackInProgress() ) return;
	SendWeaponAnim( IsEmpty() ? ANIM_SKS_STAB_START_EMPTY : ANIM_SKS_STAB_START );
	m_pPlayer->SetAnimation( PLAYER_ATTACK2_PRE );
	n_meleeAttackType = MELEE_ATTACK_HEAVY;
	float flHoldTime = m_attackTracers[1].flAnimTimeHold;
	if ( flHoldTime <= 0.0f ) flHoldTime = 0.1f;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + flHoldTime;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flHoldTime;
}

void CWeaponRifleSKS::WeaponIdle()
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() ) return;
	if ( IsAttackInProgress() )
	{
		ResetEmptySound();
		DoMeleeAttack( this );
		return;
	}
	BaseClass::WeaponIdle();
}


void CWeaponRifleSKS::DoWeaponSoundFromAttack( MeleeAttackType attackType, bool bHitWorld )
{
	const char *szSoundFile = nullptr;
	if ( bHitWorld )
	{
		switch ( RANDOM_LONG( 0, 1 ) )
		{
			case 0: szSoundFile = "weapons/melee/fireaxe/hit1_heavy.wav"; break;
			case 1: szSoundFile = "weapons/melee/fireaxe/hit2_heavy.wav"; break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
			case 0: szSoundFile = "weapons/melee/fireaxe/hitbod1_heavy.wav"; break;
			case 1: szSoundFile = "weapons/melee/fireaxe/hitbod2_heavy.wav"; break;
			case 2: szSoundFile = "weapons/melee/fireaxe/hitbod3_heavy.wav"; break;
		}
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

void CWeaponRifleSKS::DoWeaponSoundFromMiss( MeleeAttackType attackType )
{
	const char *szSoundFile = nullptr;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
		case 0: szSoundFile = "weapons/melee/fireaxe/miss1.wav"; break;
		case 1: szSoundFile = "weapons/melee/fireaxe/miss2.wav"; break;
	}
	EmitWeaponSound( szSoundFile, CHAN_ITEM, 1, ATTN_NORM );
}

float CWeaponRifleSKS::GetAttackAnimationTime( int iAnim )
{
	switch ( iAnim )
	{
		case ANIM_SKS_STAB_HIT:
		case ANIM_SKS_STAB_HIT_EMPTY: return GetAnimationTime( 51, 30 );
		case ANIM_SKS_STAB_HITWORLD:
		case ANIM_SKS_STAB_HITWORLD_EMPTY: return GetAnimationTime( 31, 30 );
	    case ANIM_SKS_STAB_MISS:
		case ANIM_SKS_STAB_MISS_EMPTY: return GetAnimationTime( 25, 30 );
	}
	return 1.0f;
}

int CWeaponRifleSKS::DoMeleeAnimation( CWeaponBase *pWeapon )
{
	bool bHitWorld;
	bool bHitSomething = DidMeleeAttackHit( pWeapon, n_meleeAttackType, bHitWorld );
	if ( IsEmpty() )
	{
		if ( bHitSomething ) return bHitWorld ? ANIM_SKS_STAB_HITWORLD_EMPTY : ANIM_SKS_STAB_HIT_EMPTY;
		return ANIM_SKS_STAB_MISS_EMPTY;
	}
	if ( bHitSomething ) return bHitWorld ? ANIM_SKS_STAB_HITWORLD : ANIM_SKS_STAB_HIT;
	return ANIM_SKS_STAB_MISS;
}
