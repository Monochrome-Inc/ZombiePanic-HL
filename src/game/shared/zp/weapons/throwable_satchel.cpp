// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "throwable_satchel.h"

#ifndef CLIENT_DLL
#include "soundent.h"
#include "decals.h"
#endif

LINK_ENTITY_TO_CLASS(monster_satchel, CThrowableSatchelCharge);
PRECACHE_REGISTER( monster_satchel );

void CThrowableSatchelCharge::Deactivate(void)
{
	pev->solid = SOLID_NOT;
	UTIL_Remove(this);
}

void CThrowableSatchelCharge::Spawn(void)
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_satchel.mdl");
	//UTIL_SetSize(pev, Vector( -16, -16, -4), Vector(16, 16, 32));	// Old box -- size of headcrab monsters/players get blocked by this
	//UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4)); // Uses point-sized, and can be stepped over
	UTIL_SetOrigin(pev, pev->origin);

	SetTouch(&CThrowableSatchelCharge::SatchelSlide);
	SetUse(&CThrowableSatchelCharge::SatchelUse);
	SetThink(&CThrowableSatchelCharge::SatchelThink);
	pev->nextthink = gpGlobals->time + 0.1;

	pev->team = ZP::TEAM_SURVIVIOR;
	pev->gravity = 0.5;
	pev->friction = 0.8;

	// ResetSequenceInfo( );
	pev->sequence = 1;
	m_flExplodeRange = 300.0f;
}

void CThrowableSatchelCharge::SatchelSlide(CBaseEntity *pOther)
{
	entvars_t *pevOther = pOther->pev;

	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// pev->avelocity = Vector (300, 300, 300);
	pev->gravity = 1; // normal gravity now

	// HACKHACK - On ground isn't always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 10), ignore_monsters, edict(), &tr);

	if (tr.flFraction < 1.0)
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;
		pev->avelocity = pev->avelocity * 0.9;
		// play sliding sound, volume based on velocity
	}
	if (!(pev->flags & FL_ONGROUND) && pev->velocity.Length2D() > 10)
	{
		// Fix for a bug in engine: when object isn't moving, but its speed isn't 0 and on ground isn't set
		if (pev->origin != m_lastBounceOrigin)
		{
			BounceSound();
		}
	}
	m_lastBounceOrigin = pev->origin;
	// There is no model animation so commented this out to prevent net traffic
	//StudioFrameAdvance( );
}

void CThrowableSatchelCharge ::SatchelThink(void)
{
	// There is no model animation so commented this out to prevent net traffic
	//StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	if (pev->waterlevel == 3)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * 0.8;
		pev->avelocity = pev->avelocity * 0.9;
		pev->velocity.z += 8;
	}
	else if (pev->waterlevel == 0)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		pev->velocity.z -= 8;
	}

	if ( m_flDisallowPickup != -1 && m_flDisallowPickup - gpGlobals->time <= 0 )
	{
		m_flDisallowPickup = -1;
		m_iThrower = pev->owner ? ENTINDEX( pev->owner ) : 0;
		pev->owner = nullptr;
	}
}

void CThrowableSatchelCharge::SatchelUse(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if ( useType == USE_SET )
	{
#ifndef CLIENT_DLL
		// Allow for pickup! (if we can)
		CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>( pCaller );
		if ( m_iThrower != -1 && pPlayer && pPlayer->IsPlayer() && pPlayer->pev->team == ZP::TEAM_SURVIVIOR )
		{
			CBasePlayer *pOwner = (CBasePlayer *)UTIL_PlayerByIndex( m_iThrower );
			bool bCanBePickedUp = pOwner ? false : true;
			if ( pOwner && pOwner->pev->team != pev->team )
				bCanBePickedUp = true;
			else if ( pPlayer->edict() == pOwner->edict() )
				bCanBePickedUp = true;
			if ( bCanBePickedUp )
			{
				CBasePlayerWeapon *pWeapon = pPlayer->GetWeaponFromID( WEAPON_SATCHEL );
				if ( pWeapon )
				{
					// If we already have one, stop.
					if ( pWeapon->m_iClip > 0 ) return;
					pWeapon->OnClipIncrease( 1 );
					EMIT_SOUND( ENT(pPlayer->pev), CHAN_ITEM, "items/ammo_pickup.wav", 1, ATTN_NORM );
				}
				else
					pPlayer->GiveNamedItem( "weapon_ied" );
				SetUse( NULL );
				SetTouch( NULL );
				SetThink( &CThrowableSatchelCharge::SUB_Remove );
				return;
			}
		}
#endif
	}
}

