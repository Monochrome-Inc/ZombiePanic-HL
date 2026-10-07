// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "triggers.h"
#include "player.h"
#include "func_break.h"
#include "zp/weapons/CWeaponBase.h"
#include "zp/weapons/weapon_misc_nailgun.h"
#ifdef SCRIPT_SYSTEM
#include "core.h"
#endif

extern void SetBodygroup( void *pmodel, entvars_t *pev, int iGroup, int iValue );
extern int gmsgBarricadeBuildProgress;

#define BGROUP_BODY 0
#define BGROUP_SUB_HEALTHY 0
#define BGROUP_SUB_DAMAGED 1
#define BGROUP_SUB_DESTROYED 2

#define SF_BARRICADE_START_BUILT (1<<0)		// Start already built?
#define SF_BARRICADE_LARGE       (1 << 1)	// Large barricade (longer build time, and more health)

static char *s_blockClasses[] =
{
	"player",
	"prop_pushable",
	"func_pushable",
	"" // End of list
};

class CPropBarricade : public CBreakable
{
	SET_BASECLASS( CBreakable );

public:
	void Spawn() override;
	void Precache() override;
	void SoftRemove() override;
	void Restart();
	void KeyValue( KeyValueData *pkvd ) override;
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps(void) { return CBaseEntity::ObjectCaps() | FCAP_MUST_RESET | FCAP_CONTINUOUS_USE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	EXPORT void OnBarricading();
	void StopBuilding();
	void SetBarricadeMode();
	bool CanBuildBarricade();
	void OnScriptCallBack(KeyValues *pData);

protected:
	void SetSequenceBox();
	void GetSequenceBox( Vector &vMin, Vector &vMax );
	int ExtractBbox(int sequence, float *mins, float *maxs);
	void OnBarricadeBuilt( bool bPreBuilt );
	void ResetPlayerInfo( CBasePlayer *pPlayer );
	bool IsPlayerUsingNailgun( CBasePlayer *pPlayer );

private:
	enum BuildingState
	{
		NOT_BUILT = 0,
		BUILDING,
		BUILT
	};
	BuildingState m_eIsBuilt;
	int m_nBuilder;
	float m_flBuildTime;
	float m_flBuildStartTime;

public:
	enum BuildingSound
	{
		SND_NONE = -1,
		SND_BUILDING = 0,
		SND_BUILDING_LONG,
		SND_BUILT,
		SND_MAX
	};
	static char *m_BarricadeSounds[SND_MAX];
	BuildingSound m_eBarricadeSnd;
	void PlayBarricadeSound( BuildingSound snd );
	int m_nPlayerViewModel;
	int m_nPlayerWorldModel;
};
LINK_ENTITY_TO_CLASS( prop_barricade, CPropBarricade );

char *CPropBarricade::m_BarricadeSounds[SND_MAX] = {
	"barricade/building.wav",
	"barricade/building_long.wav",
	"barricade/built.wav"
};

void CPropBarricade::Precache()
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );
	for (int i = 0; i < 3; i++)
		PRECACHE_SOUND( CPushable::m_soundNames[i] );
	for (int i = 0; i < SND_MAX; i++)
		PRECACHE_SOUND( CPropBarricade::m_BarricadeSounds[i] );
	BaseClass::Precache();
}


void CPropBarricade::SoftRemove()
{
	BaseClass::SoftRemove();
	pev->effects = 0;
	SetBodygroup( GET_MODEL_PTR(ENT(pev)), pev, BGROUP_BODY, BGROUP_SUB_DESTROYED );
	FireEntityOutput( this, "OnBarricadeDestroyed" );
}


void CPropBarricade::Spawn()
{
	Precache();

	SET_MODEL( ENT(pev), STRING(pev->model) );

	SetBodygroup( GET_MODEL_PTR(ENT(pev)), pev, BGROUP_BODY, BGROUP_SUB_HEALTHY );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	pev->movetype = MOVETYPE_FLY;
	pev->deadflag = DEAD_DEAD;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->skin = CONTENTS_BARRICADE_NOT_BUILT;
	pev->rendermode = kRenderTransAlpha;

	// Default build time
	m_flBuildTime = 1.6f;

	if (pev->friction > 399)
		pev->friction = 399;

	UTIL_SetOrigin(pev, pev->origin);

	m_bIsDestroyed = false;
	m_eIsBuilt = NOT_BUILT;
	m_nBuilder = 0;

	SetBarricadeMode();

	// Pre-Built?
	if ( pev->spawnflags & SF_BARRICADE_START_BUILT )
		OnBarricadeBuilt( true );

#ifdef SCRIPT_SYSTEM
	// Outputs
	ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, this, "OnBarricadePreBuilt" );
	ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, this, "OnBarricadeBuilt" );
	ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, this, "OnBarricadeDestroyed" );

	// Inputs
	ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, this, "PlaceBarricade" );
	ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, this, "SetHealth" );
	ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, this, "Break" );

	SetEntityScriptCallback( &CPropBarricade::OnScriptCallBack );
