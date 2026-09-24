// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "extdll.h"
#include "util.h"
#include "player.h"
#include "maprules.h"
#include "zp/info_random_base.h"
#include "zp/info_beacon.h"
#include "zp/prop_static_spawn.h"
#include "zp/gamemodes/zp_gamemodebase.h"
#include "convar.h"
#ifdef SCRIPT_SYSTEM
#include "core.h"
#endif

// for std::vector random_shuffle
#include <iterator>
#include <random>
#include <algorithm>

ConVar sv_fafo_only( "sv_fafo_only", "0", FCVAR_CHEATS, "Only spawns FAFO-FW from random spawns." );

struct PossibleRandomSpawnLocation
{
	int EntIndex;
	ItemType Type;
};
static std::vector<PossibleRandomSpawnLocation> s_RandomSpawnLocations;
static void SpawnItems( const ItemType &nType )
{
	for ( size_t i = 0; i < s_RandomSpawnLocations.size(); i++ )
	{
		PossibleRandomSpawnLocation SpawnLoc = s_RandomSpawnLocations[i];
		if ( SpawnLoc.Type != nType ) continue;
		edict_t *pEdict = INDEXENT( SpawnLoc.EntIndex );
		if ( pEdict && !pEdict->free )
		{
			CRandomItemBase *pSpawner = (CRandomItemBase *)CBaseEntity::Instance( pEdict );
			if ( pSpawner )
				pSpawner->SpawnItem();
		}
	}
}
static void AddSpawnItem( const ItemType &nType, CBaseEntity *pEntity )
{
	if ( !pEntity ) return;
	PossibleRandomSpawnLocation item;
	item.EntIndex = pEntity->entindex();
	item.Type = nType;
	s_RandomSpawnLocations.push_back( item );
}

struct DebugSpawnList
{
	std::string Classname;
	int Amount;
};
static std::vector<DebugSpawnList> s_SpawnedItems;
static std::vector<SpawnList*> s_SpawnList;
static SpawnList *m_DefaultSpawnItem = new SpawnList();
static int s_iCurrentPlayerAmount = 0;

static void OnItemCreated(const char *szClassname)
{
	for ( size_t i = 0; i < s_SpawnedItems.size(); i++ )
	{
		DebugSpawnList &item = s_SpawnedItems[i];
		if ( FStrEq( item.Classname.c_str(), szClassname ) )
		{
			item.Amount++;
			return;
		}
	}
	DebugSpawnList item;
	item.Amount = 1;
	item.Classname = szClassname;
	s_SpawnedItems.push_back( item );
}
static void SetItemAsFull( const char *szClassname )
{
	for ( size_t i = 0; i < s_SpawnList.size(); i++ )
	{
		SpawnList *item = s_SpawnList[i];
		if ( FStrEq( item->Classname, szClassname ) )
			item->Full = true;
	}
}

struct ReportEntities
{
	string_t classname;
	int amount;
};

