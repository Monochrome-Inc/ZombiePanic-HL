// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "triggers.h"
#include "player.h"
#include "zp/zp_spawnpoint_ent.h"

class CTriggerSpawnBlocker : public CBaseTrigger
{
	SET_BASECLASS( CBaseTrigger );

public:
	void Spawn(void);
	void OnSpawnBlockTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( trigger_spawnblocker, CTriggerSpawnBlocker );

void CTriggerSpawnBlocker::Spawn(void)
{
	InitTrigger();
	SetTouch( &CTriggerSpawnBlocker::OnSpawnBlockTouch );
}

void CTriggerSpawnBlocker::OnSpawnBlockTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() ) return;
	if ( !pOther->IsAlive() ) return;
	if ( pOther->pev->team != ZP::TEAM_SURVIVIOR ) return;
	CBasePlayerSpawnPoint *pFind = (CBasePlayerSpawnPoint *)UTIL_FindEntityByTargetname( nullptr, STRING( pev->message ) );
	while ( pFind )
	{
		pFind->DisableSpawn();
		pFind = (CBasePlayerSpawnPoint *)UTIL_FindEntityByTargetname( pFind, STRING( pev->message ) );
	}
}

// Reverse blocker, created for zp_mansion and zpo_keretti

class CTriggerSpawnEnabler : public CBaseTrigger
{
	SET_BASECLASS( CBaseTrigger );

public:
	void Spawn(void);
	void OnSpawnEnableTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( trigger_spawnenabler, CTriggerSpawnEnabler );

void CTriggerSpawnEnabler::Spawn(void)
{
	InitTrigger();
	SetTouch( &CTriggerSpawnEnabler::OnSpawnEnableTouch );
}

void CTriggerSpawnEnabler::OnSpawnEnableTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() ) return;
	if ( !pOther->IsAlive() ) return;
	if ( pOther->pev->team != ZP::TEAM_SURVIVIOR ) return;
	CBasePlayerSpawnPoint *pFind = (CBasePlayerSpawnPoint *)UTIL_FindEntityByTargetname( nullptr, STRING( pev->message ) );
	while ( pFind )
	{
		pFind->TemporarilyEnableSpawn( 1.0f );
		pFind = (CBasePlayerSpawnPoint *)UTIL_FindEntityByTargetname( pFind, STRING( pev->message ) );
	}
}