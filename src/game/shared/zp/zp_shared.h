// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SHARED_ZOMBIEPANIC
#define SHARED_ZOMBIEPANIC
#pragma once

#include "Color.h"
#include <vector>
#include <functional>

// Auto include em.
#include "zp_achievements.h"

// For convars
#include <convar.h>

#if !defined( CLIENT_DLL )
extern ConVar sv_testmode;
#endif

#define ZP_GAMEDESC_VERSION "Zombie Panic! v1.2"

class CBaseEntity;

/// <summary>
/// Objective states, used by info_objective.cpp & zp_objective.cpp
/// </summary>
enum ObjectiveState
{
	State_Normal = 0,
	State_InProgress,
	State_Completed,
	State_Failed
};

/// <summary>
/// Player vocalize types
/// </summary>
typedef enum
{
	// Invalid line
	VOCALIZE_NONE = 0,

	// Manual
	VOCALIZE_AGREE,
	VOCALIZE_DECLINE,
	VOCALIZE_COVER,
	VOCALIZE_NEED_AMMO,
	VOCALIZE_NEED_WEAPON,
	VOCALIZE_HOLD_HERE,
	VOCALIZE_OPEN_FIRE,
	VOCALIZE_TAUNT,
	VOCALIZE_PANIC,

	// Automatic
	VOCALIZE_AUTO_ONSTART,
	VOCALIZE_AUTO_KILL,
	VOCALIZE_AUTO_CAMP,
	VOCALIZE_AUTO_HURT,
	VOCALIZE_AUTO_PAIN,
	VOCALIZE_AUTO_PAIN_DROWN,
	VOCALIZE_AUTO_DEATH,
	VOCALIZE_AUTO_DEATH_FALL,

	VOCALIZE_MAX
} PlayerVocalizeType;

/// <summary>
/// The WeaponID's used by Zombie Panic!
/// </summary>
enum ZPWeaponID
{
	WEAPON_NONE = 0,

	// Weapons from the prototype
	WEAPON_CROWBAR,
	WEAPON_WALKSTICK,
	WEAPON_SWIPE,
	WEAPON_SIG,
	WEAPON_PYTHON,
	WEAPON_MP5,		// Was cut from original
	WEAPON_556AR,
	WEAPON_SKS,
	WEAPON_SHOTGUN,
	WEAPON_TNT,
	WEAPON_SATCHEL,

	// New weapons
	WEAPON_LEADPIPE,
	WEAPON_MACHETE,
	WEAPON_FIREAXE,
	WEAPON_DOUBLEBARREL,
	WEAPON_PPK,
	WEAPON_CZ75,
	WEAPON_GLOCK17,
	WEAPON_FAFO_ERW,
	WEAPON_MOLOTOV,
	WEAPON_NAILGUN,

	// Misc stuff, should be last
	WEAPON_BACKPACK,	// Not really a weapon, but used to give extra slots in Hardcore mode,
						// Does nothing in other gamemodes.
	WEAPON_SUIT,		// HUD related

	LAST_WEAPON_ID
};

/// <summary>
/// The available ammo types, used by Zombie Panic!.
/// This is also used by the new ammo table, since
/// the GoldSrc version sucked ass.
/// </summary>
enum ZPAmmoTypes
{
	AMMO_NONE = 0,
	AMMO_PISTOL,
	AMMO_MAGNUM,
	AMMO_SHOTGUN,
	AMMO_RIFLE,
	AMMO_LONGRIFLE,
	AMMO_BARRICADE, // Used for barricade
	AMMO_GRENADE, // Everything after this, we don't allow for dropping ammo
	AMMO_SATCHEL,
	AMMO_NAILS,

	AMMO_MAX
};

/// <summary>
/// The required data for our ammo. That's about it.
/// </summary>
struct AmmoData
{
	ZPAmmoTypes AmmoType;
	char AmmoName[32];
	int AmmoBoxGive;
	int MaxCarry;
	float WeightPerBullet;
	bool DrawClip; // Set this to true, if MaxCarry is 0
	AmmoData( ZPAmmoTypes nType, const char *szName, int iAmmoBox, int iMaxCarry, float flWeight, bool bDrawClip );
};
AmmoData GetAmmoByName( const char *szAmmoType );
AmmoData GetAmmoByTableIndex( int id );
AmmoData GetAmmoByAmmoID( int id );
AmmoData GetAmmoByWeaponID( ZPWeaponID id, bool bPrimary = true );

