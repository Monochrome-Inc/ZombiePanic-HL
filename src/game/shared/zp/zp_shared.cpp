// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#if defined( CLIENT_DLL )
#include "hud.h"
#include "strtools.h"
#include "steam_achievements.h"
#include "console.h"
#else
#include "decals.h"
#include "func_break.h"
#endif

#include <tier2/tier2.h>
#include "FileSystem.h"
#include <KeyValues.h>

#include "weapons.h"
#include "player.h"

#include "zp_shared_weapons.h"
#include "zp_shared.h"

// =========================================================
// Shared convars
// =========================================================

#if !defined( CLIENT_DLL )
ConVar sv_testmode( "sv_testmode", "0", FCVAR_EXTDLL );
#else
ConVar zp_debug( "zp_debug", "0", FCVAR_EXTDLL );
#endif

// =========================================================
// Bot Game Event Types
// =========================================================

char *BotGameEventName[ZP::BOT_EVENT_MAX] = {
	"None",
	"WeaponFired",
	"WeaponUnloaded",
	"WeaponReloaded",
	"WeaponDropped",
	"Explosion",
	"ThrowableThrown",
	"ThrowableBounced",
	"EntityBroken",
	"DoorOpened",
	"PlayerKilled",
	"PlayerJoined",
	"PlayerLeft",
	"PlayerEscaped",
	"PlayerFootstep",
	"PlayerLandedFromHeight",
	"ObjectiveCompleted",
	"ObjectiveFailed",
	"CanEscape",
	"RoundStarted",
	"RoundEnded"
};

// =========================================================
// Achievements Data Class
// =========================================================

std::vector<RequiredStepsTable> m_MarathonSteps = {
	RequiredStepsTable( MAP_ZP_SANTERIA, 1 ),
	RequiredStepsTable( MAP_ZP_CLUBZOMBO, 1 ),
	RequiredStepsTable( MAP_ZP_THELABS, 1 ),
	RequiredStepsTable( MAP_ZP_EASTSIDE, 1 ),
	RequiredStepsTable( MAP_ZP_INDUSTRY, 1 ),
	RequiredStepsTable( MAP_ZP_CONTINGENCY, 1 ),
	RequiredStepsTable( MAP_ZP_HAUNTED, 1 ),
	RequiredStepsTable( MAP_ZP_RUINS, 1 ),
	RequiredStepsTable( MAP_ZP_HOTEL, 1 ),
	//RequiredStepsTable( MAP_ZP_TOWN, 1 ),
	//RequiredStepsTable( MAP_ZP_MANSION, 1 ),
	RequiredStepsTable( MAP_ZPO_CONTINGENCY, 1 ),
	RequiredStepsTable( MAP_ZPO_CLUBZOMBO, 1 ),
	RequiredStepsTable( MAP_ZPO_EASTSIDE, 1 ),
	RequiredStepsTable( MAP_ZPO_HOTEL, 1 ),
	RequiredStepsTable( MAP_ZPH_EASTSIDE, 1 ),
	RequiredStepsTable( MAP_ZPH_INDUSTRY, 1 ),
	RequiredStepsTable( MAP_ZPH_HAUNTED, 1 ),
	RequiredStepsTable( MAP_ZPH_SANTERIA, 1 ),
	RequiredStepsTable( MAP_ZPH_THELABS, 1 ),
	RequiredStepsTable( MAP_ZPH_CONTINGENCY, 1 ),
	RequiredStepsTable( MAP_ZPH_HOTEL, 1 ),
	RequiredStepsTable( MAP_ZPH_RUINS, 1 )
};

std::vector<RequiredStepsTable> m_MapsSurvivalSteps = {
	RequiredStepsTable( MAP_ZP_SANTERIA, 1 ),
	RequiredStepsTable( MAP_ZP_CLUBZOMBO, 1 ),
	RequiredStepsTable( MAP_ZP_THELABS, 1 ),
	RequiredStepsTable( MAP_ZP_EASTSIDE, 1 ),
	RequiredStepsTable( MAP_ZP_INDUSTRY, 1 ),
	RequiredStepsTable( MAP_ZP_CONTINGENCY, 1 ),
	RequiredStepsTable( MAP_ZP_HAUNTED, 1 ),
	RequiredStepsTable( MAP_ZP_RUINS, 1 ),
	RequiredStepsTable( MAP_ZP_HOTEL, 1 ),
	RequiredStepsTable( MAP_ZP_TOWN, 1 ),
	RequiredStepsTable( MAP_ZP_MANSION, 1 )
};

std::vector<RequiredStepsTable> m_MapsHardcoreSteps = {
	RequiredStepsTable( MAP_ZPH_EASTSIDE, 1 ),
	RequiredStepsTable( MAP_ZPH_INDUSTRY, 1 ),
	RequiredStepsTable( MAP_ZPH_HAUNTED, 1 ),
	RequiredStepsTable( MAP_ZPH_SANTERIA, 1 ),
	RequiredStepsTable( MAP_ZPH_THELABS, 1 ),
	RequiredStepsTable( MAP_ZPH_CONTINGENCY, 1 ),
	RequiredStepsTable( MAP_ZPH_HOTEL, 1 ),
	RequiredStepsTable( MAP_ZPH_RUINS, 1 ),
	RequiredStepsTable( MAP_ZPH_TOWN, 1 ),
	RequiredStepsTable( MAP_ZPH_MANSION, 1 )
};

std::vector<RequiredStepsTable> m_MapsObjectiveSteps = {
	RequiredStepsTable( MAP_ZPO_CONTINGENCY, 1 ),
	RequiredStepsTable( MAP_ZPO_CLUBZOMBO, 1 ),
	RequiredStepsTable( MAP_ZPO_HOTEL, 1 ),
	RequiredStepsTable( MAP_ZPO_EASTSIDE, 1 )
};

