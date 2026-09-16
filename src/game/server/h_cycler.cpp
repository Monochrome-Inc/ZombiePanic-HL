/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== h_cycler.cpp ========================================================

  The Halflife Cycler Monsters

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "weapons.h"
#include "player.h"
#include "zp/info_beacon.h"
#ifdef SCRIPT_SYSTEM
#include "core.h"
#endif

#define TEMP_FOR_SCREEN_SHOTS
#ifdef TEMP_FOR_SCREEN_SHOTS //===================================================

class CCycler : public CBaseMonster
{
public:
	void GenericCyclerSpawn(char *szModel, Vector vecMin, Vector vecMax);
	virtual int ObjectCaps(void) { return (CBaseEntity ::ObjectCaps() | FCAP_IMPULSE_USE); }
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	void Spawn(void);
	void Restart(void);
	virtual void Think(void);
	//void Pain( float flDamage );
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);

	// Don't treat as a live target
	virtual BOOL IsAlive(void) { return FALSE; }
	virtual BOOL IsAllowedToSpeak() { return TRUE; }

	virtual int Save(CSave &save);
	virtual int Restore(CRestore &restore);
	static TYPEDESCRIPTION m_SaveData[];

	int m_animate;

	int m_renderfx;
	int m_rendermode;
	float m_renderamt;
	Vector m_rendercolor;

	Vector m_origspawn;
	Vector m_origangle;
};

TYPEDESCRIPTION CCycler::m_SaveData[] = {
	DEFINE_FIELD(CCycler, m_animate, FIELD_INTEGER),
};

IMPLEMENT_SAVERESTORE(CCycler, CBaseMonster);

//
// we should get rid of all the other cyclers and replace them with this.
//
class CGenericCycler : public CCycler
{
public:
	void Spawn(void) { GenericCyclerSpawn((char *)STRING(pev->model), Vector(-16, -16, 0), Vector(16, 16, 72)); }
};
LINK_ENTITY_TO_CLASS(cycler, CGenericCycler);

//
// Prop Objective
//
class CPropObjective : public CGenericCycler
{
public:
	void Spawn(void)
	{
		CGenericCycler::Spawn();
#ifdef SCRIPT_SYSTEM
		// Outputs
		ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, this, "OnUse" );
#endif

		pev->movetype = MOVETYPE_TOSS;
		pev->solid = SOLID_BBOX;
		UTIL_SetSize( pev, Vector(0, 0, 0), Vector(0, 0, 0) );
		pev->classname = MAKE_STRING( "prop_objective" );

		if ( FStringNull( m_iszBeacon ) == FALSE )
		{
			m_bApplyBeaconOffset = true;
			SetThink( &CPropObjective::SetBeaconParent );
			pev->nextthink = gpGlobals->time + 2.5f;
		}
	}
	void Restart()
	{
		CGenericCycler::Restart();
		if ( FStringNull( m_iszBeacon ) == FALSE )
		{
			SetThink( &CPropObjective::SetBeaconParent );
			pev->nextthink = gpGlobals->time;
		}
	}
	void Think(void)
	{
		CGenericCycler::Think();
		CBaseEntity::Think();
	}
	void EXPORT SetBeaconParent();
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) override;
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "beacon" ) )
		{
			m_iszBeacon = ALLOC_STRING(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else
			CGenericCycler::KeyValue( pkvd );
	}
private:
	string_t m_iszBeacon;
	Vector m_vecBeaconOffset;
	bool m_bApplyBeaconOffset;
};
LINK_ENTITY_TO_CLASS( prop_objective, CPropObjective );