void ZP::CheckHowManySpawnedItems( CBasePlayer *pPlayer )
{
	bool bIsCheatsEnabled = CVAR_GET_FLOAT("sv_cheats") >= 1 ? true : false;
	if ( !bIsCheatsEnabled ) return;

	int m_EntAmount = 0;
	std::vector<ReportEntities> m_Classnames;
	for ( size_t i = 1; i < gpGlobals->maxEntities - 1; i++ )
	{
		edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex( i );
		CBaseEntity *pEnt = CBaseEntity::Instance( pEdict );
		if ( !pEnt ) continue;
		string_t classname = pEnt->pev->classname;
		if ( classname == 0 ) continue;
		if ( FStrEq( STRING( classname ), "worldspawn" ) ) continue;
		m_EntAmount++;
		bool bAlreadyExist = false;
		for (size_t i = 0; i < m_Classnames.size(); i++)
		{
			ReportEntities &repEnt = m_Classnames[i];
			if ( FStrEq( STRING( repEnt.classname ), STRING( classname ) ) )
			{
				repEnt.amount++;
				bAlreadyExist = true;
			}
		}
		if ( bAlreadyExist ) continue;

		ReportEntities repEnt;
		repEnt.classname = classname;
		repEnt.amount = 1;
		m_Classnames.push_back( repEnt );
	}

	// Before we print, sort the list alphabetically
	qsort( m_Classnames.data(), m_Classnames.size(), sizeof( ReportEntities ), []( const void *a, const void *b ) -> int
	{
		ReportEntities *entA = (ReportEntities *)a;
		ReportEntities *entB = (ReportEntities *)b;
		return strcmp( STRING( entA->classname ), STRING( entB->classname ) );
	});

	for ( size_t i = 0; i < m_Classnames.size(); i++ )
	{
		ReportEntities repEnt = m_Classnames[i];
		UTIL_PrintConsole( UTIL_VarArgs( "Class: %s (%i)\n", STRING( repEnt.classname ), repEnt.amount ), pPlayer );
	}
	UTIL_PrintConsole( UTIL_VarArgs( "Total %i entities\n", m_EntAmount ), pPlayer );

	//UTIL_PrintConsole( "Items spawned this round:\n", pPlayer );
	//for (DebugSpawnList &i : s_SpawnedItems)
	//	UTIL_PrintConsole( UTIL_VarArgs( "%s [%i]\n", i.Classname.c_str(), i.Amount ), pPlayer );
	UTIL_PrintConsole( "--------------------\n", pPlayer );
}

void ZP::SpawnItems()
{
	// First spawn all weapons
	CRandomItemBase *pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( nullptr, "info_random_weapon" );
	while ( pFind )
	{
		AddSpawnItem( ItemType::TypeWeapon, pFind );
		pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( pFind, "info_random_weapon" );
	}

	// Now go trough our ammo.
	pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( nullptr, "info_random_ammo" );
	while ( pFind )
	{
		AddSpawnItem( ItemType::TypeAmmo, pFind );
		pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( pFind, "info_random_ammo" );
	}

	// Now go trough our items.
	pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( nullptr, "info_random_item" );
	while ( pFind )
	{
		AddSpawnItem( ItemType::TypeItem, pFind );
		pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( pFind, "info_random_item" );
	}

	// Now go trough our melee.
	pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( nullptr, "info_random_melee" );
	while ( pFind )
	{
		AddSpawnItem( ItemType::TypeMelee, pFind );
		pFind = (CRandomItemBase *)UTIL_FindEntityByClassname( pFind, "info_random_melee" );
	}

	std::random_device rd;
	std::mt19937 g( rd() );
	std::shuffle( s_RandomSpawnLocations.begin(), s_RandomSpawnLocations.end(), g );

	SpawnItems( ItemType::TypeWeapon );
	SpawnItems( ItemType::TypeItem );
	SpawnItems( ItemType::TypeAmmo );
	SpawnItems( ItemType::TypeMelee );

	// Reset after use
	for ( size_t i = 0; i < s_SpawnList.size(); i++ )
	{
		SpawnList *item = s_SpawnList[i];
		item->Full = false;
	}
	s_iCurrentPlayerAmount = 0;
}

void AddDefaultAmmoSpawn( const char *szClassname, int iLimit, ItemType nType )
{
	s_SpawnList.push_back( new SpawnList( szClassname, iLimit, nType ) );
}

void ZP::SetupDefaultMeleeSpawnList()
{
	for ( size_t i = 0; i < ZPWeaponID::LAST_WEAPON_ID; i++ )
	{
		GetWeaponInfo( (ZPWeaponID)i, [&]( const WeaponInfo &info )
		{
			// Add melee weapons
			if ( info.Melee )
			{
				char szClassname[32];
				UTIL_strcpy( szClassname, "weapon_" );
				V_strcat_safe( szClassname, info.szWeapon );
				AddDefaultAmmoSpawn( szClassname, 3, ItemType::TypeMelee );
			}
		});
	}
}