// We only need to check for one single kill for each
std::vector<RequiredStepsTable> m_JackOfTradesSteps = {
	RequiredStepsTable( ZP_KILLS_CROWBAR, 1 ),
	RequiredStepsTable( ZP_KILLS_LEADPIPE, 1 ),
//	RequiredStepsTable( ZP_KILLS_PLANK, 1 ),
//	RequiredStepsTable( ZP_KILLS_SLEDGEHAMMER, 1 ),
	RequiredStepsTable( ZP_KILLS_FIREAXE, 1 ),
//	RequiredStepsTable( ZP_KILLS_MACHETE, 1 ),
//	RequiredStepsTable( ZP_KILLS_SHOVEL, 1 ),
//	RequiredStepsTable( ZP_KILLS_GOLFCLUB, 1 ),
//	RequiredStepsTable( ZP_KILLS_GUITAR, 1 ),
//	RequiredStepsTable( ZP_KILLS_GUITAR_BASE, 1 ),
//	RequiredStepsTable( ZP_KILLS_GUITAR_ELECTRIC, 1 ),
//	RequiredStepsTable( ZP_KILLS_WINEBOTTLE, 1 ),
//	RequiredStepsTable( ZP_KILLS_CHAIR, 1 ),
//	RequiredStepsTable( ZP_KILLS_KEYBOARD, 1 ),
//	RequiredStepsTable( ZP_KILLS_BASEBALLBAT, 1 ),
//	RequiredStepsTable( ZP_KILLS_BASEBALLBAT_METAL, 1 ),
	RequiredStepsTable( ZP_KILLS_PISTOL, 1 ),
	RequiredStepsTable( ZP_KILLS_PPK, 1 ),
	RequiredStepsTable( ZP_KILLS_REVOLVER, 1 ),
	RequiredStepsTable( ZP_KILLS_RIFLE, 1 ),
	RequiredStepsTable( ZP_KILLS_MP5, 1 ),
	RequiredStepsTable( ZP_KILLS_SHOTGUN, 1 ),
	RequiredStepsTable( ZP_KILLS_DBARREL, 1 ),
	RequiredStepsTable( ZP_KILLS_SATCHEL, 1 ),
	RequiredStepsTable( ZP_KILLS_TNT, 1 ),
	RequiredStepsTable( ZP_KILLS_MOLOTOV, 1 )
};