void CPropObjective::SetBeaconParent()
{
	CBaseEntity *pEnt = UTIL_FindEntityByTargetname( nullptr, STRING(m_iszBeacon) );
	if ( pEnt )
	{
		if ( m_bApplyBeaconOffset )
		{
			// Apply offset from beacon to prop objective
			m_vecBeaconOffset = pEnt->pev->origin - pev->origin;
			Vector vecDesiredOrigin = pEnt->pev->origin + m_vecBeaconOffset;
			UTIL_SetOrigin( pev, vecDesiredOrigin );
			m_bApplyBeaconOffset = false;
		}
		else
		{
			Vector vecDesiredOrigin = m_vecBeaconOffset + pev->origin;
			Vector vecCurrentOrigin = pEnt->pev->origin;
			UTIL_SetOrigin( pEnt->pev, vecDesiredOrigin );
			bool bIsCorrectOrigin = true;

			// If our previous desired origin is not the same as our current origin, update the beacon state
			if ( vecDesiredOrigin != vecCurrentOrigin )
				bIsCorrectOrigin = false;

			// Tell our client that something has changed
			CInfoBeacon *pBeacon = dynamic_cast<CInfoBeacon *>(pEnt);
			if ( pBeacon && !bIsCorrectOrigin )
				pBeacon->UpdateMessageState();
		}
	}
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CPropObjective::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator->pev->team != ZP::TEAM_SURVIVIOR ) return;
	if ( pev->effects == EF_NODRAW ) return;
	SoftRemove();

	// Turn off the beacon
	if ( FStringNull( m_iszBeacon ) == FALSE )
	{
		CBaseEntity *pEnt = UTIL_FindEntityByTargetname( nullptr, STRING(m_iszBeacon) );
		if ( pEnt )
		{
			CInfoBeacon *pBeacon = dynamic_cast<CInfoBeacon *>(pEnt);
			if ( pBeacon )
				pBeacon->Use( pActivator, this, USE_OFF, 0.0f );
		}
	}

#ifdef SCRIPT_SYSTEM
	CBaseEntity *m_pActivator = pActivator;
	if ( !m_pActivator ) m_pActivator = this;
	const std::string &szOutput( "OnUse" );
	ScriptSystem::CallScriptDelay(
		AvailableScripts_t::InputOutput,
		nullptr,
		szOutput,
		0.0f,
		2,
		std::to_string( entindex() ),
		std::to_string( m_pActivator->entindex() )
	);
#endif
}

// Probe droid imported for tech demo compatibility
//
// PROBE DROID
//
class CCyclerProbe : public CCycler
{
public:
	void Spawn(void);
};
LINK_ENTITY_TO_CLASS(cycler_prdroid, CCyclerProbe);
void CCyclerProbe ::Spawn(void)
{
	pev->origin = pev->origin + Vector(0, 0, 16);
	GenericCyclerSpawn("models/prdroid.mdl", Vector(-16, -16, -16), Vector(16, 16, 16));
}

// Cycler member functions

void CCycler ::GenericCyclerSpawn(char *szModel, Vector vecMin, Vector vecMax)
{
	if (!szModel || !*szModel)
	{
		ALERT(at_error, "cycler at %.0f %.0f %0.f missing modelname", pev->origin.x, pev->origin.y, pev->origin.z);
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	pev->classname = MAKE_STRING("cycler");
	PRECACHE_MODEL(szModel);
	SET_MODEL(ENT(pev), szModel);

	CCycler::Spawn();

	UTIL_SetSize(pev, vecMin, vecMax);
}

void CCycler ::Spawn()
{
	CBaseEntity::Spawn();

	InitBoneControllers();
#if defined( HL_CLIENT )
	pev->solid = SOLID_SLIDEBOX;
	pev->takedamage = DAMAGE_YES;
#else
	// Let's make sure cyclers is not solid on ZP1
	pev->solid = SOLID_NOT;
	// No damage
	pev->takedamage = DAMAGE_NO;
#endif
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->health = 80000; // no cycler should die
	pev->yaw_speed = 5;
	pev->ideal_yaw = pev->angles.y;
	ChangeYaw(360);

	m_flFrameRate = 75;
	m_flGroundSpeed = 0;

	pev->nextthink += 1.0;

	ResetSequenceInfo();

	if (pev->sequence != 0 || pev->frame != 0)
	{
		m_animate = 0;
		pev->framerate = 0;
	}
	else
	{
		m_animate = 1;
	}

	m_renderfx = pev->renderfx;
	m_rendermode = pev->rendermode;
	m_renderamt = pev->renderamt;
	m_rendercolor = pev->rendercolor;

	m_origspawn = pev->origin;
	m_origangle = pev->angles;
}

void CCycler ::Restart(void)
{
	pev->effects = 0;
	pev->frame = 0;
	pev->sequence = 0;
	m_animate = 1;
	pev->framerate = 1.0;
	ResetSequenceInfo();
	pev->nextthink = gpGlobals->time + 0.1;

	pev->renderfx = m_renderfx;
	pev->rendermode = m_rendermode;
	pev->renderamt = m_renderamt;
	pev->rendercolor = m_rendercolor;

	UTIL_SetOrigin( pev, m_origspawn );
	pev->angles = m_origangle;
}

//
// cycler think
//
void CCycler ::Think(void)
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_animate)
	{
		StudioFrameAdvance();
	}
	if (m_fSequenceFinished && !m_fSequenceLoops)
	{
		// ResetSequenceInfo();
		// hack to avoid reloading model every frame
		pev->animtime = gpGlobals->time;
		pev->framerate = 1.0;
		m_fSequenceFinished = FALSE;
		m_flLastEventCheck = gpGlobals->time;
		pev->frame = 0;
		if (!m_animate)
			pev->framerate = 0.0; // FIX: don't reset framerate
	}
}