/// <summary>
/// The static weapon list
/// Used by CreateWeaponSlotData and 'give' command.
/// </summary>
struct WeaponInfo
{
	const char *szWeapon = nullptr;
	ZPWeaponID WeaponID = WEAPON_NONE;
	bool Hidden = false;
	bool Melee = false;
};
WeaponInfo GetWeaponInfo( const char *szClassname );
WeaponInfo GetWeaponInfo( ZPWeaponID id );
WeaponInfo GetRandomWeaponInfo();
WeaponInfo GetRandomMeleeWeaponInfo();

/// <summary>
// Overloaded function to get weapon info with a callback
// Usage: GetWeaponInfo((ZPWeaponID)i, [&](const WeaponInfo &info){ ... });
// The callback will be called with the WeaponInfo for the given WeaponID if it is valid.
/// </summary>
inline void GetWeaponInfo( ZPWeaponID id, const std::function<void(const WeaponInfo &)> &callback )
{
	WeaponInfo info = GetWeaponInfo( id );
	if ( info.WeaponID != WEAPON_NONE && info.szWeapon != nullptr )
		callback( info );
}

enum WeaponDataIcons
{
	ICON_WEAPON = 0,
	ICON_WEAPON_SELECTED,
	ICON_AMMO1,
	ICON_AMMO2,
	ICON_CROSSHAIR,
	ICON_CROSSHAIR_AUTO,
	ICON_CROSSHAIR_ZOOM,
	ICON_CROSSHAIR_ZOOMAUTO,
	MAX_ICONS,
};

struct WeaponData
{
	ZPWeaponID WeaponID;
	char Classname[32]; // weapon classname, used when dropping the weapon,
						// because pev->classname wants to break at times.
	int Slot;
	int Position;
	char Ammo1[16]; // ammo 1 type
	char Ammo2[16]; // ammo 2 type
	char Icons[WeaponDataIcons::MAX_ICONS][32]; // HUD Icons
	int DefaultAmmo;
	int AmmoBox; // How much should the ammo item give?
	int MaxClip;
	int Bullets = 1;
	int Flags;
	float Weight;
	bool DoubleSlots = false; // Does this weapon take 2 slots?
	float WeaponSpread[2]; // 0 - Primary, 1 - Secondary
	float FireRate[2]; // 0 - Primary, 1 - Secondary
};

/// <summary>
/// Grabs the weapon information by using the WeaponID.
/// </summary>
/// <param name="WeaponID">The WeaponID we want to look for</param>
/// <returns>Returns the data from our weapon script file</returns>
WeaponData GetWeaponSlotInfo( ZPWeaponID WeaponID );
WeaponData GetWeaponSlotInfo( const char *szClassname );

// Player Death flags
#define PLR_DEATH_FLAG_NONE				0					// No flag found
#define PLR_DEATH_FLAG_HEADSHOT			(1 << 0)			// Player now have a permanent headache
#define PLR_DEATH_FLAG_GIBBED			(1 << 1)			// Player got gibbed. Mmmm, free food
#define PLR_DEATH_FLAG_FELL				(1 << 2)			// Player fell to their death. Good job
#define PLR_DEATH_FLAG_BEYOND_GRAVE		(1 << 3)			// Player killed someone while already dead
#define PLR_DEATH_FLAG_TEAMKILLER		(1 << 4)			// Player was a friend :(


// Beacon Types (used by info_beacon entity)
enum BeaconTypes
{
	BEACON_BUTTON = 0,
	BEACON_DEFEND,
	BEACON_DESTROY,
	BEACON_WAYPOINT,
	BEACON_CAPTUREPOINT,
};

// Beacon Draw Types (used by info_beacon entity)
enum BeaconDrawTypes
{
	BEACON_DRAW_ALWAYS = 0, // Always draw, no matter what
	BEACON_DRAW_UNCOLLUDED, // Only draw when uncolluded
	BEACON_DRAW_COLLUDED, // Only draw when colluded
};

// Our gait state, so we set the movment speed correctly for the clients.
enum PlayerGaitState
{
	GAIT_STATE_IDLE = 0,
	GAIT_STATE_WALK,
	GAIT_STATE_RUN,
	GAIT_STATE_PANIC,
};

