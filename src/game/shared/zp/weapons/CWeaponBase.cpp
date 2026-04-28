// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "CWeaponBase.h"
#ifndef CLIENT_DLL
#include "gamerules.h"
#include "zp/gamemodes/zp_gamemodebase.h"
#endif

/// <summary>
/// Modified iItemSlot function.
/// This is deprecated, so do not use it.
/// </summary>
/// <param name=""></param>
/// <returns>Weapon slot number + 1</returns>
int CWeaponBase::iItemSlot( void )
{
	WeaponData slot = GetWeaponSlotInfo( GetWeaponID() );
	return slot.Slot + 1;
}

bool CWeaponBase::DoDeploy( const char *szViewModel, const char *szWeaponModel, int iAnim, const char *szAnimExt, int skiplocal, int body )
{
	m_bIsHolstering = false;
	m_bIsUnloading = false;
	ClearWeaponSounds();

#ifndef CLIENT_DLL
	m_pPlayer->TabulateAmmo();
	m_pPlayer->pev->viewmodel = MAKE_STRING( szViewModel );
	m_pPlayer->pev->weaponmodel = MAKE_STRING( szWeaponModel );
	UTIL_strcpy( m_pPlayer->m_szAnimExtention, szAnimExt );
	m_pPlayer->m_iWeaponKillCount = 0;
#endif
	SendWeaponAnim( iAnim, skiplocal, body );

	m_pPlayer->m_flNextAttack = GetPlayerTimerBase() + 0.5f;
	m_flTimeWeaponIdle = GetWeaponTimerBase() + 1.0f;
	return true;
}

int CWeaponBase::DefaultReload(int iAnim, float fDelay, int body)
{
	// No reloads while holstering
	if ( IsHolstering() ) return FALSE;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return FALSE;

	int j = min(GetData().MaxClip - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);

	if (j == 0)
		return FALSE;

	m_pPlayer->m_flNextAttack = GetPlayerTimerBase() + fDelay;

	// Play our animation!
	m_pPlayer->SetAnimation( IsEmpty() ? PLAYER_RELOAD_EMPTY : PLAYER_RELOAD );

	SendWeaponAnim(iAnim, 0, body);

	m_fInReload = TRUE;

#ifndef CLIENT_DLL
	m_pPlayer->m_iWeaponKillCount = 0;
#endif

	m_flTimeWeaponIdle = GetWeaponTimerBase() + fDelay + 0.1f;
	return TRUE;
}

void CWeaponBase::FallInit()
{
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0)); //pointsize until it lands on the ground.

	SetTouch( &CWeaponBase::OnWeaponTouch );
	SetThink( &CWeaponBase::OnWeaponThink );

	pev->nextthink = gpGlobals->time + 0.1f;
}

void CWeaponBase::OnWeaponTouch( CBaseEntity *pOther )
{
	// Ignore players
	if ( pOther->IsPlayer() ) return;

	// If we hit ammo, ignore it.
	CBasePlayerAmmo *pItem = dynamic_cast<CBasePlayerAmmo *>( pOther );
	if ( pItem ) return;

	// If we hit weapon, ignore it.
	CWeaponBase *pWeapon = dynamic_cast<CWeaponBase *>( pOther );
	if ( pWeapon ) return;

	ZP::Physics::OnHit( this, pOther );
}

void CWeaponBase::OnWeaponThink()
{
	if ( ZP::Physics::Simulate( this ) )
	{
#if !defined( CLIENT_DLL )
		if ( m_bHasDropped )
			CheckIfStuckInWorld();
#endif
		pev->nextthink = gpGlobals->time + 0.1f;
	}
}

void CWeaponBase::BounceSound()
{
	const float flSpeed   = pev->velocity.Length();
	const float flSpeed2D = pev->velocity.Length2D();
	const float flAbsZ    = fabs( pev->velocity.z );

	// Ignore tiny/jitter motion
	if ( flSpeed < 40.0f )
		return;

	// On ground: only allow real bumps (not sliding/resting micro-contacts)
	if ( (pev->flags & FL_ONGROUND) && flAbsZ < 30.0f && flSpeed2D < 80.0f )
		return;

	// Cooldown to prevent touch-callback spam
	if ( gpGlobals->time < m_flNextBounceSound )
		return;

	m_flNextBounceSound = gpGlobals->time + 0.18f;

	int pitch = 95 + RANDOM_LONG( 0, 10 );
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "items/weapon_drop.wav", 1, ATTN_NORM, 0, pitch );
}

float CWeaponBase::GetWeaponTimerBase()
{
	return UseDecrement() ? UTIL_WeaponTimeBase() : gpGlobals->time;
}