void ZP::SetupDefaultSpawnList()
{
	int nPlayers = s_iCurrentPlayerAmount;

	enum DefaultSpawnIndices
	{
		AMMO_9MM = 0,
		AMMO_556,
		AMMO_BUCKSHOT,
		AMMO_357,
		AMMO_22LRBOX,

		WEAPON_SIG,
		WEAPON_GLOCK17,
		WEAPON_CZ75,
		WEAPON_357,
		WEAPON_556AR,
		WEAPON_SKS,
		WEAPON_MP5,
		WEAPON_SHOTGUN,
		WEAPON_PPK,
		WEAPON_DOUBLEBARREL,

		ITEM_HEALTHKIT,
		ITEM_HEALTHKIT_MEDIUM,
		ITEM_HEALTHKIT_SMALL,
		ITEM_PAINKILLER,
		ITEM_BANDAGE,
		ITEM_BATTERY,
		ITEM_SATCHEL,
		ITEM_TNT,
		ITEM_MOLOTOV,

		MAX_DEFAULT_SPAWNS_COUNT
	};

	struct DefaultSpawnInfo
	{
		const char *szClassname;
		int iLimit;
		ItemType nType;
	};

	DefaultSpawnInfo defaultSpawns[MAX_DEFAULT_SPAWNS_COUNT] =
	{
		{ "ammo_9mmbox", 0, ItemType::TypeAmmo },
		{ "ammo_riflebox", 0, ItemType::TypeAmmo },
		{ "ammo_buckshot", 0, ItemType::TypeAmmo },
		{ "ammo_357box", 0, ItemType::TypeAmmo },
		{ "ammo_22lrbox", 0, ItemType::TypeAmmo },

		{ "weapon_sig", 0, ItemType::TypeWeapon },
		{ "weapon_glock17", 0, ItemType::TypeWeapon },
		{ "weapon_cz75", 0, ItemType::TypeWeapon },
		{ "weapon_357", 0, ItemType::TypeWeapon },
		{ "weapon_556ar", 0, ItemType::TypeWeapon },
		{ "weapon_sks", 0, ItemType::TypeWeapon },
		{ "weapon_mp5", 0, ItemType::TypeWeapon },
		{ "weapon_shotgun", 0, ItemType::TypeWeapon },
		{ "weapon_ppk", 0, ItemType::TypeWeapon },
		{ "weapon_doublebarrel", 0, ItemType::TypeWeapon },

		{ "item_healthkit", 0, ItemType::TypeItem },
		{ "item_healthkit_medium", 3, ItemType::TypeItem },
		{ "item_healthkit_small", 3, ItemType::TypeItem },
		{ "item_painkiller", 4, ItemType::TypeItem },
		{ "item_bandage", 4, ItemType::TypeItem },
		{ "item_battery", 0, ItemType::TypeItem },
		{ "weapon_ied", 0, ItemType::TypeItem },
		{ "weapon_tnt", 0, ItemType::TypeItem },
		{ "weapon_molotov", 0, ItemType::TypeItem },
	};

	if ( nPlayers >= 18 )
	{
		defaultSpawns[AMMO_9MM].iLimit = 12;
		defaultSpawns[AMMO_556].iLimit = 4;
		defaultSpawns[AMMO_BUCKSHOT].iLimit = 4;
		defaultSpawns[AMMO_357].iLimit = 4;
		defaultSpawns[AMMO_22LRBOX].iLimit = 11;

		defaultSpawns[WEAPON_SIG].iLimit = 3;
		defaultSpawns[WEAPON_GLOCK17].iLimit = 3;
		defaultSpawns[WEAPON_CZ75].iLimit = 3;
		defaultSpawns[WEAPON_357].iLimit = 3;
		defaultSpawns[WEAPON_556AR].iLimit = 3;
		defaultSpawns[WEAPON_SKS].iLimit = 3;
		defaultSpawns[WEAPON_MP5].iLimit = 3;
		defaultSpawns[WEAPON_SHOTGUN].iLimit = 3;
		defaultSpawns[WEAPON_PPK].iLimit = 3;
		defaultSpawns[WEAPON_DOUBLEBARREL].iLimit = 2;

		defaultSpawns[ITEM_HEALTHKIT].iLimit = 3;
		defaultSpawns[ITEM_HEALTHKIT_MEDIUM].iLimit = 3;
		defaultSpawns[ITEM_HEALTHKIT_SMALL].iLimit = 3;
		defaultSpawns[ITEM_PAINKILLER].iLimit = 6;
		defaultSpawns[ITEM_BANDAGE].iLimit = 6;
		defaultSpawns[ITEM_BATTERY].iLimit = 3;
		defaultSpawns[ITEM_SATCHEL].iLimit = 4;
		defaultSpawns[ITEM_TNT].iLimit = 4;
		defaultSpawns[ITEM_MOLOTOV].iLimit = 4;
	}
	else if ( nPlayers >= 15 )
	{
		defaultSpawns[AMMO_9MM].iLimit = 11;
		defaultSpawns[AMMO_556].iLimit = 3;
		defaultSpawns[AMMO_BUCKSHOT].iLimit = 3;
		defaultSpawns[AMMO_357].iLimit = 3;
		defaultSpawns[AMMO_22LRBOX].iLimit = 10;

		defaultSpawns[WEAPON_SIG].iLimit = 2;
		defaultSpawns[WEAPON_GLOCK17].iLimit = 2;
		defaultSpawns[WEAPON_CZ75].iLimit = 2;
		defaultSpawns[WEAPON_357].iLimit = 2;
		defaultSpawns[WEAPON_556AR].iLimit = 2;
		defaultSpawns[WEAPON_SKS].iLimit = 2;
		defaultSpawns[WEAPON_MP5].iLimit = 2;
		defaultSpawns[WEAPON_SHOTGUN].iLimit = 2;
		defaultSpawns[WEAPON_PPK].iLimit = 3;
		defaultSpawns[WEAPON_DOUBLEBARREL].iLimit = 2;

		defaultSpawns[ITEM_HEALTHKIT].iLimit = 3;
		defaultSpawns[ITEM_HEALTHKIT_MEDIUM].iLimit = 3;
		defaultSpawns[ITEM_HEALTHKIT_SMALL].iLimit = 3;
		defaultSpawns[ITEM_PAINKILLER].iLimit = 5;
		defaultSpawns[ITEM_BANDAGE].iLimit = 5;
		defaultSpawns[ITEM_BATTERY].iLimit = 3;
		defaultSpawns[ITEM_SATCHEL].iLimit = 2;
		defaultSpawns[ITEM_TNT].iLimit = 3;
		defaultSpawns[ITEM_MOLOTOV].iLimit = 3;
	}
	else if ( nPlayers >= 12 )
	{
		defaultSpawns[AMMO_9MM].iLimit = 10;
		defaultSpawns[AMMO_556].iLimit = 2;
		defaultSpawns[AMMO_BUCKSHOT].iLimit = 2;
		defaultSpawns[AMMO_357].iLimit = 3;
		defaultSpawns[AMMO_22LRBOX].iLimit = 9;

		defaultSpawns[WEAPON_SIG].iLimit = 2;
		defaultSpawns[WEAPON_GLOCK17].iLimit = 2;
		defaultSpawns[WEAPON_CZ75].iLimit = 2;
		defaultSpawns[WEAPON_357].iLimit = 2;
		defaultSpawns[WEAPON_556AR].iLimit = 1;
		defaultSpawns[WEAPON_SKS].iLimit = 1;
		defaultSpawns[WEAPON_MP5].iLimit = 2;
		defaultSpawns[WEAPON_SHOTGUN].iLimit = 2;
		defaultSpawns[WEAPON_PPK].iLimit = 3;
		defaultSpawns[WEAPON_DOUBLEBARREL].iLimit = 2;

		defaultSpawns[ITEM_HEALTHKIT].iLimit = 3;
		defaultSpawns[ITEM_HEALTHKIT_MEDIUM].iLimit = 3;
		defaultSpawns[ITEM_HEALTHKIT_SMALL].iLimit = 3;
		defaultSpawns[ITEM_PAINKILLER].iLimit = 5;
		defaultSpawns[ITEM_BATTERY].iLimit = 2;
		defaultSpawns[ITEM_SATCHEL].iLimit = 1;
		defaultSpawns[ITEM_TNT].iLimit = 3;
		defaultSpawns[ITEM_MOLOTOV].iLimit = 3;
	}
	else if ( nPlayers >= 8 )
	{
		defaultSpawns[AMMO_9MM].iLimit = 9;
		defaultSpawns[AMMO_556].iLimit = 2;
		defaultSpawns[AMMO_BUCKSHOT].iLimit = 2;
		defaultSpawns[AMMO_357].iLimit = 2;
		defaultSpawns[AMMO_22LRBOX].iLimit = 6;

		defaultSpawns[WEAPON_SIG].iLimit = 2;
		defaultSpawns[WEAPON_GLOCK17].iLimit = 2;
		defaultSpawns[WEAPON_CZ75].iLimit = 2;
		defaultSpawns[WEAPON_357].iLimit = 2;
		defaultSpawns[WEAPON_556AR].iLimit = 1;
		defaultSpawns[WEAPON_SKS].iLimit = 1;
		defaultSpawns[WEAPON_MP5].iLimit = 1;
		defaultSpawns[WEAPON_SHOTGUN].iLimit = 1;
		defaultSpawns[WEAPON_PPK].iLimit = 3;
		defaultSpawns[WEAPON_DOUBLEBARREL].iLimit = 2;

		defaultSpawns[ITEM_HEALTHKIT].iLimit = 2;
		defaultSpawns[ITEM_HEALTHKIT_MEDIUM].iLimit = 2;
		defaultSpawns[ITEM_HEALTHKIT_SMALL].iLimit = 2;
		defaultSpawns[ITEM_BATTERY].iLimit = 2;
		defaultSpawns[ITEM_SATCHEL].iLimit = 1;
		defaultSpawns[ITEM_TNT].iLimit = 2;
		defaultSpawns[ITEM_MOLOTOV].iLimit = 2;
	}
	else if ( nPlayers >= 5 )
	{
		defaultSpawns[AMMO_9MM].iLimit = 8;
		defaultSpawns[AMMO_556].iLimit = 2;
		defaultSpawns[AMMO_BUCKSHOT].iLimit = 2;
		defaultSpawns[AMMO_357].iLimit = 2;
		defaultSpawns[AMMO_22LRBOX].iLimit = 5;

		defaultSpawns[WEAPON_SIG].iLimit = 2;
		defaultSpawns[WEAPON_GLOCK17].iLimit = 2;
		defaultSpawns[WEAPON_CZ75].iLimit = 2;
		defaultSpawns[WEAPON_357].iLimit = 1;
		defaultSpawns[WEAPON_556AR].iLimit = 1;
		defaultSpawns[WEAPON_SKS].iLimit = 1;
		defaultSpawns[WEAPON_MP5].iLimit = 1;
		defaultSpawns[WEAPON_SHOTGUN].iLimit = 1;
		defaultSpawns[WEAPON_PPK].iLimit = 3;
		defaultSpawns[WEAPON_DOUBLEBARREL].iLimit = 1;

		defaultSpawns[ITEM_HEALTHKIT].iLimit = 2;
		defaultSpawns[ITEM_HEALTHKIT_MEDIUM].iLimit = 2;
		defaultSpawns[ITEM_HEALTHKIT_SMALL].iLimit = 2;
		defaultSpawns[ITEM_BATTERY].iLimit = 1;
		defaultSpawns[ITEM_SATCHEL].iLimit = 1;
		defaultSpawns[ITEM_TNT].iLimit = 2;
		defaultSpawns[ITEM_MOLOTOV].iLimit = 2;
	}
	else if ( nPlayers >= 3 )
	{
		defaultSpawns[AMMO_9MM].iLimit = 7;
		defaultSpawns[AMMO_556].iLimit = 2;
		defaultSpawns[AMMO_BUCKSHOT].iLimit = 1;
		defaultSpawns[AMMO_357].iLimit = 2;
		defaultSpawns[AMMO_22LRBOX].iLimit = 4;

		defaultSpawns[WEAPON_SIG].iLimit = 2;
		defaultSpawns[WEAPON_GLOCK17].iLimit = 2;
		defaultSpawns[WEAPON_CZ75].iLimit = 2;
		defaultSpawns[WEAPON_357].iLimit = 1;
		defaultSpawns[WEAPON_556AR].iLimit = 1;
		defaultSpawns[WEAPON_SKS].iLimit = 1;
		defaultSpawns[WEAPON_MP5].iLimit = 1;
		defaultSpawns[WEAPON_SHOTGUN].iLimit = 1;
		defaultSpawns[WEAPON_PPK].iLimit = 3;
		defaultSpawns[WEAPON_DOUBLEBARREL].iLimit = 1;

		defaultSpawns[ITEM_HEALTHKIT].iLimit = 1;
		defaultSpawns[ITEM_HEALTHKIT_MEDIUM].iLimit = 1;
		defaultSpawns[ITEM_HEALTHKIT_SMALL].iLimit = 1;
		defaultSpawns[ITEM_BATTERY].iLimit = 1;
		defaultSpawns[ITEM_SATCHEL].iLimit = 1;
		defaultSpawns[ITEM_TNT].iLimit = 1;
		defaultSpawns[ITEM_MOLOTOV].iLimit = 1;
	}
	else
	{
		defaultSpawns[AMMO_9MM].iLimit = 6;
		defaultSpawns[AMMO_556].iLimit = 1;
		defaultSpawns[AMMO_BUCKSHOT].iLimit = 1;
		defaultSpawns[AMMO_357].iLimit = 2;
		defaultSpawns[AMMO_22LRBOX].iLimit = 4;

		defaultSpawns[WEAPON_SIG].iLimit = 2;
		defaultSpawns[WEAPON_GLOCK17].iLimit = 2;
		defaultSpawns[WEAPON_CZ75].iLimit = 2;
		defaultSpawns[WEAPON_357].iLimit = 1;
		defaultSpawns[WEAPON_556AR].iLimit = 1;
		defaultSpawns[WEAPON_SKS].iLimit = 1;
		defaultSpawns[WEAPON_MP5].iLimit = 1;
		defaultSpawns[WEAPON_SHOTGUN].iLimit = 1;
		defaultSpawns[WEAPON_PPK].iLimit = 3;
		defaultSpawns[WEAPON_DOUBLEBARREL].iLimit = 1;

		defaultSpawns[ITEM_HEALTHKIT].iLimit = 1;
		defaultSpawns[ITEM_HEALTHKIT_MEDIUM].iLimit = 1;
		defaultSpawns[ITEM_HEALTHKIT_SMALL].iLimit = 1;
		defaultSpawns[ITEM_BATTERY].iLimit = 1;
		defaultSpawns[ITEM_SATCHEL].iLimit = 1;
		defaultSpawns[ITEM_TNT].iLimit = 1;
		defaultSpawns[ITEM_MOLOTOV].iLimit = 1;
	}

	for ( size_t i = 0; i < MAX_DEFAULT_SPAWNS_COUNT; i++ )
	{
		DefaultSpawnInfo spawnInfo = defaultSpawns[i];
		AddDefaultAmmoSpawn( spawnInfo.szClassname, spawnInfo.iLimit, spawnInfo.nType );
	}

	// Our current gamemode
	IGameModeBase *pGameMode = ZP::GetCurrentGameMode();

	// In hardcore, add a few backpacks
	if ( pGameMode )
	{
		switch ( pGameMode->GetGameModeType() )
		{
			case ZP::GameModeType_e::GAMEMODE_HARDCORE:
				AddDefaultAmmoSpawn( "item_backpack", 3, ItemType::TypeItem );
				AddDefaultAmmoSpawn( "weapon_nailgun", 4, ItemType::TypeItem );
			break;
			case ZP::GameModeType_e::GAMEMODE_SURVIVAL:
				AddDefaultAmmoSpawn( "weapon_nailgun", 4, ItemType::TypeItem );
			break;
		}
	}

	SetupDefaultMeleeSpawnList();
}