enum ChatFilters
{
	CHAT_FILTER_NONE		= 0,
	CHAT_FILTER_JOINLEAVE	= (1<<0),
	CHAT_FILTER_NAMECHANGE	= (1<<1),
	CHAT_FILTER_PUBLICCHAT	= (1<<2),
	CHAT_FILTER_SERVERMSG	= (1<<3),
    CHAT_FILTER_ACHIEVEMENT	= (1<<4),
};

namespace ZP
{
#ifdef SERVER_DLL
    void ClearStaticSpawnList();
	void AddToStaticSpawnList( string_t classname, int spawnflags, float flOrigin[3], float flAngle[3] );
	void SpawnStaticSpawns();
    int GrabCorrectDecal( int iDamageFlag );
    void CheckIfBreakableGlass( TraceResult *pTrace, CBaseEntity *pEnt, const Vector &vDir, int iDamageFlag );
    void DoBulletPenetration( entvars_t *pevAttacker, float flDamage, TraceResult *pTrace, CBaseEntity *pEnt, const Vector &vSrc, const Vector &vDir, int iPenetration, int iBulletType );
#endif

	// Fake physics "engine". Wow... So amazing... I guess???
    namespace Physics
    {
		// Simulate the "physics" when in the air etc.
	    bool Simulate( CBaseEntity *pEnt );
		// We hit something, change our angles to the new hit position
	    void OnHit( CBaseEntity *pEnt, CBaseEntity *pHit );
		// If we hit something, reset the angles towards it
	    void ResetAngles( CBaseEntity *pEnt );
    }

	// Normalize our vector output
	inline Vector2D Normalize( const Vector2D &input )
	{
	    float flLen = input.Length();
		if ( flLen == 0 )
			return Vector2D( 0, 0 );
		else
		{
			flLen = 1 / flLen;
			return Vector2D( input.x * flLen, input.y * flLen );
		}
	}

	enum RoundState
	{
		RoundState_Invalid = -1,
		RoundState_WaitingForPlayers = 0,
		RoundState_RoundIsStarting,
		RoundState_PickVolunteers,
		RoundState_RoundHasBegunPost,
		RoundState_RoundHasBegun,
		RoundState_RoundIsOver
	};
	
	enum Teams_e
	{
		TEAM_NONE,
		TEAM_OBSERVER,
		TEAM_SURVIVIOR,
		TEAM_ZOMBIE,
	
		MAX_TEAM
	};

	static int ExtraPanicSpeed = 60;

	static int MaxSpeeds[2] = {
		230,	// Human Max Speed
		228		// Zombie Max Speed
	};

	static int MaxHealth[2] = {
		100,	// Human Max Health
		200		// Zombie Max Health
	};

	static const char *Teams[MAX_TEAM] = {
		"",
		"Observer",
		"Survivor",
		"Zombie"
	};

	enum GameModeType_e
	{
		GAMEMODE_INVALID = -1,
		GAMEMODE_NONE,
		GAMEMODE_SURVIVAL,
		GAMEMODE_OBJECTIVE,
		GAMEMODE_HARDCORE,
	
		MAX_GAMEMODES
	};
	GameModeType_e IsValidGameModeMap(const char *szLevel);

	enum BotGameEventTypes_e
	{
		BOT_EVENT_NONE = 0,
		BOT_EVENT_WEAPON_FIRED,			// A weapon was fired
		BOT_EVENT_WEAPON_UNLOADED,		// A weapon was unloaded
		BOT_EVENT_WEAPON_RELOADED,		// A weapon was reloaded
		BOT_EVENT_WEAPON_DROPPED,		// A weapon was dropped
		BOT_EVENT_EXPLOSION,			// An explosion occurred
		BOT_EVENT_THROWABLE,			// A throwable was thrown
		BOT_EVENT_THROWABLE_BOUNCED,	// A throwable bounced
		BOT_EVENT_ENTITY_BROKEN,		// An entity was broken
		BOT_EVENT_DOOR_OPENED,			// A door was opened
		BOT_EVENT_PLAYER_KILLED,		// Player was killed, either by another player or by the world.
		BOT_EVENT_PLAYER_JOINED,		// Player joined the game
		BOT_EVENT_PLAYER_LEFT,			// Player left the game
		BOT_EVENT_PLAYER_ESCAPED,		// Player escaped
		BOT_EVENT_PLAYER_FOOTSTEP,		// Player made a loud footstep
	    BOT_EVENT_PLAYER_LANDED_FROM_HEIGHT, // Player landed from a height, causing a loud noise
		BOT_EVENT_OBJECTIVE_COMPLETED,	// An objective was completed
		BOT_EVENT_OBJECTIVE_FAILED,		// An objective was failed
		BOT_EVENT_CAN_ESCAPE,			// The survivors can escape, so the bots should try to get to the escape zone
		BOT_EVENT_ROUND_STARTED,		// A new round has started
		BOT_EVENT_ROUND_ENDED,			// A round has ended