float CWeaponBase::GetPlayerTimerBase()
{
#if defined(CLIENT_WEAPONS)
	return UTIL_WeaponTimeBase();
#else
	return gpGlobals->time;
#endif
}

void CWeaponBase::EmitWeaponSound( const char *szSoundFile, int channel, float volume, float attenuation, int flags, int pitch )
{
	if ( !m_pPlayer || !szSoundFile || !szSoundFile[0] )
		return;

#if defined( CLIENT_DLL )
	if ( UseDecrement() )
		return;
#endif

	EMIT_SOUND_DYN( ENT(m_pPlayer->pev), channel, szSoundFile, volume, attenuation, flags, pitch );
}

#if !defined( CLIENT_DLL )
CBasePlayerWeapon *CWeaponBase::FindNextBestWeapon()
{
	if ( !m_pPlayer )
		return nullptr;

	CBasePlayerWeapon *pBest = nullptr;
	int iBestWeight = -1;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		for ( CBasePlayerItem *pCheck = m_pPlayer->m_rgpPlayerItems[i]; pCheck; pCheck = pCheck->m_pNext )
		{
			if ( pCheck == this )
				continue;

			if ( ( pCheck->iFlags() & ITEM_FLAG_NOAUTOSWITCHTO ) != 0 )
				continue;

			if ( !pCheck->CanDeploy() )
				continue;

			if ( pCheck->iWeight() > -1 && pCheck->iWeight() == iWeight() )
				return static_cast<CBasePlayerWeapon *>( pCheck );

			if ( pCheck->iWeight() > iBestWeight )
			{
				iBestWeight = pCheck->iWeight();
				pBest = static_cast<CBasePlayerWeapon *>( pCheck );
			}
		}
	}

	return pBest;
}

void CWeaponBase::DestroyAndSwitchToNextBestWeapon()
{
	if ( !m_pPlayer )
	{
		DestroyItem();
		return;
	}

	CBasePlayer *pPlayer = m_pPlayer;
	CBasePlayerWeapon *pNextWeapon = FindNextBestWeapon();
	pPlayer->WeaponSlotSet( this, false );
	DestroyItem();

	if ( pNextWeapon )
		pPlayer->SelectNewActiveWeapon( pNextWeapon );
}
#endif

void CWeaponBase::DoDeployAnimation()
{
	float flDeploy = Deploy();
	float flWeaponTimerBase = GetWeaponTimerBase();
	float flPlayerTimerBase = GetPlayerTimerBase();
	m_flHolsterTime = -1;
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = flWeaponTimerBase + flDeploy;
	m_pPlayer->m_flNextAttack = flPlayerTimerBase + flDeploy;
	m_pPlayer->SetAnimation( PLAYER_DRAW );
}

void CWeaponBase::BeginHolster( CBasePlayerWeapon *pWeapon )
{
	m_pPlayer->SetAnimation( PLAYER_HOLSTER );
	m_pNewWeapon = pWeapon;
	m_bIsHolstering = true;
	m_fInReload = FALSE;
	m_pPlayer->m_flNextAttack = 0;
	float flHolsterDelay = DoHolsterAnimation();
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = GetWeaponTimerBase() + flHolsterDelay;
	m_flHolsterTime = gpGlobals->time + flHolsterDelay;
	ClearWeaponSounds();
}

void CWeaponBase::FinishHolster()
{
	m_bIsHolstering = false;
	m_flHolsterTime = -1;
	BaseClass::Holster();
	if ( !m_pPlayer ) return;
	if ( !m_pNewWeapon ) return;
#if !defined( CLIENT_DLL )
	m_pPlayer->SelectNewActiveWeapon( m_pNewWeapon );
	m_pPlayer->m_iWeaponKillCount = 0;
#endif
	m_pNewWeapon = nullptr;
}

void CWeaponBase::StopHolstering()
{
	m_bIsHolstering = false;
	m_flHolsterTime = -1;
	m_pNewWeapon = nullptr;
}

/// <summary>
/// Modified ItemPostFrame from CBasePlayerWeapon.
/// Because having 2 cpp files for the same class is stupid.
/// </summary>
/// <param name=""></param>
void CWeaponBase::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = m_pPlayer; // Cache player cos attack could retire weapon and remove it from player
	if ( !pPlayer ) return;

	// If we are holstering, we can't do anything until it's done
	if ( m_bIsHolstering )
	{
		pPlayer->pev->button &= ~IN_ATTACK;
		pPlayer->pev->button &= ~IN_ATTACK2;
		if ( m_flHolsterTime - gpGlobals->time <= 0.0f )
			FinishHolster();
		return;
	}

	if ( m_bIsUnloading )
		FinishUnloading();