#endif
}


void CPropBarricade::OnScriptCallBack( KeyValues *pData )
{
	const char *szAction = pData->GetString( "Action" );
	const char *szValue = pData->GetString( "arg0" );
	// Check what kind of action we got
	if ( FStrEq( szAction, "Break" ) )
		TakeDamage( pev, pev, pev->health + 1, DMG_CLUB );
	else if ( FStrEq( szAction, "SetHealth" ) )
		pev->health = atoi( szValue );
	else if ( FStrEq( szAction, "PlaceBarricade" ) )
		OnBarricadeBuilt( true );
}

void CPropBarricade::Restart()
{
	SET_MODEL(ENT(pev), STRING(pev->model));

	SetBodygroup( GET_MODEL_PTR(ENT(pev)), pev, BGROUP_BODY, BGROUP_SUB_HEALTHY );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	pev->deadflag = DEAD_DEAD;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->rendermode = kRenderTransAlpha;

	SetBarricadeMode();

	// Make sure skin is set to not built, so it appears invisible for zombies.
	// But if we have some woodenboards "ammo" we will be able to see and build it.
	pev->skin = CONTENTS_BARRICADE_NOT_BUILT;

	m_bIsDestroyed = false;
	m_eIsBuilt = NOT_BUILT;
	m_nBuilder = 0;

	// Pre-Built?
	if ( pev->spawnflags & SF_BARRICADE_START_BUILT )
		OnBarricadeBuilt( true );
}


void CPropBarricade::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "model" ) )
		pev->model = ALLOC_STRING( pkvd->szValue );
	else
		BaseClass::KeyValue( pkvd );
}


void CPropBarricade::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		return;
	if ( m_eIsBuilt > NOT_BUILT ) return;

	// Make sure the activator has wooden boards to build with, aka ZPAmmoTypes::AMMO_BARRICADE.
	CBasePlayer *pPlayer = static_cast<CBasePlayer *>( pActivator );
	if ( !pPlayer ) return;
	if ( pPlayer->AmmoInventory( ZPAmmoTypes::AMMO_BARRICADE ) <= 0 )
		return;

	if ( !CanBuildBarricade() ) return;

	// Make sure we are on the floor, we don't want players to be able to build barricades in mid-air :V
	if ( !(pPlayer->pev->flags & FL_ONGROUND) || !pPlayer->pev->groundentity ) return;

	// Is this a large barricade?
	bool bIsLarge = ( pev->spawnflags & SF_BARRICADE_LARGE );

	// Save the current viewmodel index
	m_nPlayerViewModel = pPlayer->pev->viewmodel;
	m_nPlayerWorldModel = pPlayer->pev->weaponmodel;
	float flBuildTime = m_flBuildTime;
	bool bUseNailgun = IsPlayerUsingNailgun( pPlayer );

	int iAnim = bIsLarge ? ANIM_BARRICADE_UNARMED2 : ANIM_BARRICADE_UNARMED1;
	if ( bUseNailgun )
	{
		flBuildTime = 1.33f;
		iAnim = ANIM_BARRICADE_NAILGUN_BARRICADE;
	}
	else
	{
		pPlayer->pev->weaponmodel = 0; // Hide world model
		pPlayer->pev->viewmodel = MAKE_STRING( "models/v_barricade.mdl" );
	}

	// Set the weapon animation
	pPlayer->pev->weaponanim = iAnim;
	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, pPlayer->pev );
	WRITE_BYTE( iAnim );
	WRITE_BYTE( 0 );
	MESSAGE_END();

	// Make sure it play properly
	CWeaponBase *pBaseWeapon = dynamic_cast<CWeaponBase *>( pPlayer->m_pActiveItem );
	if ( pBaseWeapon )
	{
		pBaseWeapon->m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flBuildTime;
		pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flBuildTime;
		if ( bUseNailgun )
		{
			pBaseWeapon->AddWeaponSound( "weapons/nailgun/nailgun_use1.wav", 1, ATTN_NORM, pBaseWeapon->GetAnimationTime( 12, 30 ) );
			pBaseWeapon->AddWeaponSound( "weapons/nailgun/nailgun_use2.wav", 1, ATTN_NORM, pBaseWeapon->GetAnimationTime( 19, 30 ) );
			pBaseWeapon->AddWeaponSound( "weapons/nailgun/nailgun_use3.wav", 1, ATTN_NORM, pBaseWeapon->GetAnimationTime( 29, 30 ) );
		}
	}

	m_eIsBuilt = BUILDING;
	m_nBuilder = pPlayer->entindex();
	m_flBuildStartTime = gpGlobals->time + flBuildTime;

	PLAYER_ANIM buildAnim = bIsLarge ? PLAYER_BARRICADE_LONG : PLAYER_BARRICADE;
	if ( bUseNailgun )
		buildAnim = PLAYER_BARRICADE_NAILGUN;

	pPlayer->m_Activity = ACT_RESET;
	pPlayer->SetAnimation( buildAnim );

	// Send build progress message to the player.
	MESSAGE_BEGIN( MSG_ONE, gmsgBarricadeBuildProgress, NULL, pPlayer->pev );
		WRITE_FLOAT( flBuildTime );
	MESSAGE_END();

	// Barricade sound
	if ( bUseNailgun )
		m_eBarricadeSnd = SND_NONE;
	else
		m_eBarricadeSnd = bIsLarge ? SND_BUILDING_LONG : SND_BUILDING;

	PlayBarricadeSound( m_eBarricadeSnd );

	SetThink( &CPropBarricade::OnBarricading );
	pev->nextthink = gpGlobals->time + 0.1f;
}


