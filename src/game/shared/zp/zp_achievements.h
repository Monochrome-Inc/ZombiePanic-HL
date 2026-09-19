// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SHARED_ZOMBIEPANIC_ACHIEVEMENTS
#define SHARED_ZOMBIEPANIC_ACHIEVEMENTS
#pragma once

// ===================================
// Achievement categories (Used by VGUI)
enum
{
	CATEGORY_SHOWALL = 0,
	CATEGORY_GENERAL,
	CATEGORY_MAPS,
	CATEGORY_KILLS,

	MAX_CATEGORIES
};

// ===================================
// Stats
enum EStats
{
	INVALID_STAT = -1,
	ZP_KILLS_CROWBAR = 0,
	ZP_KILLS_DBARREL,
	ZP_KILLS_LEADPIPE,
	ZP_KILLS_PLANK,
	ZP_KILLS_SLEDGEHAMMER,
	ZP_KILLS_FIREAXE,
	ZP_KILLS_MACHETE,
	ZP_KILLS_SHOVEL,
	ZP_KILLS_GOLFCLUB,
	ZP_KILLS_GUITAR,
	ZP_KILLS_GUITAR_BASE,
	ZP_KILLS_GUITAR_ELECTRIC,
	ZP_KILLS_WINEBOTTLE,
	ZP_KILLS_CHAIR,
	ZP_KILLS_KEYBOARD,
	ZP_KILLS_BASEBALLBAT,
	ZP_KILLS_BASEBALLBAT_METAL,
	ZP_KILLS_PISTOL,
	ZP_KILLS_PPK,
	ZP_KILLS_PPK_HS,
	ZP_KILLS_REVOLVER,
	ZP_KILLS_RIFLE,
	ZP_KILLS_SHOTGUN,
	ZP_KILLS_SATCHEL,
	ZP_KILLS_TNT,
	ZP_KILLS_MOLOTOV,
	ZP_KILLS_ZOMBIE,
	ZP_FLEEESH,
	ZP_ITS_A_MASSACRE,
	ZP_PANIC_100,
	ZP_PUMPUPSHOTGUN,
	ZP_CHILDOFGRAVE,
	ZP_KILLS_MP5,
	ZP_MMMWAP,
	ZP_ESCAPE_ARTIST,
	ZP_REGEN_10K,
	ZP_ILIVEAGAIN,
	ZP_HC_SNACKTIME,
	ZP_GENOCIDESTEP3,
	ZP_KILLYOSELF,
	ZP_EFFED_FACE,

	// Maps -- Survival
	MAP_ZP_SANTERIA,
	MAP_ZP_CLUBZOMBO,
	MAP_ZP_THELABS,
	MAP_ZP_EASTSIDE,
	MAP_ZP_INDUSTRY,
	MAP_ZP_CONTINGENCY,
	MAP_ZP_HAUNTED,
	MAP_ZP_HOTEL,
	MAP_ZP_RUINS,
	MAP_ZP_TOWN,
	MAP_ZP_MANSION,

	// Maps -- Objective
	MAP_ZPO_CONTINGENCY,
	MAP_ZPO_CLUBZOMBO,
	MAP_ZPO_EASTSIDE,
	MAP_ZPO_HOTEL,

	// Maps -- Hardcore
	MAP_ZPH_THELABS,
	MAP_ZPH_EASTSIDE,
	MAP_ZPH_INDUSTRY,
	MAP_ZPH_HAUNTED,
	MAP_ZPH_SANTERIA,
	MAP_ZPH_CONTINGENCY,
	MAP_ZPH_HOTEL,
	MAP_ZPH_RUINS,
	MAP_ZPH_TOWN,
	MAP_ZPH_MANSION,

	ZP_RESET_ALL, // If this stat is 1, reset all stats.

	STAT_MAX
};