#ifndef CLIENT_DLL
	if ( ZP::GetCurrentRoundState() != ZP::RoundState::RoundState_RoundHasBegun )
		return;
#endif

	if ((m_fInReload) && (pPlayer->m_flNextAttack <= UTIL_WeaponTimeBase()))
	{
		// complete the reload.
		int j = min(iMaxClip() - m_iClip, pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);

		// Add them to the clip
		m_iClip += j;
		pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;

#ifndef CLIENT_DLL
		pPlayer->TabulateAmmo();
#endif

		m_fInReload = FALSE;
	}

	if ( IsAutomaticWeapon() )
	{
		if ((pPlayer->pev->button & IN_ATTACK2) && CanAttack(m_flNextSecondaryAttack, gpGlobals->time, UseDecrement()))
		{
			if (HasValidAmmoType(false) && !pPlayer->m_rgAmmo[SecondaryAmmoIndex()])
			{
				m_fFireOnEmpty = TRUE;
			}

#ifndef CLIENT_DLL
			pPlayer->TabulateAmmo();
#endif
			SecondaryAttack();
		}
		else if ((pPlayer->pev->button & IN_ATTACK) && CanAttack(m_flNextPrimaryAttack, gpGlobals->time, UseDecrement()))
		{
			if ((m_iClip == 0 && HasValidAmmoType(true)) || (iMaxClip() == WEAPON_NOCLIP && !pPlayer->m_rgAmmo[PrimaryAmmoIndex()]))
			{
				m_fFireOnEmpty = TRUE;
			}

#ifndef CLIENT_DLL
			pPlayer->TabulateAmmo();
#endif
			PrimaryAttack();
		}
	}
	else
	{
		if ((pPlayer->m_afButtonPressed & IN_ATTACK2) && CanAttack(m_flNextSecondaryAttack, gpGlobals->time, UseDecrement()))
		{
			if (HasValidAmmoType(false) && !pPlayer->m_rgAmmo[SecondaryAmmoIndex()])
			{
				m_fFireOnEmpty = TRUE;
			}

#ifndef CLIENT_DLL
			pPlayer->TabulateAmmo();
#endif
			SecondaryAttack();
		}
		else if ((pPlayer->m_afButtonPressed & IN_ATTACK) && CanAttack(m_flNextPrimaryAttack, gpGlobals->time, UseDecrement()))
		{
			if ((m_iClip == 0 && HasValidAmmoType(true)) || (iMaxClip() == WEAPON_NOCLIP && !pPlayer->m_rgAmmo[PrimaryAmmoIndex()]))
			{
				m_fFireOnEmpty = TRUE;
			}

#ifndef CLIENT_DLL
			pPlayer->TabulateAmmo();
#endif
			PrimaryAttack();
		}
	}

	if ( pPlayer->pev->button & IN_RELOAD
		&& iMaxClip() != WEAPON_NOCLIP
		&& !m_fInReload
		&& CanAttack( m_flNextPrimaryAttack, gpGlobals->time, true ) )
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if (!(pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2)))
	{
		// no fire buttons down

		m_fFireOnEmpty = FALSE;

		// Reload if empty and weapon has waited as long as it has to after firing
		if ((pPlayer->pev->button & (IN_RELOAD)) && m_iClip == 0 && m_flNextPrimaryAttack - GetWeaponTimerBase() < 0.0f)
		{
			Reload();
			return;
		}

		// Weapon unload
		if ((pPlayer->pev->button & (IN_UNLOAD)) && m_iClip > 0 && m_flNextPrimaryAttack - GetWeaponTimerBase() < 0.0f)
		{
			Unload();
			return;
		}

		WeaponIdle();
		return;
	}
}

void CWeaponBase::Unload()
{
	// Player isn't valid, skip.
	if ( !m_pPlayer ) return;
	// Make sure we have a valid ammo type
	if ( m_iPrimaryAmmoType < 0 ) return;
	// Can't unload while reloading
	if ( m_fInReload ) return;
	// Can't unload while holstering
	if ( IsHolstering() ) return;
	// Can't unload an empty weapon
	if ( m_iClip <= 0 ) return;
	// How much ammo to unload
	int iUnloadAmount = UnloadAmount();
	// Can we give ammo to the player?
	if ( !m_pPlayer->CanGiveAmmo( iUnloadAmount, GetAmmoByAmmoID( m_iPrimaryAmmoType ) ) ) return;
	// Play this first, so we can override it on DoWeaponUnload on our child classes.
	m_pPlayer->SetAnimation( PLAYER_RELOAD );
	// Let's tell our weapon classes to play a specific animation.
	float flDelay = DoWeaponUnload();
	m_flNextPrimaryAttack = m_flNextSecondaryAttack
		= m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flDelay;
	// next attack requires needs to remove 0.15, so we don't get the weird snap issue.
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flDelay - 0.15f;
	m_bIsUnloading = true;
}

