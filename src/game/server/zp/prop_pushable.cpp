// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "triggers.h"
#include "player.h"
#include "func_break.h" // for CPushable::m_soundNames

class CPropPushable : public CBreakable
{
	SET_BASECLASS( CBreakable );

public:
	void Spawn() override;
	void Precache() override;
	void Restart();
	void KeyValue( KeyValueData *pkvd ) override;
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps(void) { return CBaseEntity::ObjectCaps() | FCAP_MUST_RESET | FCAP_CONTINUOUS_USE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Move( CBaseEntity *pMover, int push );

	void EXPORT OnPlayerTouch( CBaseEntity *pOther );

	float MaxSpeed() const { return m_maxSpeed; }

	bool IsUseableBrush() const override { return true; }

protected:
	void SetSequenceBox();
	int ExtractBbox(int sequence, float *mins, float *maxs);

private:
	float m_maxSpeed;
	float m_soundTime;

	int m_lastSound;

	Vector oldPos;
	float m_OldFriction;
};
LINK_ENTITY_TO_CLASS( prop_pushable, CPropPushable );


void CPropPushable::Precache()
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );
	for (int i = 0; i < 3; i++)
		PRECACHE_SOUND( CPushable::m_soundNames[i] );
	BaseClass::Precache();
}


void CPropPushable::Spawn()
{
	// Make sure we also read the baseclass spawn func
	BaseClass::Spawn();

	Precache();

	SET_MODEL( ENT(pev), STRING(pev->model) );

	pev->sequence = 0;
	SetSequenceBox(); // Copied from CBaseAnimating

	// Just add extra 26 if Z is 1...
	if ( pev->maxs.z == 1 )
		pev->maxs.z += 26;

	m_flHealth = pev->health;
	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;

	if (pev->friction > 399)
		pev->friction = 399;

	m_OldFriction = pev->friction;
	m_maxSpeed = 400 - pev->friction;
	SetBits(pev->flags, FL_FLOAT);
	pev->friction = 0;

	pev->origin.z += 1; // Pick up off of the floor
	UTIL_SetOrigin(pev, pev->origin);

	oldPos = pev->origin;
	m_soundTime = 0;

	m_bIsDestroyed = false;

	SetTouch( &CPropPushable::OnPlayerTouch );
}


void CPropPushable::Restart()
{
	BaseClass::Restart();

	SET_MODEL(ENT(pev), STRING(pev->model));

	pev->sequence = 0;
	SetSequenceBox(); // Copied from CBaseAnimating

	pev->health = m_flHealth;
	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;

	m_maxSpeed = 400 - m_OldFriction;
	SetBits(pev->flags, FL_FLOAT);
	pev->friction = 0;

	UTIL_SetOrigin(pev, oldPos);

	m_soundTime = 0;
	m_bIsDestroyed = false;

	SetTouch( &CPropPushable::OnPlayerTouch );
}


void CPropPushable::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "model" ) )
		pev->model = ALLOC_STRING( pkvd->szValue );
	else
		BaseClass::KeyValue( pkvd );
}


void CPropPushable::OnPlayerTouch( CBaseEntity *pOther )
{
	if ( FClassnameIs( pOther->pev, "worldspawn" ) ) return;
	Move( pOther, 1 );
}


void CPropPushable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_bIsDestroyed ) return;
	if (!pActivator || !pActivator->IsPlayer())
	{
		this->BaseClass::Use( pActivator, pCaller, useType, value );
		return;
	}

	if ( pActivator->pev->velocity != g_vecZero )
		Move( pActivator, 0 );
}


int CPropPushable::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_bIsDestroyed ) return 0;
	return BaseClass::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}