void CRandomItemBase::SpawnItem(void)
{
	int istr = GetRandomClassname();
	if ( istr == 0 ) return;
	const char *szItemToSpawn = STRING( istr );
	if ( szItemToSpawn && szItemToSpawn[0] )
	{
		edict_t *pent = CREATE_NAMED_ENTITY( istr );
		if ( FNullEnt(pent) ) return;

		VARS(pent)->origin = pev->origin + Vector( 0, 0, 2 );
		VARS(pent)->angles = pev->angles;
		pent->v.spawnflags |= SF_NORESPAWN;
		DispatchSpawn(pent);

		CBaseEntity *pSpawned = (CBaseEntity *)GET_PRIVATE(pent);
		if ( pSpawned )
		{
			pSpawned->SetSpawnedTroughRandomEntity( true );
			OnItemCreated( szItemToSpawn );
		}
	}
}

string_t CRandomItemBase::GetRandomClassname() const
{
	if ( s_SpawnList.size() == 0 ) return 0;
	std::vector<SpawnList*> temp = s_SpawnList;
	int idx = RANDOM_LONG( 0, temp.size() - 1 );
	SpawnList *item = temp[ idx ];

	bool bNotValidItem = IsLimited( item );
	while ( bNotValidItem )
	{
		// All of it was invalid?
		// Return default value then.
		if ( temp.size() == 0 )
		{
			item = m_DefaultSpawnItem;
			break;
		}
		else
		{
			idx = RANDOM_LONG( 0, temp.size() - 1 );
			item = temp[ idx ];
			temp.erase( temp.begin() + idx );
		}
		bNotValidItem = IsLimited( item );
	}
	if ( FStrEq( item->Classname, "" ) ) return 0;

	// How many items did we find?
	// We also start at 1, since *this* item needs to be counted for.
	int iFound = 1;

	// Now we check how many there are on the map
	CBaseEntity *pFind = UTIL_FindEntityByClassname( nullptr, item->Classname );
	while ( pFind )
	{
		iFound++;
		pFind = UTIL_FindEntityByClassname( pFind, item->Classname );
	}

	// Check how many we got, if it's equal or above, make Full true
	if ( iFound >= item->Limit )
		SetItemAsFull( item->Classname );

	return ALLOC_STRING( item->Classname );
}