void CPropBarricade::OnBarricadeBuilt( bool bPreBuilt )
{
	CBasePlayer *pPlayer = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( m_nBuilder ) );
	if ( pPlayer )
	{
		ResetPlayerInfo( pPlayer );
		pPlayer->m_rgAmmo[ ZPAmmoTypes::AMMO_BARRICADE ] = 0; // Consume the wooden boards.
	}

	// No more thinking!
	SetThink( NULL );

	// Set bodygroup to healthy state.
	SetBodygroup( GET_MODEL_PTR(ENT(pev)), pev, BGROUP_BODY, BGROUP_SUB_HEALTHY );

	// Now, let's make sure we set the barricade to be solid.
	pev->solid = SOLID_BBOX;
	pev->deadflag = DEAD_NO;
	pev->skin = 0;
	pev->takedamage = DAMAGE_YES;
	pev->rendermode = kRenderNormal; // Make sure it's fully visible now.
	m_eIsBuilt = BUILT;
	m_nBuilder = 0;

	pev->sequence = 0;
	SetSequenceBox(); // Copied from CBaseAnimating

	// Pre built? Then no sounds
	if ( bPreBuilt )
	{
		FireEntityOutput( this, "OnBarricadePreBuilt" );
		return;
	}
	FireEntityOutput( this, "OnBarricadeBuilt" );

	// Let's play a simple button sound to indicate building is done.
	m_eBarricadeSnd = SND_BUILT;
	PlayBarricadeSound( m_eBarricadeSnd );
}

void CPropBarricade::ResetPlayerInfo( CBasePlayer *pPlayer )
{
	pPlayer->pev->viewmodel = m_nPlayerViewModel;
	pPlayer->pev->weaponmodel = m_nPlayerWorldModel;
	CWeaponBase *pBaseWeapon = dynamic_cast<CWeaponBase *>( pPlayer->m_pActiveItem );
	bool bUseNailgun = IsPlayerUsingNailgun( pPlayer );
	if ( pBaseWeapon )
	{
		pBaseWeapon->ClearWeaponSounds();
		if ( !bUseNailgun )
			pBaseWeapon->DoDeployAnimation();
		else
		{
			pBaseWeapon->m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
			pPlayer->m_flNextAttack = UTIL_WeaponTimeBase();
		}
	}
	pPlayer->m_Activity = ACT_RESET;
	pPlayer->SetAnimation( bUseNailgun ? PLAYER_IDLE : PLAYER_DRAW );
}

bool CPropBarricade::IsPlayerUsingNailgun( CBasePlayer *pPlayer )
{
	CWeaponBase *pBaseWeapon = dynamic_cast<CWeaponBase *>( pPlayer->m_pActiveItem );
	if ( pBaseWeapon && pBaseWeapon->GetWeaponID() == WEAPON_NAILGUN ) return true;
	return false;
}

void CPropBarricade::PlayBarricadeSound( BuildingSound snd )
{
	if ( snd == SND_NONE )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "common/null.wav", 0, ATTN_NORM, 0, 100 );
	else
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, m_BarricadeSounds[snd], 1.0, ATTN_NORM );
}


int CPropBarricade::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_bIsDestroyed ) return 0;
	int nPrevHealth = pev->health;
	int nRet = BaseClass::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	if ( nRet > 0 )
		SetBodygroup( GET_MODEL_PTR(ENT(pev)), pev, BGROUP_BODY, BGROUP_SUB_DAMAGED );
	return nRet;
}