void CWeaponBase::FinishUnloading()
{
	// Turn it off.
	m_bIsUnloading = false;
	// How much ammo to unload
	int iUnloadAmount = UnloadAmount();
	// Check how much we can give the player
	int iGiveAmmo = m_pPlayer->PickupAmmo( iUnloadAmount, GetAmmoByAmmoID( m_iPrimaryAmmoType ) );
	if ( iGiveAmmo <= 0 ) return; // Couldn't give any ammo
	// Remove the ammo from the clip
	m_iClip -= iGiveAmmo;
}

void CWeaponBase::DoAudioFrame( void )
{
	for ( int x = 0; x < (int)m_vecWeaponSoundData.size(); x++ )
	{
		WeaponSoundData &soundData = m_vecWeaponSoundData[x];
		if ( soundData.Delay - gpGlobals->time <= 0 )
		{
			// Time to play the sound
			EmitWeaponSound( soundData.File, CHAN_AUTO, soundData.Volume, soundData.Attenuation );
			// Remove it from the list
			m_vecWeaponSoundData.erase( m_vecWeaponSoundData.begin() + x );
			break; // Only play one sound per frame
		}
	}
}

void CWeaponBase::AddWeaponSound( const char *szSoundFile, float volume, float attenuation, float delay )
{
	WeaponSoundData newSound;
	newSound.File = szSoundFile;
	newSound.Volume = volume;
	newSound.Attenuation = attenuation;
	newSound.Delay = gpGlobals->time + delay;
	m_vecWeaponSoundData.push_back( newSound );
}

void CWeaponBase::ClearWeaponSounds()
{
	m_vecWeaponSoundData.clear();
}

void CWeaponBase::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Make sure we cannot steal each other!
	// This will cause a crash, we don't want that.
	if ( m_pPlayer ) return;

	// Make sure we have a player
	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;
	if ( !pPlayer || !pPlayer->IsPlayer() ) return;

	// Check if we can pickup the weapon
	if ( pPlayer->AddPlayerItem(this) )
	{
		AttachToPlayer( pPlayer );
		if ( pPlayer->pev->team == ZP::TEAM_SURVIVIOR )
			EMIT_SOUND( ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
		return;
	}

	BaseClass::Use( pActivator, pCaller, useType, value );
}

#if !defined( CLIENT_DLL )
void CWeaponBase::CheckIfStuckInWorld()
{
	if ( m_StuckChecks >= 5 )
		return; // We've already checked enough times

	// First we set the new size
	SetSequenceBox();

	// Now we grab our mins/maxs, and check if we are stuck in the world
	Vector vecMins = pev->mins + Vector( -5, -5, -5 );
	Vector vecMaxs = pev->maxs + Vector( 5, 5, 0 );
	if ( UTIL_IsBoxInWorld( this, vecMins, vecMaxs ) )
		return; // Not stuck

	// We are stuck, remove our size.
	UTIL_SetSize( pev, Vector(0,0,0), Vector(0,0,0) );
	pev->velocity = pev->basevelocity = Vector( 0, 0, 0 ); // Stop moving
}
#endif

void CWeaponBase::DefaultSpawn()
{
	BaseClass::Spawn();

	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.9;

	// World-spawned items: suppress the first-landing sound so round-start
	// spawns don't create a cacophony. Player drops skip this (they call
	// FallInit directly, not DefaultSpawn).
	m_flNextBounceSound = gpGlobals->time + 0.5f;

	FallInit(); // get ready to fall down.

	WeaponData slot = GetWeaponSlotInfo( GetWeaponID() );
	m_iDefaultAmmo = slot.DefaultAmmo;
	m_iClip = slot.DefaultAmmo;
}

/// <summary>
/// Checks if we can attack or not
/// </summary>
/// <param name="attack_time">Our current attack time</param>
/// <param name="curtime">Current time from the global timer</param>
/// <param name="isPredicted">Is this predicted?</param>
/// <returns></returns>
bool CWeaponBase::CanAttack( float attack_time, float curtime, bool isPredicted )
{
#if defined( CLIENT_WEAPONS )
	if ( !isPredicted )
#else
	if ( 1 )
#endif
		return (attack_time <= curtime) ? true : false;
	return (attack_time <= 0.0) ? true : false;
}