void CPropPushable::Move( CBaseEntity *pOther, int push )
{
	entvars_t *pevToucher = pOther->pev;
	int playerTouch = 0;

	// Is entity standing on this pushable ?
	if (FBitSet(pevToucher->flags, FL_ONGROUND) && pevToucher->groundentity && VARS(pevToucher->groundentity) == pev)
	{
		// Only push if floating
		if (pev->waterlevel > 0)
			pev->velocity.z += pevToucher->velocity.z * 0.1;

		return;
	}

	if (pOther->IsPlayer())
	{
		// JoshA: Used to check for FORWARD too and logic was inverted
		// from comment which seems wrong.
		// Fixed to just check for USE being not set for PUSH.
		// Should have the right effect.
		if (push && !!(pevToucher->button & IN_USE)) // Don't push unless the player is not useing (pull)
			return;
		playerTouch = 1;
	}

	float factor;

	if (playerTouch)
	{
		if (!(pevToucher->flags & FL_ONGROUND)) // Don't push away from jumping/falling players unless in water
		{
			if (pev->waterlevel < 1)
				return;
			else
				factor = 0.1;
		}
		else
			factor = 1;
	}
	else
		factor = 0.25;

	// This used to be added every 'frame', but to be consistent at high fps,
	// now act as if it's added at a constant rate with a fudge factor.
	extern cvar_t sv_pushable_fixed_tick_fudge;

	if (!push && sv_pushable_fixed_tick_fudge.value >= 0.0f)
	{
		factor *= gpGlobals->frametime * sv_pushable_fixed_tick_fudge.value;
	}

	// JoshA: Always apply this if pushing, or if under the player's velocity.
	if (push || (abs(pev->velocity.x) < abs(pevToucher->velocity.x - pevToucher->velocity.x * factor)))
		pev->velocity.x += pevToucher->velocity.x * factor;
	if (push || (abs(pev->velocity.y) < abs(pevToucher->velocity.y - pevToucher->velocity.y * factor)))
		pev->velocity.y += pevToucher->velocity.y * factor;

	float length = sqrt(pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y);
	if (length > MaxSpeed())
	{
		pev->velocity.x = (pev->velocity.x * MaxSpeed() / length);
		pev->velocity.y = (pev->velocity.y * MaxSpeed() / length);
	}
	if (playerTouch)
	{
		// JoshA: Match the player to our pushable's velocity.
		// Previously this always happened, but it should only
		// happen if the player is pushing (or rather, being pushed.)
		// This either stops the player in their tracks or nudges them along.
		if (push)
		{
			pevToucher->velocity.x = pev->velocity.x;
			pevToucher->velocity.y = pev->velocity.y;
		}

		if ((gpGlobals->time - m_soundTime) > 0.7)
		{
			m_soundTime = gpGlobals->time;
			if (length > 0 && FBitSet(pev->flags, FL_ONGROUND))
			{
				m_lastSound = RANDOM_LONG(0, 2);
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, CPushable::m_soundNames[m_lastSound], 0.5, ATTN_NORM);
				//			SetThink( StopSound );
				//			pev->nextthink = pev->ltime + 0.1;
			}
			else
				STOP_SOUND(ENT(pev), CHAN_WEAPON, CPushable::m_soundNames[m_lastSound]);
		}
	}
}

//=========================================================
//=========================================================

extern int ExtractBbox( void *pmodel, int sequence, float *mins, float *maxs );

int CPropPushable::ExtractBbox( int sequence, float *mins, float *maxs )
{
	return ::ExtractBbox( GET_MODEL_PTR(ENT(pev)), sequence, mins, maxs );
}

void CPropPushable::SetSequenceBox()
{
	Vector mins, maxs;

	// Get sequence bbox
	if ( ExtractBbox( pev->sequence, mins, maxs ) )
	{
		// expand box for rotation
		// find min / max for rotations
		float yaw = pev->angles.y * (M_PI / 180.0);

		Vector xvector, yvector;
		xvector.x = cos(yaw);
		xvector.y = sin(yaw);
		yvector.x = -sin(yaw);
		yvector.y = cos(yaw);
		Vector bounds[2];

		bounds[0] = mins;
		bounds[1] = maxs;

		Vector rmin(9999, 9999, 9999);
		Vector rmax(-9999, -9999, -9999);
		Vector base, transformed;

		for (int i = 0; i <= 1; i++)
		{
			base.x = bounds[i].x;
			for (int j = 0; j <= 1; j++)
			{
				base.y = bounds[j].y;
				for (int k = 0; k <= 1; k++)
				{
					base.z = bounds[k].z;

					// transform the point
					transformed.x = xvector.x * base.x + yvector.x * base.y;
					transformed.y = xvector.y * base.x + yvector.y * base.y;
					transformed.z = base.z;

					if (transformed.x < rmin.x)
						rmin.x = transformed.x;
					if (transformed.x > rmax.x)
						rmax.x = transformed.x;
					if (transformed.y < rmin.y)
						rmin.y = transformed.y;
					if (transformed.y > rmax.y)
						rmax.y = transformed.y;
					if (transformed.z < rmin.z)
						rmin.z = transformed.z;
					if (transformed.z > rmax.z)
						rmax.z = transformed.z;
				}
			}
		}
		UTIL_SetSize(pev, rmin, rmax);
	}
}