void CPropBarricade::StopBuilding()
{
	CBasePlayer *pPlayer = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( m_nBuilder ) );
	if ( pPlayer )
	{
		ResetPlayerInfo( pPlayer );
		// Reset build progress.
		MESSAGE_BEGIN( MSG_ONE, gmsgBarricadeBuildProgress, NULL, pPlayer->pev );
			WRITE_FLOAT( 0.0f );
		MESSAGE_END();
	}

	// Stop the damn sound
	PlayBarricadeSound( SND_NONE );

	m_eIsBuilt = NOT_BUILT;
	m_nBuilder = 0;
	SetThink( NULL );
}

void CPropBarricade::SetBarricadeMode()
{
	int nHealth = 100;
	switch ( m_Material )
	{
		case matGlass: nHealth = 50; break;
		case matWood: nHealth = 200; break;
		case matMetal: nHealth = 270; break;
		case matFlesh: nHealth = 70; break;
		case matCinderBlock: nHealth = 250; break;
		case matCeilingTile: nHealth = 30; break;
		case matComputer: nHealth = 150; break;
		case matRocks: nHealth = 300; break;
	}

	float flBuildTime = 1.9f;

	// Add extra 50 if large barricade.
	if ( pev->spawnflags & SF_BARRICADE_LARGE )
	{
		nHealth += 50;
		flBuildTime = 2.45f;
	}

	pev->health = nHealth;
	m_flBuildTime = flBuildTime;
}

bool CPropBarricade::CanBuildBarricade()
{
	// Let's use UTIL_EntitiesInBox to check for entities in our bbox, since GoldSrc UTIL_TraceHull
	// does not have custom min/max size support...
	Vector mins, maxs;
	GetSequenceBox( mins, maxs );
	CBaseEntity *pList[32];
	int nCount = UTIL_EntitiesInBox( pList, 32, pev->origin + mins, pev->origin + maxs, 0 );
	for ( int i = 0; i < nCount; i++ )
	{
		// Ignore ourselves.
		if ( pList[i] == this ) continue;
		// Go through block classes.
		for ( int j = 0; s_blockClasses[j][0] != '\0'; j++ )
		{
			if ( FClassnameIs( pList[i]->pev, s_blockClasses[j] ) )
				return false;
		}
	}
	return true;
}


void CPropBarricade::OnBarricading()
{
	bool bStopThinking = false;
	CBasePlayer *pPlayer = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( m_nBuilder ) );
	if ( !pPlayer )
		bStopThinking = true;
	else
	{
		// Make sure we can't move
		pPlayer->pev->velocity = pPlayer->pev->basevelocity = g_vecZero;

		// Still holding use?
		if ( !( pPlayer->pev->button & IN_USE ) )
			bStopThinking = true;
		// Out of ammo for some reason?
		else if ( pPlayer->AmmoInventory( ZPAmmoTypes::AMMO_BARRICADE ) <= 0 )
			bStopThinking = true;
		else if ( !CanBuildBarricade() )
			bStopThinking = true;
		else if ( !(pPlayer->pev->flags & FL_ONGROUND) || !pPlayer->pev->groundentity )
			bStopThinking = true;

		// If the player isn't looking at this barricade anymore, stop building. (but ignore our Z angle, that must always be ZERO. We really don't care if we are looking up or down.)
		Vector vecPlayerForward;
		UTIL_MakeVectors( pPlayer->pev->angles );
		vecPlayerForward = gpGlobals->v_forward;
		Vector vecPlayerOrigin = pPlayer->pev->origin;
		Vector vecOrigin = pev->origin;
		// Look only at the X and Y axis, ignore Z.
		vecPlayerForward.z = vecOrigin.z = 0;
		Vector vecToBarricade = pev->origin - pPlayer->pev->origin;
		VectorNormalize( vecToBarricade );
		float flDot = DotProduct( vecPlayerForward, vecToBarricade );
		if ( flDot < 0.5f ) // Arbitrary dot product threshold,
			bStopThinking = true;
	}

	// Stop building?
	if ( bStopThinking )
	{
		StopBuilding();
		return;
	}

	// Fully built?
	if ( m_flBuildStartTime - gpGlobals->time <= 0.0f )
	{
		OnBarricadeBuilt( false );
		return;
	}

	pev->nextthink = gpGlobals->time;
}


//=========================================================
//=========================================================

extern int ExtractBbox( void *pmodel, int sequence, float *mins, float *maxs );

int CPropBarricade::ExtractBbox( int sequence, float *mins, float *maxs )
{
	return ::ExtractBbox( GET_MODEL_PTR(ENT(pev)), sequence, mins, maxs );
}

void CPropBarricade::SetSequenceBox()
{
	Vector mins, maxs;
	GetSequenceBox( mins, maxs );
	if ( mins == g_vecZero && maxs == g_vecZero ) return;
	UTIL_SetSize( pev, mins, maxs );
}

void CPropBarricade::GetSequenceBox( Vector &vMin, Vector &vMax )
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
		vMin = rmin;
		vMax = rmax;
	}
	else
		vMin = vMax = g_vecZero;
}