static DialogAchievementData g_DAchievements[] =
{
	_ACH_ADD_ID(KILLS_CROWBAR,					CATEGORY_KILLS,			ZP_KILLS_CROWBAR),
	_ACH_ADD_ID(KILLS_LEADPIPE,					CATEGORY_KILLS,			ZP_KILLS_LEADPIPE),
	_ACH_ADD_ID(KILLS_FIREAXE,					CATEGORY_KILLS,			ZP_KILLS_FIREAXE),
	_ACH_ADD_ID(KILLS_PISTOL,					CATEGORY_KILLS,			ZP_KILLS_PISTOL),
	_ACH_ADD_ID(KILLS_PPK,						CATEGORY_KILLS,			ZP_KILLS_PPK),
	_ACH_ADD_ID(KILLS_PPK_HEADSHOT,				CATEGORY_KILLS,			ZP_KILLS_PPK_HS),
	_ACH_ADD_ID(KILLS_REVOLVER,					CATEGORY_KILLS,			ZP_KILLS_REVOLVER),
	_ACH_ADD_ID(KILLS_RIFLE,					CATEGORY_KILLS,			ZP_KILLS_RIFLE),
	_ACH_ADD_ID(KILLS_MP5,						CATEGORY_KILLS,			ZP_KILLS_MP5),
	_ACH_ADD_ID(KILLS_SHOTGUN,					CATEGORY_KILLS,			ZP_KILLS_SHOTGUN),
	_ACH_ADD_ID(KILLS_DBARREL,					CATEGORY_KILLS,			ZP_KILLS_DBARREL),
	_ACH_ADD_ID(KILLS_SATCHEL,					CATEGORY_KILLS,			ZP_KILLS_SATCHEL),
	_ACH_ADD_ID(KILLS_TNT,						CATEGORY_KILLS,			ZP_KILLS_TNT),
	_ACH_ADD_ID(KILLS_MOLOTOV,					CATEGORY_KILLS,			ZP_KILLS_MOLOTOV),
	_ACH_ADD_ID(KILLS_ZOMBIE,					CATEGORY_KILLS,			ZP_KILLS_ZOMBIE),
	_ACH_ADD_ID(LAST_BULLET,					CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(DBARREL_2KILL,					CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(HC_HEADSHOTFEST,				CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(YOU_WILL_DIE_WITH_ME,			CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(UNSAFE_HANDLING,				CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID_LIST(JACKOFTRADES,				CATEGORY_KILLS,			INVALID_STAT, m_JackOfTradesSteps),
	_ACH_ADD_ID(PANICRUSH,						CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(FLEEESH,						CATEGORY_KILLS,			ZP_FLEEESH),
	_ACH_ADD_ID(ZOMBIEDESSERT,					CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(SCREAM4ME,						CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(INLINEP2,						CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(CUTYOUDOWN,						CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(RABBITBEAST,					CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(ITS_A_MASSACRE,					CATEGORY_KILLS,			ZP_ITS_A_MASSACRE),
	_ACH_ADD_ID(HC_SNACKTIME,					CATEGORY_KILLS,			ZP_HC_SNACKTIME),
	_ACH_ADD_ID(HC_OVERWEIGHTKILLER,			CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(GENOCIDESTEP3,					CATEGORY_KILLS,			ZP_GENOCIDESTEP3),
	_ACH_ADD_ID(DANCE_FLOOR,					CATEGORY_KILLS,			INVALID_STAT),
	_ACH_ADD_ID(EFFED_FACE,						CATEGORY_KILLS,			ZP_EFFED_FACE),

	_ACH_ADD_ID(FIRST_SURVIVAL,					CATEGORY_MAPS,			INVALID_STAT),
	_ACH_ADD_ID(FIRST_OBJECTIVE,				CATEGORY_MAPS,			INVALID_STAT),
	_ACH_ADD_ID(PARTOFHORDE,					CATEGORY_MAPS,			INVALID_STAT),
	_ACH_ADD_ID(CLOCKOUT,						CATEGORY_MAPS,			INVALID_STAT),
	_ACH_ADD_ID(THE_ATEAM,						CATEGORY_MAPS,			INVALID_STAT),
	_ACH_ADD_ID(PARTNERINCRIME,					CATEGORY_MAPS,			INVALID_STAT),
	_ACH_ADD_ID(LASTMANSTAND,					CATEGORY_MAPS,			INVALID_STAT),

	_ACH_ADD_ID_LIST(MARATHON,					CATEGORY_MAPS,			INVALID_STAT, m_MarathonSteps),
	_ACH_ADD_ID_LIST(PLAY_ALL_SURVIVAL,			CATEGORY_MAPS,			INVALID_STAT, m_MapsSurvivalSteps),
	_ACH_ADD_ID_LIST(PLAY_ALL_OBJECTIVE,		CATEGORY_MAPS,			INVALID_STAT, m_MapsObjectiveSteps),
	_ACH_ADD_ID_LIST(PLAY_ALL_HARDCORE,			CATEGORY_MAPS,			INVALID_STAT, m_MapsHardcoreSteps),

	_ACH_ADD_ID(FIRST_ESCAPE,					CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(ESCAPE_ARTIST,					CATEGORY_GENERAL,		ZP_ESCAPE_ARTIST),
	_ACH_ADD_ID(PANIC_ATTACK,					CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(PANIC_100,						CATEGORY_GENERAL,		ZP_PANIC_100),
	_ACH_ADD_ID(MMMWAP,							CATEGORY_GENERAL,		ZP_MMMWAP),
	_ACH_ADD_ID(I_FELL,							CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(HC_STOP_THE_BLEEDIN,			CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(HC_BLOODHARVEST,				CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(INTERFECTUM,					CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(ONE_OF_US,						CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(FIRST_TO_DIE,					CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(TABLE_FLIP,						CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(ZMASH,							CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(DIE_BY_DOOR,					CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(PUMPUPSHOTGUN,					CATEGORY_GENERAL,		ZP_PUMPUPSHOTGUN),
	_ACH_ADD_ID(CHILDOFGRAVE,					CATEGORY_GENERAL,		ZP_CHILDOFGRAVE),
	_ACH_ADD_ID(REGEN_10K,						CATEGORY_GENERAL,		ZP_REGEN_10K),
	_ACH_ADD_ID(ILIVEAGAIN,						CATEGORY_GENERAL,		ZP_ILIVEAGAIN),
	_ACH_ADD_ID(HOUSEOFHORRORS,					CATEGORY_GENERAL,		INVALID_STAT),
	_ACH_ADD_ID(KILLYOSELF,						CATEGORY_GENERAL,		ZP_KILLYOSELF),
};

#define _ACH_KILL_ID( id, name, headshot, melee ) { id, name, headshot, melee }
#define _ACH_KILL_ID_MELEE( id, name ) _ACH_KILL_ID( id, name, false, true )
#define _ACH_KILL_ID_HEADSHOT( id, name ) _ACH_KILL_ID( id, name, true, false )
#define _ACH_KILL_ID_DEFAULT( id, name ) _ACH_KILL_ID( id, name, false, false )
KillAchievementData_t g_KillAchievements[] = {
	// { EAchievements ID, const char *Name, bool NeedHeadshot, bool NeedMelee }

	// Melee kills
	_ACH_KILL_ID_MELEE( KILLS_CROWBAR, "crowbar" ),
	_ACH_KILL_ID_MELEE( KILLS_LEADPIPE, "leadpipe" ),
	_ACH_KILL_ID_MELEE( KILLS_FIREAXE, "fireaxe" ),

	// Default kills
	_ACH_KILL_ID_DEFAULT( KILLS_PISTOL, "sig" ),
	_ACH_KILL_ID_DEFAULT( KILLS_PPK, "ppk" ),
	_ACH_KILL_ID_DEFAULT( KILLS_REVOLVER, "357" ),
	_ACH_KILL_ID_DEFAULT( KILLS_RIFLE, "556ar" ),
	_ACH_KILL_ID_DEFAULT( KILLS_MP5, "mp5" ),
	_ACH_KILL_ID_DEFAULT( KILLS_DBARREL, "doublebarrel" ),
	_ACH_KILL_ID_DEFAULT( DBARREL_2KILL, "doublebarrel" ),
	_ACH_KILL_ID_DEFAULT( KILLS_SHOTGUN, "shotgun" ),
	_ACH_KILL_ID_DEFAULT( CUTYOUDOWN, "shotgun" ),
	_ACH_KILL_ID_DEFAULT( KILLS_SATCHEL, "satchel" ),
	_ACH_KILL_ID_DEFAULT( KILLS_TNT, "grenade" ),
	_ACH_KILL_ID_DEFAULT( KILLS_MOLOTOV, "flame_large" ),
	_ACH_KILL_ID_DEFAULT( KILLS_MOLOTOV, "flame_medium" ),
	_ACH_KILL_ID_DEFAULT( KILLS_ZOMBIE, "swipe" ),
	_ACH_KILL_ID_DEFAULT( FLEEESH, "swipe" ),
	_ACH_KILL_ID_DEFAULT( RABBITBEAST, "swipe" ),
	_ACH_KILL_ID_DEFAULT( HC_SNACKTIME, "swipe" ),

	// Headshot kills
	_ACH_KILL_ID_HEADSHOT( KILLS_PPK_HEADSHOT, "ppk" ),
	_ACH_KILL_ID_HEADSHOT( INLINEP2, "357" ),
	_ACH_KILL_ID_HEADSHOT( EFFED_FACE, "swipe" ),
};

// =========================================================
// Weapon Script
// =========================================================

AmmoData::AmmoData( ZPAmmoTypes nType, const char *szName, int iAmmoBox, int iMaxCarry, float flWeight, bool bDrawClip )
{
	AmmoType = nType;
	UTIL_strcpy( AmmoName, szName );
	AmmoBoxGive = iAmmoBox;
	MaxCarry = iMaxCarry;
	WeightPerBullet = flWeight;
	DrawClip = bDrawClip;
}

#define WEAPON_SCRIPT_PATH "scripts/"
#define WEAPON_SCRIPT_FILE ".txt"
static std::vector<WeaponData> sWeaponDataList;
static AmmoData sAmmoDataList[] = {
	AmmoData( AMMO_PISTOL, "9mm", 15, 150, 0.21f, false ),
	AmmoData( AMMO_MAGNUM, "357", 6, 30, 0.65f, false ),
	AmmoData( AMMO_SHOTGUN, "buckshot", 6, 24, 1.35f, false ),
	AmmoData( AMMO_RIFLE, "556ar", 20, 150, 0.35f, false ),
	AmmoData( AMMO_LONGRIFLE, "longrifle", 10, 100, 0.48f, false ),

	AmmoData( AMMO_GRENADE, "explosive_tnt", 0, 0, 0.1f, true ),
	AmmoData( AMMO_SATCHEL, "explosive_ied", 0, 0, 0.5f, false ),
	AmmoData( AMMO_NAILS, "misc_nails", 0, 0, 0.1f, true ),

	AmmoData( AMMO_BARRICADE, "barricade", 1, 1, 0.0f, false ),

	// MUST BE LAST, DO NOT CHANGE THIS.
	// This is used by Crowbar and Swipe (or any other weapon that has no ammo)
	AmmoData( AMMO_NONE, "", 0, -1, 0.0f, false )
};

static WeaponInfo sWeaponInfoList[] = {
	// { const char *szWeapon, ZPWeaponID WeaponID, bool Hidden, bool Melee }
	{ "crowbar", WEAPON_CROWBAR, false, true },
	{ "walkstick", WEAPON_WALKSTICK, false, true },
	{ "leadpipe", WEAPON_LEADPIPE, false, true },
	{ "fireaxe", WEAPON_FIREAXE, false, true },
	{ "swipe", WEAPON_SWIPE, true, false },
	{ "sig", WEAPON_SIG, false, false },
	{ "ppk", WEAPON_PPK, false, false },
	{ "cz75", WEAPON_CZ75, false, false },
	{ "357", WEAPON_PYTHON, false, false },
	{ "mp5", WEAPON_MP5, false, false },
	{ "556ar", WEAPON_556AR, false, false },
	{ "sks", WEAPON_SKS, false, false },
	{ "shotgun", WEAPON_SHOTGUN, false, false },
	{ "doublebarrel", WEAPON_DOUBLEBARREL, false, false },
	{ "tnt", WEAPON_TNT, false, false },
	{ "molotov", WEAPON_MOLOTOV, false, false },
	{ "nailgun", WEAPON_NAILGUN, false, false },
	{ "ied", WEAPON_SATCHEL, false, false },
	{ "glock17", WEAPON_GLOCK17, false, false },
	{ "fafo", WEAPON_FAFO_ERW, false, false },
};


WeaponData CreateWeaponSlotData( const char *szClassname )
{
	if ( !szClassname ) return WeaponData();

	std::string szClassnameStr( szClassname );
	// Make sure our classname is starting with "weapon_"
	// if not, add it.
	if ( szClassnameStr.find( "weapon_" ) != 0 )
		szClassnameStr = "weapon_" + szClassnameStr;

	std::string szFile( WEAPON_SCRIPT_PATH + szClassnameStr + WEAPON_SCRIPT_FILE );
	KeyValues *pWeaponScript = new KeyValues( "WeaponInfo" );
	if ( !pWeaponScript->LoadFromFile( g_pFullFileSystem, szFile.c_str() ) )
	{
#if defined( CLIENT_DLL )
		if ( zp_debug.GetBool() )
			ConPrintf( Color( 255, 51, 51, 255 ), "[WeaponInfo] Failed to load weapon script \"%s\"\n", szFile.c_str() );
#endif
		pWeaponScript->deleteThis();
		return WeaponData();
	}

	WeaponData slot;
	slot.WeaponID = GetWeaponInfo( szClassnameStr.c_str() ).WeaponID;
	UTIL_strcpy( slot.Classname, szClassnameStr.c_str() );
	slot.Ammo1[0] = 0;
	slot.Ammo2[0] = 0;

	// Default
	slot.FireRate[0] = 0.1f;
	slot.FireRate[1] = 0.1f;
	slot.WeaponSpread[0] = 0.1f;
	slot.WeaponSpread[1] = 0.1f;
	for ( int i = 0; i < WeaponDataIcons::MAX_ICONS; i++ )
		slot.Icons[i][0] = 0;

	slot.Slot = pWeaponScript->GetInt( "Slot", 0 );
	slot.Position = pWeaponScript->GetInt( "Position", 0 );

	const char *szAmmoType = pWeaponScript->GetString( "AmmoType1", NULL );
	if ( szAmmoType && szAmmoType[0] )
	{
		AmmoData ammo = GetAmmoByName( szAmmoType );
		if ( ammo.MaxCarry != -1 )
		{
			UTIL_strcpy( slot.Ammo1, szAmmoType );
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_AMMO1], szAmmoType );
		}
	}

	szAmmoType = pWeaponScript->GetString( "AmmoType2", NULL );
	if ( szAmmoType && szAmmoType[0] )
	{
		AmmoData ammo = GetAmmoByName( szAmmoType );
		if ( ammo.MaxCarry != -1 )
		{
			UTIL_strcpy( slot.Ammo2, szAmmoType );
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_AMMO2], szAmmoType );
		}
	}

	slot.DefaultAmmo = pWeaponScript->GetInt( "DefaultGive", 7 );
	slot.AmmoBox = pWeaponScript->GetInt( "AmmoBox", 7 );
	slot.MaxClip = pWeaponScript->GetInt( "MaxClip", 7 );
	slot.Bullets = pWeaponScript->GetInt( "Bullets", 1 );
	slot.Flags = 0;

	// Go trough our flags
	KeyValues *pWeaponFlags = pWeaponScript->FindKey( "Flags" );
	if ( pWeaponFlags )
	{
		if ( pWeaponFlags->GetBool( "SELECTONEMPTY" ) ) slot.Flags |= ITEM_FLAG_SELECTONEMPTY;
		if ( pWeaponFlags->GetBool( "NOAUTORELOAD" ) ) slot.Flags |= ITEM_FLAG_NOAUTORELOAD;
		//if ( pWeaponFlags->GetBool( "NOAUTOSWITCHEMPTY" ) ) slot.Flags |= ITEM_FLAG_NOAUTOSWITCHEMPTY; // Unused
		if ( pWeaponFlags->GetBool( "LIMITINWORLD" ) ) slot.Flags |= ITEM_FLAG_LIMITINWORLD;
		if ( pWeaponFlags->GetBool( "EXHAUSTIBLE" ) ) slot.Flags |= ITEM_FLAG_EXHAUSTIBLE;
		if ( pWeaponFlags->GetBool( "NOAUTOSWITCHTO" ) ) slot.Flags |= ITEM_FLAG_NOAUTOSWITCHTO;
		if ( pWeaponFlags->GetBool( "ALLOWDUPLICATE" ) ) slot.Flags |= ITEM_FLAG_ALLOWDUPLICATE;
	}

	slot.Weight = pWeaponScript->GetFloat( "Weight", 0 );
	slot.DoubleSlots = pWeaponScript->GetBool( "DoubleSlots", false );

	// Primary Attack stuff
	KeyValues *pPrimaryAttack = pWeaponScript->FindKey( "Primary" );
	if ( pPrimaryAttack )
	{
		slot.FireRate[0] = pPrimaryAttack->GetFloat( "FireRate", 0.1f );
		slot.WeaponSpread[0] = pPrimaryAttack->GetFloat( "Spread", 0.1f );
	}

	// Secondary Attack stuff
	KeyValues *pSecondaryAttack = pWeaponScript->FindKey( "Secondary" );
	if ( pSecondaryAttack )
	{
		slot.FireRate[1] = pSecondaryAttack->GetFloat( "FireRate", 0.1f );
		slot.WeaponSpread[1] = pSecondaryAttack->GetFloat( "Spread", 0.1f );
	}

	KeyValues *pWeaponSprites = pWeaponScript->FindKey( "HUD" );
	if ( pWeaponSprites )
	{
		const char *szIcon = pWeaponSprites->GetString( "Weapon", NULL );
		if ( szIcon && szIcon[0] )
		{
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_WEAPON], szIcon );
			char buffer[32];
			Q_snprintf( buffer, sizeof( buffer ), "%s_s", szIcon );
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_WEAPON_SELECTED], buffer );
		}
		szIcon = pWeaponSprites->GetString( "Crosshair", NULL );
		if ( szIcon && szIcon[0] )
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_CROSSHAIR], szIcon );
		szIcon = pWeaponSprites->GetString( "CrosshairAuto", NULL );
		if ( szIcon && szIcon[0] )
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_CROSSHAIR_AUTO], szIcon );
		szIcon = pWeaponSprites->GetString( "CrosshairZoom", NULL );
		if ( szIcon && szIcon[0] )
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_CROSSHAIR_ZOOM], szIcon );
		szIcon = pWeaponSprites->GetString( "CrosshairZoomAuto", NULL );
		if ( szIcon && szIcon[0] )
			UTIL_strcpy( slot.Icons[WeaponDataIcons::ICON_CROSSHAIR_ZOOMAUTO], szIcon );
	}

	pWeaponScript->deleteThis();
	
#if defined( CLIENT_DLL )
	if ( zp_debug.GetBool() )
		ConPrintf( Color( 153, 255, 51, 255 ), "[WeaponInfo] Loaded weapon script \"%s\" [%i]\n", szFile.c_str(), slot.WeaponID );
#endif

	sWeaponDataList.push_back( slot );
	return slot;
}

WeaponData CreateWeaponSlotData( ZPWeaponID WeaponID )
{
	char szWeaponScriptFile[64];
	WeaponInfo info = GetWeaponInfo( WeaponID );
	strcpy( szWeaponScriptFile, "weapon_" );
	if ( info.szWeapon )
		strcat( szWeaponScriptFile, info.szWeapon );
	else
		strcat( szWeaponScriptFile, "example" );
	
#if defined( CLIENT_DLL )
	if ( zp_debug.GetBool() )
		Msg( "[WeaponInfo] Loading weapon script \"%s\" [%i]\n", szWeaponScriptFile, WeaponID );
#endif

	return CreateWeaponSlotData( szWeaponScriptFile );
}

AmmoData GetAmmoByName( const char *szAmmoType )
{
	if ( szAmmoType && szAmmoType[0] )
	{
		for ( int i = 0; i < ARRAYSIZE( sAmmoDataList ); i++ )
		{
			AmmoData ammo = sAmmoDataList[i];
			if ( !ammo.AmmoName ) continue;
			if ( FStrEq( ammo.AmmoName, szAmmoType ) )
				return ammo;
		}
	}
	// If we find nothing, grab the last item in the list.
	return sAmmoDataList[ ARRAYSIZE( sAmmoDataList ) - 1 ];
}

AmmoData GetAmmoByTableIndex(int id)
{
	int iFind = (int)clamp( id, 0, ARRAYSIZE( sAmmoDataList ) - 1 );
	return sAmmoDataList[ iFind ];
}

AmmoData GetAmmoByAmmoID( int id )
{
	for ( int i = 0; i < ARRAYSIZE( sAmmoDataList ); i++ )
	{
		AmmoData ammo = sAmmoDataList[i];
		if ( ammo.AmmoType == id )
			return ammo;
	}
	// If we find nothing, grab the last item in the list.
	return sAmmoDataList[ ARRAYSIZE( sAmmoDataList ) - 1 ];
}

AmmoData GetAmmoByWeaponID(ZPWeaponID id, bool bPrimary)
{
	WeaponData slot = GetWeaponSlotInfo( id );
	return GetAmmoByName( bPrimary ? slot.Ammo1 : slot.Ammo2 );
}

WeaponInfo GetWeaponInfo( const char *szClassname )
{
	if ( szClassname && szClassname[0] )
	{
		// Check if we have "weapon_", if we do, get rid of it.
		if ( StringHasPrefix( szClassname, "weapon_" ) )
			szClassname += 7;

		for ( int i = 0; i < ARRAYSIZE( sWeaponInfoList ); i++ )
		{
			WeaponInfo weapon = sWeaponInfoList[i];
			if ( FStrEq( weapon.szWeapon, szClassname ) )
				return weapon;
		}
	}
	return WeaponInfo();
}

WeaponInfo GetWeaponInfo( ZPWeaponID id )
{
	for ( int i = 0; i < ARRAYSIZE( sWeaponInfoList ); i++ )
	{
		WeaponInfo weapon = sWeaponInfoList[i];
		if ( weapon.WeaponID == id )
			return weapon;
	}
	return WeaponInfo();
}

WeaponInfo GetRandomWeaponInfo()
{
	WeaponInfo weapon = WeaponInfo();
	do
	{
		weapon = sWeaponInfoList[ RandomInt( 0, ARRAYSIZE( sWeaponInfoList ) - 1 ) ];
	}
	while ( !weapon.Hidden );
	// Grab a random item
	return weapon;
}

WeaponInfo GetRandomMeleeWeaponInfo()
{
	WeaponInfo weapon = WeaponInfo();
	do
	{
		weapon = sWeaponInfoList[ RandomInt( 0, ARRAYSIZE( sWeaponInfoList ) - 1 ) ];
	}
	while ( !weapon.Hidden && weapon.Melee );
	// Grab a random item
	return weapon;
}

WeaponData GetWeaponSlotInfo( ZPWeaponID WeaponID )
{
	for ( int i = 0; i < sWeaponDataList.size(); i++ )
	{
		WeaponData slot = sWeaponDataList[i];
		if ( slot.WeaponID == WeaponID )
			return slot;
	}
	return CreateWeaponSlotData( WeaponID );
}

WeaponData GetWeaponSlotInfo( const char *szClassname )
{
	return GetWeaponSlotInfo( GetWeaponInfo( szClassname ).WeaponID );
}

#ifdef SERVER_DLL
struct StaticSpawn
{
	char Classname[32];
	int SpawnFlags;
	Vector Origin;
	Vector Angle;
};
static std::vector<StaticSpawn> sStaticSpawnList;
void ZP::ClearStaticSpawnList() { sStaticSpawnList.clear(); }

void ZP::AddToStaticSpawnList( string_t classname, int spawnflags, float flOrigin[3], float flAngle[3] )
{
	StaticSpawn item;
	UTIL_strcpy( item.Classname, STRING( classname ) );
	item.SpawnFlags = spawnflags;
	item.Origin.x = flOrigin[0];
	item.Origin.y = flOrigin[1];
	item.Origin.z = flOrigin[2];
	item.Angle.x = flAngle[0];
	item.Angle.y = flAngle[1];
	item.Angle.z = flAngle[2];

	sStaticSpawnList.push_back( item );
}

void ZP::SpawnStaticSpawns()
{
	for ( int i = 0; i < sStaticSpawnList.size(); i++ )
	{
		StaticSpawn slot = sStaticSpawnList[i];
		CBaseEntity::Create( (char *)slot.Classname, slot.Origin + Vector( 0, 0, 2 ), slot.Angle, nullptr );
	}
}

int ZP::GrabCorrectDecal( int iDamageFlag )
{
	// We don't use them as bits here, since only a single flag is sent here.
	switch ( iDamageFlag )
	{
		case DMG_CLUB: return DECAL_CROWBAR1 + RANDOM_LONG(0, 4);
		case DMG_SLASH: return DECAL_ZOMBIE_SWIPE1 + RANDOM_LONG(0, 4);
		case DMG_SONIC: return DECAL_MELEE_SHARP1 + RANDOM_LONG(0, 4);
	}
	// Default, if we found nothing.
	return DECAL_GUNSHOT1 + RANDOM_LONG(0, 4);
}

void ZP::CheckIfBreakableGlass( TraceResult *pTrace, CBaseEntity *pEnt, const Vector &vDir, int iDamageFlag )
{
	// Check if this is a breakable window. If so, add a decal on the other side.
	CBreakable *pBreakable = dynamic_cast<CBreakable*>( pEnt );
	if ( pBreakable && pBreakable->m_Material == matGlass )
	{
		// Point backwards
		TraceResult tr;
		Vector vecSrc = pTrace->vecEndPos + vDir * 10;
		Vector vecEnd = pTrace->vecEndPos;
		UTIL_TraceLine( vecSrc, vecEnd, ignore_monsters, nullptr, &tr );
		if ( tr.flFraction <= 1.0 )
		{
			CBreakable *pBackHit = dynamic_cast<CBreakable*>( CBaseEntity::Instance( tr.pHit ) );
			if ( pBackHit && pBackHit->m_Material == matGlass )
				UTIL_DecalTrace( &tr, iDamageFlag );
		}
	}
}

struct BulletPenetrationMaterial
{
	Materials Material;
	float Reduction;
};

struct BulletPenetration
{
	Bullet Type = Bullet::BULLET_NONE;
	float Reduction = 0.0f;
	int Amount = 0;
	float Distance = 10;
	std::vector<BulletPenetrationMaterial> MaterialList = {};

	// Get our damage reduction if we hit a specific material
	float GetMaterialPenetration( Materials nMat )
	{
		for ( size_t i = 0; i < MaterialList.size(); i++ )
		{
			BulletPenetrationMaterial MaterialPenetrationInfo = MaterialList[ i ];
			if ( MaterialPenetrationInfo.Material == nMat )
				return MaterialPenetrationInfo.Reduction;
		}
		return 1.0f;
	}
};

std::vector<BulletPenetrationMaterial> m_BulletPenetrationMat_WeakPistols = {
	{ matUnbreakableGlass, 0.0 },
	{ matGlass, 0.9 },
	{ matCeilingTile, 0.9 },
	{ matCinderBlock, 0.1 },
	{ matComputer, 0.3 },
	{ matFlesh, 0.8 },
	{ matMetal, 0.0 },
	{ matWood, 0.85 },
	{ matRocks, 0.0 }
};

std::vector<BulletPenetrationMaterial> m_BulletPenetrationMat_Revolver = {
	{ matUnbreakableGlass, 0.0 },
	{ matGlass, 0.95 },
	{ matCeilingTile, 0.95 },
	{ matCinderBlock, 0.2 },
	{ matComputer, 0.7 },
	{ matFlesh, 1.0 },
	{ matMetal, 0.25 },
	{ matWood, 0.95 },
	{ matRocks, 0.1 }
};

std::vector<BulletPenetrationMaterial> m_BulletPenetrationMat_SMG = {
	{ matUnbreakableGlass, 0.0 },
	{ matGlass, 0.8 },
	{ matCeilingTile, 0.7 },
	{ matCinderBlock, 0.1 },
	{ matComputer, 0.55 },
	{ matFlesh, 0.78 },
	{ matMetal, 0.0 },
	{ matWood, 0.65 },
	{ matRocks, 0.0 }

};

std::vector<BulletPenetrationMaterial> m_BulletPenetrationMat_Rifles = {
	{ matUnbreakableGlass, 0.0 },
	{ matGlass, 0.9 },
	{ matCeilingTile, 0.9 },
	{ matCinderBlock, 0.4 },
	{ matComputer, 0.6 },
	{ matFlesh, 1.0 },
	{ matMetal, 0.3 },
	{ matWood, 0.98 },
	{ matRocks, 0.2 }

};

std::vector<BulletPenetrationMaterial> m_BulletPenetrationMat_FAFO = {
	{ matUnbreakableGlass, 0.0 }
};

std::vector<BulletPenetrationMaterial> m_BulletPenetrationMat_Buckshot = {
	{ matUnbreakableGlass, 0.0 },
	{ matGlass, 0.98 },
	{ matCeilingTile, 0.6 },
	{ matCinderBlock, 0.18 },
	{ matComputer, 0.5 },
	{ matFlesh, 0.98 },
	{ matMetal, 0.15 },
	{ matWood, 0.85 },
	{ matRocks, 0.12 }
};

BulletPenetration m_BulletPenetrationList[] = {
	{ BULLET_PLAYER_SIG, 0.5, 1, 300, m_BulletPenetrationMat_WeakPistols },
	{ BULLET_PLAYER_PPK, 0.4, 1, 300, m_BulletPenetrationMat_WeakPistols },
	{ BULLET_PLAYER_CZ75, 0.4, 1, 300, m_BulletPenetrationMat_WeakPistols },
	{ BULLET_PLAYER_GLOCK, 0.8, 1, 300, m_BulletPenetrationMat_WeakPistols },
	{ BULLET_PLAYER_MP5, 0.65, 1, 700, m_BulletPenetrationMat_SMG },
	{ BULLET_PLAYER_357, 0.8, 2, 500, m_BulletPenetrationMat_Revolver },
	{ BULLET_PLAYER_M16, 0.8, 3, 800, m_BulletPenetrationMat_Rifles },
	{ BULLET_PLAYER_SKS, 0.8, 4, 800, m_BulletPenetrationMat_Rifles },
	{ BULLET_PLAYER_BUCKSHOT, 0.7, 4, 200, m_BulletPenetrationMat_Buckshot },
	{ BULLET_PLAYER_DBARREL, 0.7, 4, 120, m_BulletPenetrationMat_Buckshot },
	{ BULLET_PLAYER_FAFO, 0.65, 10, 800, m_BulletPenetrationMat_FAFO }
};

BulletPenetration GetBulletPenetrationData( Bullet iBulletType )
{
	for ( int i = 0; i < ARRAYSIZE( m_BulletPenetrationList ); i++ )
	{
		BulletPenetration Bullet = m_BulletPenetrationList[ i ];
		if ( iBulletType == Bullet.Type )
			return Bullet;
	}
	return BulletPenetration();
}

void ZP::DoBulletPenetration( entvars_t *pevAttacker, float flDamage, TraceResult *pTrace, CBaseEntity *pEnt, const Vector &vSrc, const Vector &vDir, int iPenetration, int iBulletType )
{
	// Entity was null somehow? Ignore.
	if ( !pEnt ) return;

	// What material did we hit?
	Materials matHit = matNone;

	// If breakable, grab it's break material
	CBreakable *pBreakable = dynamic_cast<CBreakable*>( pEnt );
	if ( pBreakable )
		matHit = pBreakable->m_Material;

	// If player, always return flesh
	if ( pEnt->IsPlayer() )
		matHit = matFlesh;

	// Grab our bullet penetration information
	BulletPenetration nBullet = GetBulletPenetrationData( (Bullet)iBulletType );

	// Not a valid bullet type.
	if ( nBullet.Type == BULLET_NONE ) return;

	// Did we hit the max value? if so, stop.
	if ( iPenetration >= nBullet.Amount ) return;

	// If we are material none, then we manually check the texture.
	if ( matHit == matNone )
	{
		char szbuffer[64];
		Vector vMatEnd = pTrace->vecEndPos + vDir * 3;

		// Find the texture, if we hit the world.
		const char *pTextureName = TRACE_TEXTURE( ENT( pEnt->pev ), vSrc, vMatEnd );
		if ( pTextureName )
		{
			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
				pTextureName += 2;

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
				pTextureName++;
			// '}}'
			strcpy(szbuffer, pTextureName);
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;

			// get texture type
			switch ( TEXTURETYPE_Find( szbuffer ) )
			{
				default:
				case CHAR_TEX_CONCRETE: matHit = matNone; break;
				case CHAR_TEX_GRATE:
			    case CHAR_TEX_VENT:
			    case CHAR_TEX_COMPUTER:
				case CHAR_TEX_METAL: matHit = matMetal; break;
				case CHAR_TEX_GLASS: matHit = matGlass; break;
				case CHAR_TEX_GLASS_UNBREAK: matHit = matUnbreakableGlass; break;
				case CHAR_TEX_FLESH: matHit = matFlesh; break;
				case CHAR_TEX_TILE: matHit = matCeilingTile; break;
				case CHAR_TEX_CARDBOARD:
				case CHAR_TEX_WOOD: matHit = matWood; break;
			}
		}
	}

	// Reduce the amount from the material we hit.
	flDamage *= nBullet.GetMaterialPenetration( matHit );

	// If 0 or less, ignore.
	if ( flDamage <= 0 ) return;

	TraceResult tr;
	Vector vecSrc = pTrace->vecEndPos + vDir * 5;
	Vector vecEnd = pTrace->vecEndPos + vDir * nBullet.Distance;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEnt->edict(), &tr );
	if ( tr.flFraction <= 1.0 )
	{
		// Our entity we found
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

		// Did we hit ourselves? Skip.
		if ( pHit->pev == pevAttacker )
		{
			ZP::DoBulletPenetration( pevAttacker, flDamage, &tr, pHit, vecSrc, vDir, iPenetration, iBulletType );
			return;
		}

		// Make sure we set the correct damage type
		int iDmgType = DMG_BULLET;
		if ( iBulletType == BULLET_PLAYER_FAFO )
			iDmgType = DMG_BULLET | DMG_ALWAYSGIB;

		// We penetrated, reduce it some more.
		flDamage *= nBullet.Reduction;

		// Hurt this entity.
		pHit->TraceAttack( pevAttacker, flDamage, vDir, &tr, iDmgType );

		iPenetration++;
		ZP::DoBulletPenetration( pevAttacker, flDamage, &tr, pHit, vecSrc, vDir, iPenetration, iBulletType );
		DecalGunshot( &tr, vDir, iBulletType );
	}

	UTIL_BubbleTrail( vecSrc, tr.vecEndPos, (nBullet.Distance * tr.flFraction) / 64.0 );
}
#endif

ZP::GameModeType_e ZP::IsValidGameModeMap(const char *szLevel)
{
	if ( !szLevel ) return GameModeType_e::GAMEMODE_INVALID;
	char sz[4];
#if defined( CLIENT_DLL )
	char buf[64];
	V_FileBase( szLevel, buf, sizeof(buf) );
	V_strcpy_safe( sz, buf );
#else
	UTIL_strcpy( sz, szLevel );
#endif
	if ( !stricmp( sz, "zp_" ) ) return GameModeType_e::GAMEMODE_SURVIVAL;
	else if ( !stricmp( sz, "zpo" ) ) return GameModeType_e::GAMEMODE_OBJECTIVE;
	else if ( !stricmp( sz, "zph" ) ) return GameModeType_e::GAMEMODE_HARDCORE;
	return GameModeType_e::GAMEMODE_NONE;
}

float CBasePlayer::GetAmmoWeight( const char *szAmmo )
{
	// We only do this for survivors!!!
	if ( pev->team != ZP::TEAM_SURVIVIOR ) return 0.0f;
	AmmoData ammo = GetAmmoByName( szAmmo );
	int amount = m_rgAmmo[ ammo.AmmoType ];
	return amount * ammo.WeightPerBullet;
}

float CBasePlayer::GetTotalWeight()
{
	int i;
	float flWeight = 0.0f;
	// Count our ammo
	flWeight += GetAmmoWeight( "9mm" );
	flWeight += GetAmmoWeight( "357" );
	flWeight += GetAmmoWeight( "buckshot" );
	flWeight += GetAmmoWeight( "556ar" );
	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)m_rgpPlayerItems[i];
			while (pWeapon)
			{
				flWeight += pWeapon->iWeight();
				pWeapon = (CBasePlayerWeapon *)pWeapon->m_pNext;
			}
		}
	}
#if defined( CLIENT_DLL )
	if ( (gHUD.m_iWeaponBits & (1 << WEAPON_BACKPACK)) )
#else
	if ( HasBackpack() )
#endif
		flWeight += 10.0f; // Backpack adds 10 pounds of weight
	return flWeight;
}

void CBasePlayer::UpdatePlayerMaxSpeed()
{
	int iHowFatAmI = GetTotalWeight();

#if !defined( CLIENT_DLL )
	// If we are in panic, ignore our weight
	if ( IsInPanic() )
		iHowFatAmI = 0;
#endif

	// Now check the weapons we got.
	bool bUseZombieSpeed = ( pev->team == ZP::TEAM_ZOMBIE );
	float flNewSpeed = ZP::MaxSpeeds[bUseZombieSpeed ? 1 : 0] - iHowFatAmI - pev->fuser4;
	if ( flNewSpeed < 50 )
		flNewSpeed = 50;

#if !defined( CLIENT_DLL )
	// If we are in panic, ignore our weight
	if ( IsInPanic() )
		flNewSpeed += ZP::ExtraPanicSpeed;
#endif

	pev->maxspeed = flNewSpeed;
}

// =========================================================
// Achievements Kill Data Struct
// =========================================================

KillAchievementData_t GetKillAchievementByID( int eAchievement )
{
	for ( int i = 0; i < ARRAYSIZE( g_KillAchievements ); i++ )
	{
		KillAchievementData_t item = g_KillAchievements[ i ];
		if ( item.ID == eAchievement )
			return item;
	}
	return g_KillAchievements[0];
}

void GetKillAchievementsByWeapon( const char *szWeapon, bool bIsHeadshot, std::vector<KillAchievementData_t> &vOut )
{
	for ( int i = 0; i < ARRAYSIZE( g_KillAchievements ); i++ )
	{
		KillAchievementData_t item = g_KillAchievements[ i ];
		if ( item.NeedHeadshot && !bIsHeadshot ) continue;
		if ( FStrEq( item.Name, szWeapon ) )
			vOut.push_back( item );
	}
}

// =========================================================
// Achievements Data Class
// =========================================================

DialogAchievementData GetAchievementByID( int eAchievement )
{
	for ( int i = 0; i < ARRAYSIZE( g_DAchievements ); i++ )
	{
		DialogAchievementData item = g_DAchievements[ i ];
		if ( item.GetAchievementID() == eAchievement )
			return item;
	}
	return g_DAchievements[0];
}

DialogAchievementData GetAchievementByID( const char *szAchievementID )
{
	if ( !szAchievementID || !szAchievementID[0] ) return g_DAchievements[0];
	for ( int i = 0; i < ARRAYSIZE( g_DAchievements ); i++ )
	{
		DialogAchievementData item = g_DAchievements[ i ];
		if ( FStrEq( szAchievementID, item.GetAchievementName() ) )
			return item;
	}
	return g_DAchievements[0];
}

void SetAchievementCompletedByID( DialogAchievementData nAch, bool bState )
{
	for ( int i = 0; i < ARRAYSIZE( g_DAchievements ); i++ )
	{
		DialogAchievementData &item = g_DAchievements[ i ];
		if ( item.GetAchievementID() == nAch.GetAchievementID() )
		{
			item.SetAchieved( bState );
			break;
		}
	}
}

DialogAchievementData::DialogAchievementData( EAchievements nID, const char *szName, int nCategory, EStats nStatID )
{
	Q_snprintf( m_pchAchievementID, sizeof(m_pchAchievementID), "%s", szName );
	m_bAchieved = false;
	m_iIconImage = 0;
	m_eCategory = nCategory;
	m_eAchievementID = nID;
	m_nStat = nStatID;
}

DialogAchievementData::DialogAchievementData( EAchievements nID, const char *szName, int nCategory, EStats nStatID, std::vector<RequiredStepsTable> nRequiredSteps )
{
	Q_snprintf( m_pchAchievementID, sizeof(m_pchAchievementID), "%s", szName );
	m_bAchieved = false;
	m_iIconImage = 0;
	m_eCategory = nCategory;
	m_eAchievementID = nID;
	m_nStat = nStatID;
	m_RequiredSteps = nRequiredSteps;
}

StatData_t DialogAchievementData::GetData()
{
#if defined( CLIENT_DLL )
	return GrabStat( m_nStat );
#else
	return StatData_t();
#endif
}

// ==========================================================================
// Entity Physics
// ==========================================================================

bool ZP::Physics::Simulate( CBaseEntity *pEnt )
{
	if ( !pEnt ) return false;
	if ( pEnt->pev->flags & FL_ONGROUND )
	{
		// We are on the ground, fire a trace to set our new angles.
		ZP::Physics::ResetAngles( pEnt );
		return false;
	}
	pEnt->pev->avelocity = pEnt->pev->avelocity + RandomVector( -200, 200 );
	return true;
}

void ZP::Physics::OnHit( CBaseEntity *pEnt, CBaseEntity *pHit )
{
	if ( !pEnt ) return;
	if ( pEnt->pev->flags & FL_ONGROUND )
	{
		// add a bit of static friction
		pEnt->pev->velocity = pEnt->pev->velocity * 0.8;
		Vector vAng = pEnt->pev->avelocity * 0.9;
		vAng.x = vAng.z = 0;
		pEnt->pev->avelocity = vAng;
	}
	else
	{
		// play bounce sound
		pEnt->BounceSound();
		ZP::Physics::ResetAngles( pEnt );
	}
}

void ZP::Physics::ResetAngles( CBaseEntity *pEnt )
{
	if ( !pEnt ) return;
	UTIL_MakeVectors( pEnt->pev->angles );
	Vector vecSrc = pEnt->Center();
	Vector vecAiming = gpGlobals->v_forward; //vecSrc + Vector( 0, 0, -32 );
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 32, ignore_monsters, ENT(pEnt->pev), &tr );
	if ( tr.flFraction < 1.0f )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if ( pEntity && !(pEntity->pev->flags & FL_CONVEYOR) )
		{
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );
			angles.x = angles.z = 0;
			pEnt->pev->avelocity.x = pEnt->pev->avelocity.z = 0;
#if defined( CLIENT_DLL )
			pEnt->pev->angles = angles;
#else
			pEnt->SetAngles( angles );
#endif
		}
	}
	else
	{
		Vector vAng = pEnt->pev->angles;
		vAng.x = vAng.z = 0;
#if defined( CLIENT_DLL )
		pEnt->pev->angles = vAng;
#else
		pEnt->SetAngles( vAng );
#endif
	}
}
