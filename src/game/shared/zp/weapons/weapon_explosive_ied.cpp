// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "weapon_explosive_ied.h"
#include "throwable_satchel.h"
#ifndef CLIENT_DLL
#include "gamerules.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_satchel, CWeaponExplosiveIED );
LINK_ENTITY_TO_CLASS( weapon_ied, CWeaponExplosiveIED );
PRECACHE_WEAPON_REGISTER( weapon_ied );

//=========================================================
//=========================================================
int CWeaponExplosiveIED::AddToPlayer(CBasePlayer *pPlayer)
{
	if ( BaseClass::AddToPlayer( pPlayer ) )
	{
		BaseClass::SendWeaponPickup( pPlayer );
		return TRUE;
	}
	return FALSE;
}

void CWeaponExplosiveIED::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_ied" );

	Precache();
	SET_MODEL(ENT(pev), "models/w_satchel.mdl");
	DefaultSpawn();

	WeaponData slot = GetWeaponSlotInfo( GetWeaponID() );
	m_iPrevClip = slot.DefaultAmmo;

	m_iIEDState = IED_STATE_NORMAL;
}

void CWeaponExplosiveIED::Precache(void)
{
	BaseClass::BasePrecache();

	PRECACHE_MODEL("models/v_satchel.mdl");
	PRECACHE_MODEL("models/w_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel_radio.mdl");
	PRECACHE_SOUND("weapons/ied/button_press.wav");

	UTIL_PrecacheOther( "monster_satchel" );
}

float CWeaponExplosiveIED::Deploy()
{
	// Check if we just picked up the satchel package again
	if ( m_iClip > m_iPrevClip )
	{
		m_iIEDState = IED_STATE_NORMAL;
		m_iPrevClip = m_iClip;
	}

	DoDeploy(
		"models/v_satchel.mdl",
		HasSatchelCharge() ? "models/p_satchel_radio.mdl" : "models/p_satchel.mdl",
		HasSatchelCharge() ? ANIM_SATCHEL_DETONATOR_DRAW : ANIM_SATCHEL_DRAW,
		HasSatchelCharge() ? "hive" : "trip"
	);
	return GetAnimationTime( HasSatchelCharge() ? 18 : 22, 30 );
}

float CWeaponExplosiveIED::DoHolsterAnimation()
{
	bool bHasSatchel = HasSatchelCharge();
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);

	SendWeaponAnim( bHasSatchel ? ANIM_SATCHEL_DETONATOR_HOLSTER : ANIM_SATCHEL_HOLSTER );

	if ( m_iIEDState == IED_STATE_DETONATED )
	{
#ifndef CLIENT_DLL
		DestroyAndSwitchToNextBestWeapon();
		return 0.0f;
#endif
		DestroyItem();
	}

	return GetAnimationTime( 9, 30 );
}

void CWeaponExplosiveIED::PrimaryAttack( void )
{
	if ( BeginThrowOrPlant() ) return;
	if ( !HasSatchelCharge() ) return;

	AddWeaponSound( "weapons/ied/button_press.wav", 1, ATTN_NORM, GetAnimationTime( 4, 30 ) );
	SendWeaponAnim( ANIM_SATCHEL_DETONATOR_USE );

	edict_t *pPlayer = m_pPlayer->edict();

	CThrowableSatchelCharge *pSatchel = NULL;

	while ((pSatchel = (CThrowableSatchelCharge *)UTIL_FindEntityInSphere(pSatchel, m_pPlayer->pev->origin, 4096)) != NULL)
	{
		if (FClassnameIs(pSatchel->pev, "monster_satchel"))
		{
#ifndef CLIENT_DLL
			if (pSatchel->pev->owner == pPlayer || pSatchel->GetThrower() == ENTINDEX(pPlayer))
				pSatchel->IEDExplode();
#endif
		}
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime( 14, 30 );
	m_iIEDState = IED_STATE_DETONATED;
}

bool CWeaponExplosiveIED::HasSatchelCharge() const
{
	return ( m_iIEDState == IED_STATE_HAS_THROWN );
}

void CWeaponExplosiveIED::OnClipIncrease( int iAmount )
{
	BaseClass::OnClipIncrease( iAmount );
	float flHolsterDelay = DoHolsterAnimation();
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = GetWeaponTimerBase() + flHolsterDelay;
	m_pPlayer->m_flNextAttack = GetPlayerTimerBase() + flHolsterDelay;
	m_iPrevClip = m_iClip;
	m_iIEDState = IED_STATE_TO_NORMAL;
}

bool CWeaponExplosiveIED::PlantIED( void )
{
	// We found a wall? Plant the IED. That's it.

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 32, ignore_monsters, ENT(m_pPlayer->pev), &tr );

	if ( tr.flFraction < 1.0f )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		if ( pEntity && !(pEntity->pev->flags & FL_CONVEYOR) )
		{
#ifndef CLIENT_DLL
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );
			CThrowableSatchelCharge *pSatchel = (CThrowableSatchelCharge *)Create( "monster_satchel", tr.vecEndPos + tr.vecPlaneNormal * 8, angles, m_pPlayer->edict() );
			pSatchel->PlantIED();
			pSatchel->DisallowPickupFor( 2.0f );
			UTIL_strcpy( m_pPlayer->m_szAnimExtention, "hive" );
#endif
			SendWeaponAnim(ANIM_SATCHEL_DROP);

			// player "shoot" animation
			m_pPlayer->SetAnimation(PLAYER_ATTACK1);

			m_iClip = 0;
			m_iPrevClip = 0;

			m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime(20, 30);

			m_iIEDState = IED_STATE_THROWING;
			return true;
		}
	 }

	return false;
}