#ifndef CLIENT_DLL
void CThrowableSatchelCharge::IEDExplode()
{
	if ( m_iThrower != -1 )
	{
		CBasePlayer *pOwner = (CBasePlayer *)UTIL_PlayerByIndex( m_iThrower );
		pev->owner = pOwner->edict();
	}

	TraceResult tr;
	Vector vecSpot; // trace starts here!

	vecSpot = pev->origin + Vector(0, 0, 8);
	UTIL_TraceLine(vecSpot, pev->origin + Vector(0, 0, -32), ignore_monsters, ENT(pev), &tr);

	float flRndSound; // sound randomizer

	pev->model = iStringNull; //invisible
	pev->solid = SOLID_NOT; // intangible

	pev->takedamage = DAMAGE_NO;

	int iContents = UTIL_PointContents(vecSpot);

	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, vecSpot);
	WRITE_BYTE(TE_EXPLOSION); // This makes a dynamic light and the explosion sprites/sound
	WRITE_COORD(vecSpot.x); // Send to PAS because of the sound
	WRITE_COORD(vecSpot.y);
	WRITE_COORD(vecSpot.z);
	if (iContents != CONTENTS_WATER)
		WRITE_SHORT(g_sModelIndexFireball);
	else
		WRITE_SHORT(g_sModelIndexWExplosion);
	WRITE_BYTE((150 - 50) * .60); // scale * 10
	WRITE_BYTE(15); // framerate
	WRITE_BYTE(TE_EXPLFLAG_NOSOUND);
	MESSAGE_END();

	EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "weapons/explosion1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );

	CSoundEnt::InsertSound( bits_SOUND_COMBAT, vecSpot, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS(pev->owner);
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set
	pev->team = pevOwner ? pevOwner->team : ZP::TEAM_SURVIVIOR;

	RadiusDamage( pev, pevOwner, 500, CLASS_NONE, DMG_BLAST, m_flExplodeRange );

	if (RANDOM_FLOAT(0, 1) < 0.5)
		UTIL_DecalTrace( &tr, DECAL_SCORCH1 );
	else
		UTIL_DecalTrace( &tr, DECAL_SCORCH2 );

	flRndSound = RANDOM_FLOAT(0, 1);

	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM);
		break;
	}

	pev->effects |= EF_NODRAW;
	SetThink(&CGrenade::Smoke);
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER)
	{
		int sparkCount = RANDOM_LONG(0, 3);
		for (int i = 0; i < sparkCount; i++)
			Create("spark_shower", pev->origin, tr.vecPlaneNormal, NULL);
	}
}

void CThrowableSatchelCharge::PlantIED()
{
	SET_MODEL( ENT(pev), "models/w_satchel_planted.mdl" );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_NOT;
	pev->body = 0;
	SetTouch( NULL );
	SetUse( NULL );
	SetThink( &CThrowableSatchelCharge::PowerupThink );
	pev->nextthink = gpGlobals->time + 1.0;
	m_flNextBeep = gpGlobals->time + 1.0;
	m_bHasBeeped = false;
}

void CThrowableSatchelCharge::PowerupThink( void )
{
	if ( pev->solid == SOLID_NOT )
	{
		pev->solid = SOLID_BBOX;
		//UTIL_SetSize( pev, Vector(-4, -4, -4), Vector(4, 4, 4) );
		SetUse( &CThrowableSatchelCharge::SatchelUse );
	}

	if ( m_flDisallowPickup != -1 && m_flDisallowPickup - gpGlobals->time <= 0 )
	{
		m_flDisallowPickup = -1;
		m_iThrower = pev->owner ? ENTINDEX( pev->owner ) : 0;
		pev->owner = nullptr;
	}

	if ( m_bHasBeeped )
	{
		m_bHasBeeped = false;
		pev->body = 0;
	}

	if ( m_flNextBeep - gpGlobals->time <= 0 )
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_ITEM, "weapons/ied/beep.wav", 0.6, ATTN_NORM, 0, 25 );
		m_flNextBeep = gpGlobals->time + 2.0;
		m_bHasBeeped = true;
		pev->body = 1;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}
#endif

void CThrowableSatchelCharge ::Precache(void)
{
	PRECACHE_MODEL("models/w_satchel_planted.mdl");
	PRECACHE_SOUND("weapons/ied/beep.wav");
	PRECACHE_SOUND("weapons/ied/bounce1.wav");
	PRECACHE_SOUND("weapons/ied/bounce2.wav");
	PRECACHE_SOUND("weapons/ied/bounce3.wav");
}

void CThrowableSatchelCharge ::BounceSound(void)
{
	switch (RANDOM_LONG(0, 2))
	{
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/ied/bounce1.wav", 1, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/ied/bounce2.wav", 1, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/ied/bounce3.wav", 1, ATTN_NORM);
		break;
	}
}