//
// CyclerUse - starts a rotation trend
//
void CCycler ::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	m_animate = !m_animate;
	if (m_animate)
		pev->framerate = 1.0;
	else
		pev->framerate = 0.0;
}

//
// CyclerPain , changes sequences when shot
//
//void CCycler :: Pain( float flDamage )
int CCycler ::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_animate)
	{
		pev->sequence++;

		ResetSequenceInfo();

		if (m_flFrameRate == 0.0)
		{
			pev->sequence = 0;
			ResetSequenceInfo();
		}
		pev->frame = 0;
	}
	else
	{
		pev->framerate = 1.0;
		StudioFrameAdvance(0.1);
		pev->framerate = 0;
		ALERT(at_console, "sequence: %d, frame %.0f\n", pev->sequence, pev->frame);
	}

	return 0;
}

#endif

class CCyclerSprite : public CBaseEntity
{
public:
	void Spawn(void);
	void Restart(void);
	void Think(void);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return (CBaseEntity ::ObjectCaps() | FCAP_DONT_SAVE | FCAP_IMPULSE_USE); }
	virtual int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	void Animate(float frames);

	virtual int Save(CSave &save);
	virtual int Restore(CRestore &restore);
	static TYPEDESCRIPTION m_SaveData[];

	inline int ShouldAnimate(void) { return m_animate && m_maxFrame > 1.0; }
	int m_animate;
	float m_lastTime;
	float m_maxFrame;

	int m_renderfx;
	int m_rendermode;
	float m_renderamt;
	Vector m_rendercolor;
};

LINK_ENTITY_TO_CLASS(cycler_sprite, CCyclerSprite);

TYPEDESCRIPTION CCyclerSprite::m_SaveData[] = {
	DEFINE_FIELD(CCyclerSprite, m_animate, FIELD_INTEGER),
	DEFINE_FIELD(CCyclerSprite, m_lastTime, FIELD_TIME),
	DEFINE_FIELD(CCyclerSprite, m_maxFrame, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CCyclerSprite, CBaseEntity);

void CCyclerSprite::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_YES;
	pev->effects = 0;

	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.1;
	m_animate = 1;
	m_lastTime = gpGlobals->time;

	PRECACHE_MODEL((char *)STRING(pev->model));
	SET_MODEL(ENT(pev), STRING(pev->model));

	m_maxFrame = (float)MODEL_FRAMES(pev->modelindex) - 1;

	m_renderfx = pev->renderfx;
	m_rendermode = pev->rendermode;
	m_renderamt = pev->renderamt;
	m_rendercolor = pev->rendercolor;
}

void CCyclerSprite::Restart()
{
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_YES;
	pev->effects = 0;

	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.1f;
	m_animate = 1;
	m_lastTime = gpGlobals->time;

	pev->renderfx = m_renderfx;
	pev->rendermode = m_rendermode;
	pev->renderamt = m_renderamt;
	pev->rendercolor = m_rendercolor;
}

void CCyclerSprite::Think(void)
{
	if (ShouldAnimate())
		Animate(pev->framerate * (gpGlobals->time - m_lastTime));

	pev->nextthink = gpGlobals->time + 0.1;
	m_lastTime = gpGlobals->time;
}

void CCyclerSprite::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	m_animate = !m_animate;
	ALERT(at_console, "Sprite: %s\n", STRING(pev->model));
}

int CCyclerSprite::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_maxFrame > 1.0)
	{
		Animate(1.0);
	}
	return 1;
}