bool CWeaponExplosiveIED::BeginThrowOrPlant(void)
{
	if ( m_iClip > 0 )
	{
		if ( PlantIED() ) return true;

		Vector vecSrc = m_pPlayer->pev->origin;
		Vector vecThrow = gpGlobals->v_forward * 274 + m_pPlayer->pev->velocity;

#ifndef CLIENT_DLL
		CThrowableSatchelCharge *pSatchel = (CThrowableSatchelCharge *)Create( "monster_satchel", vecSrc, Vector(0, 0, 0), m_pPlayer->edict() );
		pSatchel->pev->velocity = vecThrow;
		pSatchel->pev->avelocity.y = 400;
		pSatchel->DisallowPickupFor( 2.0f );
		m_pPlayer->pev->weaponmodel = MAKE_STRING( "models/p_satchel_radio.mdl" );
		UTIL_strcpy( m_pPlayer->m_szAnimExtention, "hive" );
#endif
		SendWeaponAnim( ANIM_SATCHEL_DROP );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_iClip = 0;
		m_iPrevClip = 0;

		m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = GetWeaponTimerBase() + GetAnimationTime( 20, 30 );

		m_iIEDState = IED_STATE_THROWING;
		return true;
	}
	return false;
}

void CWeaponExplosiveIED::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > GetWeaponTimerBase() ) return;

	switch ( m_iIEDState )
	{
		case IED_STATE_THROWING:
		{
			m_iIEDState = IED_STATE_HAS_THROWN;
			DoDeployAnimation();
			return;
		}
		break;

		case IED_STATE_HAS_THROWN:
		{
			if ( m_iClip > m_iPrevClip )
			{
			    m_iIEDState = IED_STATE_OBTAINED_PACKAGE;
				m_iPrevClip = m_iClip;
			    return;
			}
			break;
		}

		case IED_STATE_OBTAINED_PACKAGE:
		{
			m_iIEDState = IED_STATE_TO_NORMAL;
			float flHolsterDelay = DoHolsterAnimation();
			m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = GetWeaponTimerBase() + flHolsterDelay;
			m_pPlayer->m_flNextAttack = GetPlayerTimerBase() + flHolsterDelay;
			return;
		}
		break;

		case IED_STATE_TO_NORMAL:
		{
			m_iIEDState = IED_STATE_NORMAL;
			DoDeployAnimation();
			return;
		}
		break;

		case IED_STATE_DETONATED:
		{
		    DoHolsterAnimation();
			return;
		}
		break;
	}

	int iAnimation;
	float flDelay;
	bool bHasSatchel = HasSatchelCharge();

	switch ( RANDOM_LONG( 0, 8 ) )
	{
		default:
			iAnimation = bHasSatchel ? ANIM_SATCHEL_DETONATOR_IDLE : ANIM_SATCHEL_IDLE;
			flDelay = GetAnimationTime( bHasSatchel ? 36 : 39, 5 );
		break;

		case 4:
		    iAnimation = bHasSatchel ? ANIM_SATCHEL_DETONATOR_FIDGET : ANIM_SATCHEL_FIDGET;
			flDelay = GetAnimationTime( bHasSatchel ? 36 : 50, 20 );
		break;
	}

	SendWeaponAnim( iAnimation );
	m_flTimeWeaponIdle = GetWeaponTimerBase() + flDelay;
}