// ===================================
// Achievements
enum EAchievements
{
	ONE_OF_US = 0,
	FIRST_TO_DIE,
	PANIC_ATTACK,
	KILLS_PISTOL,
	KILLS_PPK,
	KILLS_PPK_HEADSHOT,
	KILLS_SHOTGUN,
	KILLS_RIFLE,
	KILLS_REVOLVER,
	KILLS_SATCHEL,
	KILLS_TNT,
	KILLS_MOLOTOV,
	KILLS_CROWBAR,
	KILLS_ZOMBIE,
	YOU_WILL_DIE_WITH_ME,
	UNSAFE_HANDLING,
	I_FELL,
	FIRST_SURVIVAL,
	FIRST_OBJECTIVE,
	JACKOFTRADES,
	CLOCKOUT,
	PANICRUSH,
	FLEEESH,
	ZOMBIEDESSERT,
	SCREAM4ME,
	ITS_A_MASSACRE,
	THE_ATEAM,
	LASTMANSTAND,
	MARATHON,
	PARTNERINCRIME,
	RABBITBEAST,
	PLAY_ALL_SURVIVAL,
	PLAY_ALL_OBJECTIVE,
	PLAY_ALL_HARDCORE,
	PANIC_100,
	INLINEP2,
	PUMPUPSHOTGUN,
	CUTYOUDOWN,
	CHILDOFGRAVE,
	DIE_BY_DOOR,
	ZMASH,
	TABLE_FLIP,
	KILLS_MP5,
	PARTOFHORDE,
	MMMWAP,
	FIRST_ESCAPE,
	ESCAPE_ARTIST,
	REGEN_10K,
	ILIVEAGAIN,
	HC_SNACKTIME,
	HC_OVERWEIGHTKILLER,
	GENOCIDESTEP3,
	DANCE_FLOOR,
	HOUSEOFHORRORS,
	KILLS_LEADPIPE,
	KILLS_DBARREL,
	KILLYOSELF,
	HC_STOP_THE_BLEEDIN,
	INTERFECTUM,
	HC_BLOODHARVEST,
	EFFED_FACE,
	KILLS_FIREAXE,
	LAST_BULLET,
	DBARREL_2KILL,
	HC_HEADSHOTFEST,

	ACHV_MAX
};

// ===================================
// Achievement kill structure (how we give achievements from kills)
struct KillAchievementData_t
{
	EAchievements ID;
	const char *Name;
	bool NeedHeadshot;
	bool NeedMelee;
};
void GetKillAchievementsByWeapon( const char *szWeapon, bool bIsHeadshot, std::vector<KillAchievementData_t> &vOut );

// ===================================
// Stats data structure
struct StatData_t
{
	EStats ID;
	const char *Name;
	int32 Value;
	int32 MaxValue;
};

// ===================================
// Achievement required steps structure
struct RequiredStepsTable
{
	EStats Stat;
	int32 MaxValue; // Overrides the required value
	RequiredStepsTable( EStats nStat )
	{
		Stat = nStat;
		MaxValue = 0;
	}
	RequiredStepsTable( EStats nStat, int32 nMax )
	{
		Stat = nStat;
		MaxValue = nMax;
	}
};


// ===================================
// Achievement data structure
class DialogAchievementData
{
private:
	EAchievements m_eAchievementID;
	char m_pchAchievementID[64];
	bool m_bAchieved;
	int m_iIconImage;
	int m_eCategory;
	EStats m_nStat;
	std::vector<RequiredStepsTable> m_RequiredSteps;

public:
	DialogAchievementData( EAchievements nID, const char *szName, int nCategory, EStats nStatID );
	DialogAchievementData( EAchievements nID, const char *szName, int nCategory, EStats nStatID, std::vector<RequiredStepsTable> nRequiredSteps );

	StatData_t GetData();
	const bool HasStatID() { return (m_nStat > INVALID_STAT) ? true : false; }
	const bool HasRequiredSteps() { return (m_RequiredSteps.size() > 0) ? true : false; }

	size_t GetRequiredStepCount() { return m_RequiredSteps.size(); }
	RequiredStepsTable GetRequiredStepID( int iIdx ) { return m_RequiredSteps[iIdx]; }

	const bool IsAchieved() { return m_bAchieved; }
	void SetAchieved( bool bState ) { m_bAchieved = bState; }

	int GetIconImage() { return m_iIconImage; }
	void SetIconImage( int iImage ) { m_iIconImage = iImage; }

	const EAchievements GetAchievementID() { return m_eAchievementID; }
	const char *GetAchievementName() { return m_pchAchievementID; }
	const int GetCategoryID() { return m_eCategory; }
};

#define _ACH_ADD_ID( id, category, steamstat ) DialogAchievementData( id, #id, category, steamstat )
#define _ACH_ADD_ID_LIST( id, category, steamstat, list ) DialogAchievementData( id, #id, category, steamstat, list )

// Get Achievement by ID
DialogAchievementData GetAchievementByID( int eAchievement );
DialogAchievementData GetAchievementByID( const char *szAchievementID );

void SetAchievementCompletedByID( DialogAchievementData nAch, bool bState );

#endif