		BOT_EVENT_MAX
    };
    extern char *BotGameEventName[BOT_EVENT_MAX];

	namespace ColorGradient
	{
		class ColorBase
		{
		public:
			int r;
			int g;
			int b;
			int a;
			float flValue;
			ColorBase( Color cColor, float flPercent )
			{
				r = cColor.r();
				g = cColor.g();
				b = cColor.b();
				a = cColor.a();
				flValue = flPercent;
			}
		};
	
		class Base
		{
		public:
			void AddColor( ColorBase *value )
			{
				m_Colors.push_back( value );
			}
	
			Color GetColorForValue( float flValue, float flAlpha = 255 )
			{
				if ( m_Colors.size() < 1 )
					return Color( 255, 0, 255, flAlpha );
	
				for ( int i = 0; i < int(m_Colors.size()); i++ )
				{
					ColorBase *sCurrent = m_Colors[i];
	
					if ( flValue <= sCurrent->flValue )
					{
						int y = i-1;
						if ( y < 0 ) y = 0;
	
						ColorBase *sPrevious = m_Colors[y];
	
						float flDiff = (sPrevious->flValue - sCurrent->flValue);
						float flFrac = flDiff == 0.0f ? 0.0f : (flValue - sCurrent->flValue) / flDiff;
						int iRed     = Float2Int( (sPrevious->r - sCurrent->r) * flFrac + sCurrent->r );
						int iGreen   = Float2Int( (sPrevious->g - sCurrent->g) * flFrac + sCurrent->g );
						int iBlue    = Float2Int( (sPrevious->b - sCurrent->b) * flFrac + sCurrent->b );
	
						return Color( iRed, iGreen, iBlue, flAlpha );
					}
				}
				return Color( 255, 255, 255, flAlpha );
			}
	
			std::vector<ColorBase*> m_Colors;
		};
	
		class WhiteGreen : public Base
	    {
		public:
			WhiteGreen()
			{
				AddColor( new ColorBase( Color( 255, 255, 255 ), 0.099999999999999f ) );
				AddColor( new ColorBase( Color( 0, 255, 0 ), 1.000000f ) );
			}
		};
	
		class WhiteYellow : public Base
	    {
		public:
			WhiteYellow()
			{
				AddColor( new ColorBase( Color( 255, 255, 255 ), 0.099999999999999f ) );
				AddColor( new ColorBase( Color( 255, 255, 0 ), 1.000000f ) );
			}
		};
	
		class RedYellowGreen : public Base
		{
		public:
			RedYellowGreen()
			{
				AddColor( new ColorBase( Color( 255, 0, 0 ), 0.333333f ) ); // full red
				AddColor( new ColorBase( Color( 255, 255, 0 ), 0.666666f ) ); // full yellow
				AddColor( new ColorBase( Color( 0, 255, 0 ), 1.000000f ) ); // full green
			}
		};
	
		class RainBow : public Base
	    {
		public:
			RainBow()
			{
				AddColor( new ColorBase( Color( 255, 0, 0 ), 0.099999999999999f ) );
				AddColor( new ColorBase( Color( 255, 255, 0 ), 0.1666666666666667f ) );
				AddColor( new ColorBase( Color( 0, 255, 0 ), 0.5f ) );
				AddColor( new ColorBase( Color( 0, 255, 255 ), 0.6666666666666667f ) );
				AddColor( new ColorBase( Color( 0, 0, 255 ), 0.8333333333333333f ) );
				AddColor( new ColorBase( Color( 255, 0, 255 ), 1.000000f ) );
			}
		};
	}
}

#endif