#ifdef SCRIPT_SYSTEM
void OnScriptCallback( KeyValues *pData, AvailableScripts_t nScriptType )
{
	// TODO: Make AvailableScripts_t::Angelscript call this callback if the function was a success
}

// This is for I/O system
void ZP::IO_CalculatePlayerAmount( KeyValues *pData )
{
	// Clear our spawn list first
	for ( auto p : s_SpawnList )
		delete p;
	s_SpawnList.clear();

	// Loop through our types
	for ( KeyValues *pSub = pData->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey() )
	{
		ItemType nType = ItemType::TypeNone;
		const char *szType = pSub->GetName();
		if ( FStrEq( szType, "Ammo" ) )
			nType = ItemType::TypeAmmo;
		else if ( FStrEq( szType, "Items" ) )
			nType = ItemType::TypeItem;
		else if ( FStrEq( szType, "Weapons" ) )
			nType = ItemType::TypeWeapon;
		else if ( FStrEq( szType, "Melee" ) )
			nType = ItemType::TypeMelee;
		const char *szClassname = pSub->GetString( "Classname" );
		int nAmount = pSub->GetInt( "Amount", 0 );
		if ( szClassname && szClassname[0] && nAmount > 0 && nType != ItemType::TypeNone )
			s_SpawnList.push_back( new SpawnList( szClassname, nAmount, nType ) );
	}

	// Let's spawn our items now.
	SpawnItems();
}
#endif