void CCyclerSprite::Animate(float frames)
{
	pev->frame += frames;
	if (m_maxFrame > 0)
		pev->frame = fmod(pev->frame, m_maxFrame);
}

class CWeaponCycler : public CBasePlayerWeapon
{
public:
	void Spawn(void);
	int iItemSlot(void) { return 1; }
	ZPWeaponID GetWeaponID() override { return WEAPON_NONE; }
	int GetItemInfo(ItemInfo *p) { return 0; }

	void PrimaryAttack(void);
	void SecondaryAttack(void);
	float Deploy(void);
	void Holster(int skiplocal = 0);
	int m_iszModel;
	int m_iModel;
};
LINK_ENTITY_TO_CLASS(cycler_weapon, CWeaponCycler);

void CWeaponCycler::Spawn()
{
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_NONE;

	PRECACHE_MODEL((char *)STRING(pev->model));
	SET_MODEL(ENT(pev), STRING(pev->model));
	m_iszModel = pev->model;
	m_iModel = pev->modelindex;

	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	SetTouch(&CWeaponCycler::DefaultTouch);
}

float CWeaponCycler::Deploy()
{
	m_pPlayer->pev->viewmodel = m_iszModel;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	SendWeaponAnim(0);
	m_iClip = 0;
	return 0.0f;
}

void CWeaponCycler::Holster(int skiplocal /* = 0 */)
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
}

void CWeaponCycler::PrimaryAttack()
{

	SendWeaponAnim(pev->sequence);

	m_flNextPrimaryAttack = gpGlobals->time + 0.3;
}

void CWeaponCycler::SecondaryAttack(void)
{
	float flFrameRate, flGroundSpeed;

	pev->sequence = (pev->sequence + 1) % 8;

	pev->modelindex = m_iModel;
	void *pmodel = GET_MODEL_PTR(ENT(pev));
	GetSequenceInfo(pmodel, pev, &flFrameRate, &flGroundSpeed);
	pev->modelindex = 0;

	if (flFrameRate == 0.0)
	{
		pev->sequence = 0;
	}

	SendWeaponAnim(pev->sequence);

	m_flNextSecondaryAttack = gpGlobals->time + 0.3;
}

// Flaming Wreakage
class CWreckage : public CBaseMonster
{
	int Save(CSave &save);
	int Restore(CRestore &restore);
	static TYPEDESCRIPTION m_SaveData[];

	void Spawn(void);
	void Precache(void);
	void Think(void);

	int m_flStartTime;
};
TYPEDESCRIPTION CWreckage::m_SaveData[] = {
	DEFINE_FIELD(CWreckage, m_flStartTime, FIELD_TIME),
};
IMPLEMENT_SAVERESTORE(CWreckage, CBaseMonster);

LINK_ENTITY_TO_CLASS(cycler_wreckage, CWreckage);

void CWreckage::Spawn(void)
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = 0;
	pev->effects = 0;

	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->model)
	{
		PRECACHE_MODEL((char *)STRING(pev->model));
		SET_MODEL(ENT(pev), STRING(pev->model));
	}
	// pev->scale = 5.0;

	m_flStartTime = gpGlobals->time;
}

void CWreckage::Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char *)STRING(pev->model));
}

void CWreckage::Think(void)
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.2;

	if (pev->dmgtime)
	{
		if (pev->dmgtime < gpGlobals->time)
		{
			UTIL_Remove(this);
			return;
		}
		else if (RANDOM_FLOAT(0, pev->dmgtime - m_flStartTime) > pev->dmgtime - gpGlobals->time)
		{
			return;
		}
	}

	Vector VecSrc;

	VecSrc.x = RANDOM_FLOAT(pev->absmin.x, pev->absmax.x);
	VecSrc.y = RANDOM_FLOAT(pev->absmin.y, pev->absmax.y);
	VecSrc.z = RANDOM_FLOAT(pev->absmin.z, pev->absmax.z);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, VecSrc);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(VecSrc.x);
	WRITE_COORD(VecSrc.y);
	WRITE_COORD(VecSrc.z);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(RANDOM_LONG(0, 49) + 50); // scale * 10
	WRITE_BYTE(RANDOM_LONG(0, 3) + 8); // framerate
	MESSAGE_END();
}