static void CheckCurrentPlayers()
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsConnected() )
			s_iCurrentPlayerAmount++;
	}

#ifdef SCRIPT_SYSTEM
	// For I/O, we need to register CalculatePlayerAmount as a function.
	ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, EmptyIOCallback, "CalculatePlayerAmount" );

	// Call our script system, and make sure we are calling Angelscript
	// and setup our item stuff.
	const std::string &szMapname( STRING( gpGlobals->mapname ) );
	ScriptSystem::CallScript(
		AvailableScripts_t::Angelscript,
		OnScriptCallback,
		"CalculatePlayerAmount",
		2,
		std::to_string( s_iCurrentPlayerAmount ),
		szMapname
	);

	// Let's also call our I/O, if we use that instead.
	const std::string &szArg0( "Function" );
	ScriptCallBackEnum ret = ScriptSystem::CallScript(
		AvailableScripts_t::InputOutput,
		nullptr,
		"CalculatePlayerAmount",
		2,
		szArg0,
		std::to_string( s_iCurrentPlayerAmount )
	);

	// Function was not found or failed to call?
	// Setup default spawn list then.
	if ( ret != ScriptCallBackEnum::ScriptCall_OK )
	{
		ZP::SetupDefaultSpawnList();
		ZP::SpawnItems();
	}
#else
	ZP::SetupDefaultSpawnList();
	ZP::SpawnItems();
#endif
}

bool CRandomItemBase::IsLimited( SpawnList *item ) const
{
	// Not the correct type, return true.
	if ( item->Type != GetType() ) return true;
	// if zero, we cannot spawn this.
	if ( item->Limit == 0 ) return true;
	// Hit the limit?
	if ( item->Full ) return true;
	return false;
}

extern void UTIL_EnableTriggerCapturePoints();

void ZP::OnGameModeRoundStart()
{
	// Clear it
	s_RandomSpawnLocations.clear();
	s_SpawnedItems.clear();
	for ( auto p : s_SpawnList )
		delete p;
	s_SpawnList.clear();

	// Check all players first
	CheckCurrentPlayers();

	// Now, let's start our beacons (if Start On spawnflags is set)
	CInfoBeacon *pBeacon = (CInfoBeacon *)UTIL_FindEntityByClassname( nullptr, "info_beacon" );
	while ( pBeacon )
	{
		// Make sure we only start those with SP_START_ENABLED flag set
		if ( pBeacon->pev->spawnflags & SP_START_ENABLED )
			pBeacon->UpdateMessageState();
		pBeacon = (CInfoBeacon *)UTIL_FindEntityByClassname( pBeacon, "info_beacon" );
	}

	// Check our game_timer, and see if any has the spawnflag SF_GAMETIMER_START_ON_ROUND
	CBaseEntity *pTimer = UTIL_FindEntityByClassname( nullptr, "game_timer" );
	while ( pTimer )
	{
		// Start the timer if we find t his flag
		if ( ( pTimer->pev->spawnflags & SF_GAMETIMER_START_ON_ROUND ) )
			pTimer->Use( pTimer, pTimer, USE_SET, 0 );
		pTimer = UTIL_FindEntityByClassname( pTimer, "game_timer" );
	}

	// prop_static_spawn are similar to info_random ents, but we spawn a specific item w/ a % chance added ontop.
	CPropStaticSpawn *pStaticSpawn = (CPropStaticSpawn *)UTIL_FindEntityByClassname( nullptr, "prop_static_spawn" );
	while ( pStaticSpawn )
	{
		pStaticSpawn->SpawnItem();
		pStaticSpawn = (CPropStaticSpawn *)UTIL_FindEntityByClassname( pStaticSpawn, "prop_static_spawn" );
	}

	// Enable trigger capture points (if they start enabled)
	UTIL_EnableTriggerCapturePoints();
}
