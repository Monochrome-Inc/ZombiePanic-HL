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

===== player.cpp ========================================================

  functions dealing with the player

*/

#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "player.h"
#include "trains.h"
#include "nodes.h"
#include "weapons.h"
#include "soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "hltv.h"
#include "zp/gamemodes/zp_gamemodebase.h"
#include "zp/weapons/CWeaponBase.h"
#include "zp/weapons/CWeaponBaseSingleAction.h"
#include "zp/weapons/weapon_sidearm_revolver.h"
#include "zp/zp_gamerules.h"
#include "zp/zp_shared.h"
#include "zp/zp_spawnpoint_ent.h"

#include <tier2/tier2.h>

// for std::vector random_shuffle
#include <iterator>
#include <random>
#include <algorithm>

// #define DUCKFIX

extern DLL_GLOBAL ULONG g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL g_fGameOver;
extern DLL_GLOBAL BOOL g_fDrawLines;
int gEvilImpulse101;
extern DLL_GLOBAL int g_iSkillLevel, gDisplayTitle;
extern ConVar mp_hidecorpses;

BOOL gInitHUD = TRUE;

extern void CopyToBodyQue(entvars_t *pev);
extern Vector VecBModelOrigin(entvars_t *pevBModel);
extern edict_t *EntSelectSpawnPoint(CBasePlayer *pPlayer);

class CPlayerCorpse;
static bool SpawnAnimatedPlayerCorpse(CBasePlayer *pPlayer);

static constexpr int PLAYER_CORPSE_ANIM_MARKER = 31337;

namespace
{
bool CanProcessRoundGameplayInput( const CBasePlayer *pPlayer )
{
	if ( !pPlayer ) return false;
	return ZP::GetCurrentRoundState() == ZP::RoundState::RoundState_RoundHasBegun;
}

void UpdateRoundInputLockPhysicsKey( CBasePlayer *pPlayer )
{
	if ( !pPlayer ) return;

	const bool bShouldLock = pPlayer->IsConnected()
		&& pPlayer->IsAlive()
		&& !pPlayer->IsObserver()
		&& pPlayer->pev->team != ZP::TEAM_OBSERVER
		&& ZP::GetCurrentRoundState() != ZP::RoundState::RoundState_RoundHasBegun;

	g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "zprl", bShouldLock ? "1" : "0" );
}
}

// the world node graph
extern CGraph WorldGraph;

PlayerCharacterType g_CharacterTypes[] = {
	{ SURVIVOR1, "eugene", "survivor1", "undead" },
	{ SURVIVOR2, "marcus", "survivor2", "undead2" },
	{ SURVIVOR3, "david", "survivor3", "undead3" },
};

void PrecachePlayerModels()
{
	// Survivors
	PRECACHE_MODEL( "models/player/survivor1/survivor1.mdl" );
	PRECACHE_MODEL( "models/player/survivor2/survivor2.mdl" );
	PRECACHE_MODEL( "models/player/survivor3/survivor3.mdl" );

	// Zombies
	PRECACHE_MODEL( "models/player/undead/undead.mdl" );
	PRECACHE_MODEL( "models/player/undead2/undead2.mdl" );
	PRECACHE_MODEL( "models/player/undead3/undead3.mdl" );
}

bool UTIL_IsValidPlayerModel( const char *szModel, bool bIsZombie )
{
	for ( int i = 0; i < ARRAYSIZE( g_CharacterTypes ); i++ )
	{
		const char *szModelCheck = bIsZombie ? g_CharacterTypes[i].Zombie : g_CharacterTypes[i].Survivor;
		if ( FStrEq( szModel, szModelCheck ) ) return true;
	}
	return false;
}

std::vector<VocalizeData> m_VocalizeData;

static ConVar sv_allow_player_decals( "sv_allow_player_decals", "1", FCVAR_SERVER, "Whether player decals are allowed" );
static ConVar sv_player_runspeed_gait_speed( "sv_player_runspeed_gait_speed", "100", FCVAR_SERVER, "The speed at which the player will switch to the running gait" );

#define TRAIN_ACTIVE  0x80
#define TRAIN_NEW     0xc0
#define TRAIN_OFF     0x00
#define TRAIN_NEUTRAL 0x01
#define TRAIN_SLOW    0x02
#define TRAIN_MEDIUM  0x03
#define TRAIN_FAST    0x04
#define TRAIN_BACK    0x05

#define FLASH_DRAIN_TIME  1.2 //100 units/3 minutes
#define FLASH_CHARGE_TIME 0.2 // 100 units/20 seconds  (seconds per unit)

#define ZVISION_BLIGHT 0

#define BGROUP_BODY 0
#define BGROUP_HEAD 1
#define BGROUP_EYEGLOW 2
#define BGROUP_BACKPACK 3

#define BGROUP_SUB_DEFAULT 0
#define BGROUP_SUB_VALUE1 1

constexpr int HalfPlayerHeight = 36;
constexpr int HeightTolerance = 20;
constexpr float ItemSearchRadius = 512;

enum class SpawnPointValidity
{
	NonValid,
	NonValidDisabled,
	NonValidWasRecentlyUsed,
	NonValidAlreadyUsed,
	NonValidOccupied,
	NonValidInPVS,
	Valid
};

struct SpotInfo
{
	CBaseEntity *pSpot = nullptr;
};

// Global Savedata for player
TYPEDESCRIPTION CBasePlayer::m_playerSaveData[] = {
	DEFINE_FIELD(CBasePlayer, m_flFlashLightTime, FIELD_TIME),
	DEFINE_FIELD(CBasePlayer, m_iFlashBattery, FIELD_INTEGER),

	DEFINE_FIELD(CBasePlayer, m_afButtonLast, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_afButtonPressed, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_afButtonReleased, FIELD_INTEGER),

	DEFINE_ARRAY(CBasePlayer, m_rgItems, FIELD_INTEGER, MAX_ITEMS),
	DEFINE_FIELD(CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER),

	DEFINE_FIELD(CBasePlayer, m_flTimeStepSound, FIELD_TIME),
	DEFINE_FIELD(CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME),
	DEFINE_FIELD(CBasePlayer, m_flSwimTime, FIELD_TIME),
	DEFINE_FIELD(CBasePlayer, m_flDuckTime, FIELD_TIME),
	DEFINE_FIELD(CBasePlayer, m_flWallJumpTime, FIELD_TIME),

	DEFINE_FIELD(CBasePlayer, m_flSuitUpdate, FIELD_TIME),
	DEFINE_ARRAY(CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST),
	DEFINE_FIELD(CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER),
	DEFINE_ARRAY(CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT),
	DEFINE_ARRAY(CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT),
	DEFINE_FIELD(CBasePlayer, m_lastDamageAmount, FIELD_INTEGER),

	DEFINE_ARRAY(CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES),
	DEFINE_FIELD(CBasePlayer, m_pActiveItem, FIELD_CLASSPTR),
	DEFINE_FIELD(CBasePlayer, m_pLastItem, FIELD_CLASSPTR),

	DEFINE_ARRAY(CBasePlayer, m_rgAmmo, FIELD_INTEGER, ZPAmmoTypes::AMMO_MAX),
	DEFINE_FIELD(CBasePlayer, m_idrowndmg, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_idrownrestored, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_tSneaking, FIELD_TIME),

	DEFINE_FIELD(CBasePlayer, m_iTrain, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_flFallVelocity, FIELD_FLOAT),
	DEFINE_FIELD(CBasePlayer, m_iTargetVolume, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iWeaponVolume, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iWeaponFlash, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_fLongJump, FIELD_BOOLEAN),
	DEFINE_FIELD(CBasePlayer, m_fInitHUD, FIELD_BOOLEAN),
	DEFINE_FIELD(CBasePlayer, m_tbdPrev, FIELD_TIME),

	DEFINE_FIELD(CBasePlayer, m_pTank, FIELD_EHANDLE),
	DEFINE_FIELD(CBasePlayer, m_iHideHUD, FIELD_INTEGER),
	DEFINE_FIELD(CBasePlayer, m_iFOV, FIELD_INTEGER),

	//DEFINE_FIELD( CBasePlayer, m_fDeadTime, FIELD_FLOAT ), // only used in multiplayer games
	//DEFINE_FIELD( CBasePlayer, m_fGameHUDInitialized, FIELD_INTEGER ), // only used in multiplayer games
	//DEFINE_FIELD( CBasePlayer, m_flStopExtraSoundTime, FIELD_TIME ),
	//DEFINE_FIELD( CBasePlayer, m_fKnownItem, FIELD_INTEGER ), // reset to zero on load
	//DEFINE_FIELD( CBasePlayer, m_iPlayerSound, FIELD_INTEGER ),	// Don't restore, set in Precache()
	//DEFINE_FIELD( CBasePlayer, m_pentSndLast, FIELD_EDICT ),	// Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_flSndRoomtype, FIELD_FLOAT ),	// Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_flSndRange, FIELD_FLOAT ),	// Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_fNewAmmo, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_flgeigerRange, FIELD_FLOAT ),	// Don't restore, reset in Precache()
	//DEFINE_FIELD( CBasePlayer, m_flgeigerDelay, FIELD_FLOAT ),	// Don't restore, reset in Precache()
	//DEFINE_FIELD( CBasePlayer, m_igeigerRangePrev, FIELD_FLOAT ),	// Don't restore, reset in Precache()
	//DEFINE_FIELD( CBasePlayer, m_iStepLeft, FIELD_INTEGER ), // Don't need to restore
	//DEFINE_ARRAY( CBasePlayer, m_szTextureName, FIELD_CHARACTER, CBTEXTURENAMEMAX ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_chTextureType, FIELD_CHARACTER ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_fNoPlayerSound, FIELD_BOOLEAN ), // Don't need to restore, debug
	//DEFINE_FIELD( CBasePlayer, m_iUpdateTime, FIELD_INTEGER ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_iClientHealth, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_iClientBattery, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_iClientHideHUD, FIELD_INTEGER ), // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_fWeapon, FIELD_BOOLEAN ),  // Don't restore, client needs reset
	//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't restore, depends on server message after spawning and only matters in multiplayer
	//DEFINE_FIELD( CBasePlayer, m_vecAutoAim, FIELD_VECTOR ), // Don't save/restore - this is recomputed
	//DEFINE_ARRAY( CBasePlayer, m_rgAmmoLast, FIELD_INTEGER, MAX_AMMO_SLOTS ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_fOnTarget, FIELD_BOOLEAN ), // Don't need to restore
	//DEFINE_FIELD( CBasePlayer, m_nCustomSprayFrames, FIELD_INTEGER ), // Don't need to restore

};

int giPrecacheGrunt = 0;
int gmsgShake = 0;
int gmsgFade = 0;
int gmsgSelAmmo = 0;
int gmsgFlashlight = 0;
int gmsgFlashBattery = 0;
int gmsgResetHUD = 0;
int gmsgInitHUD = 0;
int gmsgShowGameTitle = 0;
int gmsgCurWeapon = 0;
int gmsgHealth = 0;
int gmsgPanic = 0;
int gmsgVocalize = 0;
int gmsgDamage = 0;
int gmsgBattery = 0;
int gmsgRounds = 0;
int gmsgAmmoBankUpdate = 0;
int gmsgZombieLives = 0;
int gmsgRoundState = 0;
int gmsgTrain = 0;
int gmsgLogo = 0;
int gLogoState = 0;
int gmsgWeaponList = 0;
int gmsgAmmoX = 0;
int gmsgHudText = 0;
int gmsgDeathMsg = 0;
int gmsgScoreInfo = 0;
int gmsgTeamInfo = 0;
int gmsgTeamScore = 0;
int gmsgGameMode = 0;
int gmsgMOTD = 0;
int gmsgHtmlMOTD = 0;
int gmsgServerName = 0;
int gmsgAmmoPickup = 0;
int gmsgWeapPickup = 0;
int gmsgItemPickup = 0;
int gmsgHideWeapon = 0;
int gmsgSetCurWeap = 0;
int gmsgSayText = 0;
int gmsgSayConsole = 0;
int gmsgTextMsg = 0;
int gmsgSetFOV = 0;
int gmsgShowMenu = 0;
#if 0
int gmsgGeigerRange = 0;
#endif
int gmsgTeamNames = 0;

int gmsgStatusText = 0;
int gmsgStatusValue = 0;

int gmsgSpectator = 0;
int gmsgAllowSpec = 0;

int gmsgViewMode = 0;
int gmsgVGUIMenu = 0;
int gmsgStatusIcon = 0;
int gmsgFog = 0;
int gmsgWeather = 0;

int gmsgObjective = 0;
int gmsgBeaconDraw = 0;
int gmsgRoundResetPre = 0;
int gmsgRoundResetPost = 0;
int gmsgAchievement = 0;
int gmsgAchEarned = 0;
int gmsgRoundTime = 0;
int gmsgMouseFix = 0;

int gmsgZPAPICall = 0;

// When a weapon is given/removed.
// We need to send a message to the client to update the weapon selection menu.
int gmsgWeaponGiven = 0;

int gmsgBarricadeBuildProgress = 0;

// Debugging
int gmsgDebugLine = 0;

const char *const gCustomMessages[] = {
	"IconInfo",
	"CheatCheck",
	"Splash",
	"Countdown",
	"Timer",
	"IconInfo",
	NULL
};

void LinkUserMessages(void)
{
	// Already taken care of?
	if (gmsgSelAmmo)
	{
		return;
	}

	gmsgSelAmmo = REG_USER_MSG("SelAmmo", sizeof(SelAmmo));
	gmsgCurWeapon = REG_USER_MSG("CurWeapon", 4);
#if 0
	gmsgGeigerRange = REG_USER_MSG("Geiger", 1);
#endif
	gmsgFlashlight = REG_USER_MSG("Flashlight", 3);
	gmsgFlashBattery = REG_USER_MSG("FlashBat", 1);
	gmsgHealth = REG_USER_MSG("Health", 1);
	gmsgPanic = REG_USER_MSG("Panic", 0);
	gmsgVocalize = REG_USER_MSG("Voice", -1);
	gmsgDamage = REG_USER_MSG("Damage", 12);
	gmsgBattery = REG_USER_MSG("Battery", 2);
	gmsgZombieLives = REG_USER_MSG("ZombieLives", 2);
	gmsgRounds = REG_USER_MSG("Rounds", -1);
	gmsgAmmoBankUpdate = REG_USER_MSG("AmmoBank", -1);
	gmsgRoundState = REG_USER_MSG("RoundState", -1);
	gmsgTrain = REG_USER_MSG("Train", 1);
	gmsgLogo = REG_USER_MSG("Logo", 1);
	gmsgHudText = REG_USER_MSG("HudText", -1);
	gmsgSayText = REG_USER_MSG("SayText", -1);
	gmsgSayConsole = REG_USER_MSG("SayCon", -1);
	gmsgTextMsg = REG_USER_MSG("TextMsg", -1);
	gmsgWeaponList = REG_USER_MSG("WeaponList", -1);
	gmsgResetHUD = REG_USER_MSG("ResetHUD", 1); // called every respawn
	gmsgInitHUD = REG_USER_MSG("InitHUD", 0); // called every time a new player joins the server
	gmsgShowGameTitle = REG_USER_MSG("GameTitle", 1);
	gmsgDeathMsg = REG_USER_MSG("DeathMsg", -1);
	gmsgScoreInfo = REG_USER_MSG("ScoreInfo", 9);
	gmsgTeamInfo = REG_USER_MSG("TeamInfo", -1); // sets the name of a player's team
	gmsgTeamScore = REG_USER_MSG("TeamScore", -1); // sets the score of a team on the scoreboard
	gmsgGameMode = REG_USER_MSG("GameMode", 1);
	gmsgMOTD = REG_USER_MSG("MOTD", -1);
	gmsgHtmlMOTD = REG_USER_MSG("HtmlMOTD", -1);
	gmsgServerName = REG_USER_MSG("ServerName", -1);
	gmsgAmmoPickup = REG_USER_MSG("AmmoPickup", 2);
	gmsgWeapPickup = REG_USER_MSG("WeapPickup", -1);
	gmsgItemPickup = REG_USER_MSG("ItemPickup", -1);
	gmsgHideWeapon = REG_USER_MSG("HideWeapon", 1);
	gmsgSetFOV = REG_USER_MSG("SetFOV", 1);
	gmsgShowMenu = REG_USER_MSG("ShowMenu", -1);
	gmsgShake = REG_USER_MSG("ScreenShake", sizeof(ScreenShake));
	gmsgFade = REG_USER_MSG("ScreenFade", sizeof(ScreenFade));
	gmsgAmmoX = REG_USER_MSG("AmmoX", 2);
	gmsgTeamNames = REG_USER_MSG("TeamNames", -1);

	// Debugging
	gmsgDebugLine = REG_USER_MSG( "DebugLine", -1 );

	gmsgStatusText = REG_USER_MSG("StatusText", -1);
	gmsgStatusValue = REG_USER_MSG("StatusValue", 3);

	gmsgSpectator = REG_USER_MSG("Spectator", 2); // sends observer status on entering and exiting observer mode (it is not used in client dll)
	gmsgAllowSpec = REG_USER_MSG("AllowSpec", 1); // sends allow_spectators value (this will enable Spectate command button in Team select panel)

	gmsgViewMode = REG_USER_MSG("ViewMode", 0); // Switches client to first person mode
	gmsgVGUIMenu = REG_USER_MSG("VGUIMenu", 1); // Opens team selection menu with map briefing
	gmsgStatusIcon = REG_USER_MSG("StatusIcon", -1); // Displays specified status icon sprite in hud

	// Register messages from some custom mods to prevent "Host_Error: UserMsg: Not Present on Client"
	for (int i = 0; gCustomMessages[i] != NULL; i++)
	{
		REG_USER_MSG(gCustomMessages[i], 0);
	}

	gmsgFog = REG_USER_MSG("Fog", 11);
	gmsgWeather = REG_USER_MSG("Weather", 16);

	gmsgBeaconDraw = REG_USER_MSG("BcnD", -1);

	// When we reset our round.
	// We do not want to send too many messages, as it may cause the client to "soft crash",
	// by kicking them back to the mainmenu, aka disconnecting them. Probably due to an overflow.
	gmsgRoundResetPre = REG_USER_MSG("RRndPre", -1);
	gmsgRoundResetPost = REG_USER_MSG("RRndPost", -1);

	gmsgObjective = REG_USER_MSG("ObjMsg", -1);
	gmsgAchievement = REG_USER_MSG("GiveAch", -1);
	gmsgAchEarned = REG_USER_MSG("AchEarn", -1);
	gmsgRoundTime = REG_USER_MSG("RndTime", -1);
	gmsgMouseFix = REG_USER_MSG("MouseFix", -1);

	gmsgZPAPICall = REG_USER_MSG("APICheck", -1);
	gmsgWeaponGiven = REG_USER_MSG("WeapGiven", -1);

	gmsgBarricadeBuildProgress = REG_USER_MSG("Barricade", sizeof(float));
}

LINK_ENTITY_TO_CLASS(player, CBasePlayer);

void CBasePlayer ::Pain(bool bDrown)
{
	if ( pev->team == ZP::TEAM_ZOMBIE )
	{
		// TODO: Add pain sounds for zombies?
		return;
	}

	if ( bDrown )
		DoVocalize( PlayerVocalizeType::VOCALIZE_AUTO_PAIN_DROWN, true );
	else
		DoVocalize( PlayerVocalizeType::VOCALIZE_AUTO_PAIN, true );

	m_flImHurtDelay = gpGlobals->time + 13.0f;
}

void CBasePlayer::SetPlayerHullSize()
{
	if ( FBitSet(pev->flags, FL_DUCKING) )
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
}

/* 
 *
 */
Vector VecVelocityForDamage(float flDamage)
{
	Vector vec(RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(-100, 100), RANDOM_FLOAT(200, 300));

	if (flDamage > -50)
		vec = vec * 0.7;
	else if (flDamage > -200)
		vec = vec * 2;
	else
		vec = vec * 10;

	return vec;
}

#if 0 /*
static void ThrowGib(entvars_t *pev, char *szGibModel, float flDamage)
{
	edict_t *pentNew = CREATE_ENTITY();
	entvars_t *pevNew = VARS(pentNew);

	pevNew->origin = pev->origin;
	SET_MODEL(ENT(pevNew), szGibModel);
	UTIL_SetSize(pevNew, g_vecZero, g_vecZero);

	pevNew->velocity		= VecVelocityForDamage(flDamage);
	pevNew->movetype		= MOVETYPE_BOUNCE;
	pevNew->solid			= SOLID_NOT;
	pevNew->avelocity.x		= RANDOM_FLOAT(0,600);
	pevNew->avelocity.y		= RANDOM_FLOAT(0,600);
	pevNew->avelocity.z		= RANDOM_FLOAT(0,600);
	CHANGE_METHOD(ENT(pevNew), em_think, SUB_Remove);
	pevNew->ltime		= gpGlobals->time;
	pevNew->nextthink	= gpGlobals->time + RANDOM_FLOAT(10,20);
	pevNew->frame		= 0;
	pevNew->flags		= 0;
}
	
	
static void ThrowHead(entvars_t *pev, char *szGibModel, floatflDamage)
{
	SET_MODEL(ENT(pev), szGibModel);
	pev->frame			= 0;
	pev->nextthink		= -1;
	pev->movetype		= MOVETYPE_BOUNCE;
	pev->takedamage		= DAMAGE_NO;
	pev->solid			= SOLID_NOT;
	pev->view_ofs		= Vector(0,0,8);
	UTIL_SetSize(pev, Vector(-16,-16,0), Vector(16,16,56));
	pev->velocity		= VecVelocityForDamage(flDamage);
	pev->avelocity		= RANDOM_FLOAT(-1,1) * Vector(0,600,0);
	pev->origin.z -= 24;
	ClearBits(pev->flags, FL_ONGROUND);
}


*/
#endif

int TrainSpeed(int iSpeed, int iMax)
{
	float fSpeed, fMax;
	int iRet = 0;

	fMax = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed / fMax;

	if (iSpeed < 0)
		iRet = TRAIN_BACK;
	else if (iSpeed == 0)
		iRet = TRAIN_NEUTRAL;
	else if (fSpeed < 0.33)
		iRet = TRAIN_SLOW;
	else if (fSpeed < 0.66)
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}

void CBasePlayer ::DeathSound(void)
{
	// water death sounds
	/*
	if (pev->waterlevel == 3)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/h2odeath.wav", 1, ATTN_NONE);
		return;
	}
	*/

	if ( pev->team == ZP::TEAM_ZOMBIE )
	{
		switch (RANDOM_LONG(1, 3))
		{
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/zombiedeath1.wav", 1, ATTN_NORM);
			break;
		case 2:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/zombiedeath2.wav", 1, ATTN_NORM);
			break;
		case 3:
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/zombiedeath3.wav", 1, ATTN_NORM);
			break;
		}
	}
	else
		DoVocalize( PlayerVocalizeType::VOCALIZE_AUTO_DEATH, true );
}

// override takehealth
// bitsDamageType indicates type of damage healed.

int CBasePlayer ::TakeHealth(float flHealth, int bitsDamageType)
{
	// Don't heal if already at max health
	if ( pev->health >= pev->max_health )
		return 0;
	// Only screen fade if we actually healed some health
	if ( flHealth > 0 )
		DoScreenTint( false );
	return CBaseMonster ::TakeHealth(flHealth, bitsDamageType);
}

Vector CBasePlayer ::GetGunPosition()
{
	//	UTIL_MakeVectors(pev->v_angle);
	//	m_HackedGunPos = pev->view_ofs;
	Vector origin;

	origin = pev->origin + pev->view_ofs;

	return origin;
}

void CBasePlayer::DoHeadshotBlood( const Vector &vecPos, int iAmount )
{
	if ( iAmount > 255 ) iAmount = 255;

	int iSize = clamp( iAmount / 10, 3, 16 );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecPos );
	WRITE_BYTE( TE_BLOODSPRITE );
	WRITE_COORD( vecPos.x ); // pos
	WRITE_COORD( vecPos.y );
	WRITE_COORD( vecPos.z );
	WRITE_SHORT( g_sModelIndexBloodSprayHeadShot ); // initial sprite model
	WRITE_SHORT( g_sModelIndexBloodDrop ); // droplet sprite models
	WRITE_BYTE( BloodColor() ); // color index into host_basepal
	WRITE_BYTE( iSize ); // size
	MESSAGE_END();
}

void CBasePlayer::DoHeadshotChunk( const Vector &vecPos, short modelIndex, int iAmount, int iScale )
{
	Vector vecDir = vecPos + Vector( 0, 0, 18 );
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecPos );
	WRITE_BYTE( TE_SPRITETRAIL );
	WRITE_COORD( vecPos.x ); // start
	WRITE_COORD( vecPos.y );
	WRITE_COORD( vecPos.z );
	WRITE_COORD( vecDir.x ); // end
	WRITE_COORD( vecDir.y );
	WRITE_COORD( vecDir.z );
	WRITE_SHORT( modelIndex ); // sprite model
	WRITE_BYTE( iAmount ); // count
	WRITE_BYTE( 20 ); // life
	WRITE_BYTE( iScale ); // scale
	WRITE_BYTE( 15 ); // velocity
	WRITE_BYTE( RandomInt( 3, 10 ) ); // randomness of velocity
	MESSAGE_END();
}

void CBasePlayer::GiveCurrentAmmo()
{
	CWeaponBase *pBaseWeapon = dynamic_cast<CWeaponBase *>( m_pActiveItem );
	if ( !pBaseWeapon ) return;
	int nPrimaryAmmo = pBaseWeapon->PrimaryAmmoIndex();
	if ( nPrimaryAmmo == -1 ) return;
	AmmoData data = GetAmmoByAmmoID( nPrimaryAmmo );
	if ( data.AmmoType == ZPAmmoTypes::AMMO_NONE ) return;
	if ( data.MaxCarry <= 0 && !data.DrawClip ) return;
	if ( data.MaxCarry > 0 )
		PickupAmmo( data.MaxCarry, data );
	else
		pBaseWeapon->OnClipIncrease( pBaseWeapon->iMaxClip() );
	EMIT_SOUND( ENT(pev), CHAN_ITEM, "items/ammo_pickup.wav", 1, ATTN_NORM );
}

const char *CBasePlayer::GetPlayerName() const
{
	return STRING( pev->netname );
}

int CBasePlayer::GetTeamNumber() const
{
	return pev->team;
}

void CBasePlayer::DoHeadshotExploded( const Vector &vecPos )
{
	int iSize = 1;
	int iSize_large = 2;
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_EyeBall, 2, iSize );
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_Bone1, 1, iSize_large );
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_Bone2, 1, iSize_large );
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_Bone3, 1, iSize_large );
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_Bone4, 1, iSize_large );
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_Teeth, 4, iSize );
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_Jaw1, 1, iSize_large );
	DoHeadshotChunk( vecPos, g_sModelIndexHeadshotChunk_Jaw2, 1, iSize_large );
}


//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer ::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if (pev->takedamage)
	{
		bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
		bool bIsZombie = ( pevAttacker->team == ZP::TEAM_ZOMBIE );

		m_LastHitGroup = ptr->iHitgroup;

		switch (ptr->iHitgroup)
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			// In hardcore, zombies have the same head multiplier as humans
			if ( bIsZombie )
				flDamage *= bIsInHardcore ? gSkillData.plrHead : gSkillData.plrHeadZombie;
			else
				flDamage *= gSkillData.plrHead;
			break;
		case HITGROUP_CHEST:
			flDamage *= gSkillData.plrChest;
			break;
		case HITGROUP_STOMACH:
			flDamage *= gSkillData.plrStomach;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			flDamage *= gSkillData.plrArm;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= gSkillData.plrLeg;
			break;
		default:
			break;
		}

		// Are we in hardcore mode?
		if ( bIsInHardcore )
		{
			// In hardcore mode, headshots do double damage (except zombies deal extra 80% more)
			// everything else is halved by 50% (if human)
			// 2026/03/14 - This now only applies if we have no armor.
			if ( m_LastHitGroup == HITGROUP_HEAD && pev->armorvalue <= 0 )
				flDamage *= bIsZombie ? 8.0 : 2.0f;
			else
			{
				// Only humans deal reduced damage, not zombies
				if ( pevAttacker->team == ZP::TEAM_SURVIVIOR )
					flDamage *= 0.5f;
			}
		}

		if ( m_LastHitGroup == HITGROUP_HEAD )
			DoHeadshotBlood( ptr->vecEndPos, (int)flDamage + 15 );

		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage); // a little surface blood.
		TraceBleed(flDamage, vecDir, ptr, bitsDamageType);
		AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
	}
}

/*
	Take some damage.  
	NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
	etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define ARMOR_RATIO 0.2 // Armor Takes 80% of the damage
#define ARMOR_BONUS 0.5 // Each Point of Armor is work 1/x points of health

int CBasePlayer ::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = bitsDamageType;
	int ffound = TRUE;
	int fmajor;
	int fcritical;
	int fTookDamage;
	int ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = pev->health;

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	if ((bitsDamageType & DMG_BLAST) && g_pGameRules->IsMultiplayer())
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	// Check the round state before we take any damage!
	ZP::RoundState nRoundState = ZP::GetCurrentRoundState();
	// Round isn't even going, don't allow any damage!
	if ( nRoundState != ZP::RoundState::RoundState_RoundHasBegun ) return 0;

	// Already dead
	if (!IsAlive())
		return 0;
	// go take the damage first

	CBaseEntity *pInflictor = CBaseEntity::Instance(pevInflictor);
	CBaseEntity *pAttackerEntity = CBaseEntity::Instance(pevAttacker);
	CBasePlayer *pAttacker = dynamic_cast<CBasePlayer *>( pAttackerEntity );

	if (!g_pGameRules->FPlayerCanTakeDamage(this, pInflictor, pAttackerEntity))
	{
		// Refuse the damage
		return 0;
	}

	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// Armor.
	if (pev->armorvalue && !(bitsDamageType & (DMG_FALL | DMG_DROWN))) // armor doesn't protect against fall or drown damage!
	{
		float flNew = flDamage * flRatio;

		float flArmor;

		flArmor = (flDamage - flNew) * flBonus;

		// Does this use more armor than we have?
		if (flArmor > pev->armorvalue)
		{
			flArmor = pev->armorvalue;
			flArmor *= (1 / flBonus);
			flNew = flDamage - flArmor;
			pev->armorvalue = 0;
		}
		else
			pev->armorvalue -= flArmor;

		flDamage = flNew;
	}

	if ( pAttacker )
		AddToAssistDamage( pAttacker, flDamage );

	// Are we in buddha mode?
	bool bBuddhaDamage = false;
	if ( m_bBuddhaMode )
	{
		int iDamageCheck = (int)flDamage;
		float tempHP = flHealthPrev;
		tempHP -= iDamageCheck;
		if ( tempHP <= 0 )
		{
			pev->health = 1;
			bBuddhaDamage = true;
		}
	}

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	if ( bBuddhaDamage )
		fTookDamage = 1;
	else
		fTookDamage = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, (int)flDamage, bitsDamageType);

	// reset damage time countdown for each type of time based damage player just sustained

	{
		for (int i = 0; i < CDMG_TIMEBASED; i++)
			if (bitsDamageType & (DMG_PARALYZE << i))
				m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN(MSG_SPEC, SVC_DIRECTOR);
	WRITE_BYTE(9); // command length in bytes
	WRITE_BYTE(DRC_CMD_EVENT); // take damage event
	WRITE_SHORT(ENTINDEX(this->edict())); // index number of primary entity
	WRITE_SHORT(ENTINDEX(ENT(pevInflictor))); // index number of secondary entity
	WRITE_LONG(5); // eventflags (priority and flags)
	MESSAGE_END();

	// how bad is it, doc?

	ftrivial = (pev->health > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (pev->health < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

	// DMG_BURN
	// DMG_FREEZE
	// DMG_BLAST
	// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1; // make sure the damage bits get resent

	if ( fTookDamage )
	{
		bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
		if ( (bitsDamage & DMG_SLASH)
		    && pAttacker
			&& pev->team == ZP::TEAM_SURVIVIOR )
		{
			if ( bIsInHardcore )
			{
				pAttacker->IncreaseBleed( entindex() );
				m_bIsBleeding = true;
				m_bGotBandage = false;
			}
			DoScreenTint( true );
		}
		else
			DoScreenTint( true );
		Pain( (bitsDamage & DMG_DROWN) );
	}

	pev->punchangle.x = -2;

	// We got damaged!
	m_bRegenUpdated = true;

	return fTookDamage;
}

//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
void CBasePlayer::PackDeadPlayerItems(void)
{
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon *rgpPackWeapons[MAX_WEAPONS];
	int iPackAmmo[ZPAmmoTypes::AMMO_MAX];
	int iPW = 0; // index into packweapons array
	int iPA = 0; // index into packammo array

	memset(rgpPackWeapons, NULL, sizeof(rgpPackWeapons));
	memset(iPackAmmo, -1, sizeof(iPackAmmo));

	// get the game rules
	iWeaponRules = g_pGameRules->DeadPlayerWeapons(this);
	iAmmoRules = g_pGameRules->DeadPlayerAmmo(this);

	bool bInPanic = IsInPanic();
	bool bHasItems = false;

	// go through all of the weapons and make a list of the ones to pack
	for (i = 0; i < MAX_ITEM_TYPES && iPW < MAX_WEAPONS; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];
			while (pPlayerItem && iPW < MAX_WEAPONS)
			{
				CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)pPlayerItem;
				bool bValidWeaponToDrop = true;
				// Check if this is the crowbar, if so, do not pack it!
				if ( FStrEq( STRING( pPlayerItem->pev->classname ), "weapon_swipe" ) )
					bValidWeaponToDrop = false;
				// If throwable, don't pack em up if they are active!
				ThrowableDropState throwablestate = IsThrowableAndActive( pWeapon, false );
				if ( throwablestate == ThrowableDropState::DELETE_ITEM || throwablestate == ThrowableDropState::DELETE_ITEM_AND_ACTIVE )
					bValidWeaponToDrop = false;

				if ( bValidWeaponToDrop )
				{
					rgpPackWeapons[iPW++] = pWeapon;
					bHasItems = true;
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	// now go through ammo and make a list of which types to pack.
	for (i = 0; i < ZPAmmoTypes::AMMO_MAX; i++)
	{
		if (m_rgAmmo[i] > 0)
		{
			// player has some ammo of this type.
			iPackAmmo[iPA++] = i;
			bHasItems = true;
		}
	}

	// We have nothing to drop.
	if ( !bHasItems ) return;

	// create a box to pack the stuff into.
	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create("weaponbox", pev->origin, pev->angles, edict());

	pWeaponBox->pev->angles.x = 0; // don't let weaponbox tilt.
	pWeaponBox->pev->angles.z = 0;

	// Stays forever
	pWeaponBox->pev->nextthink = 0;

	// We just shat ourselves, refuse any pickup for this item for X amount of seconds
	//if ( bInPanic )
	//	pWeaponBox->DisallowPickupFor( 2.5f );

	// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

	// pack the ammo
	while (iPackAmmo[iPA] != -1)
	{
		pWeaponBox->PackAmmo( MAKE_STRING( GetAmmoByAmmoID(iPackAmmo[iPA]).AmmoName ), m_rgAmmo[iPackAmmo[iPA]] );
		iPA++;
	}

	// now pack all of the items in the lists
	while (rgpPackWeapons[iPW])
	{
		// weapon unhooked from the player. Pack it into der box.
		pWeaponBox->PackWeapon(rgpPackWeapons[iPW]);

		iPW++;
	}

	// Let's not move the weapon box on death and/or panic
#if 0
	pWeaponBox->pev->velocity = pev->velocity * 1.2; // weaponbox has player's velocity, then some.
#endif

	RemoveAllItems(TRUE); // now strip off everything that wasn't handled by the code above.

	// If we are in a panic state, give our crowbar back!
	if ( bInPanic )
	{
		// Give back our "suit"
		SetWeaponOwn( WEAPON_SUIT, true );
		// Reset our HUD back to default
		m_iHideHUD = 0;
	}
}

void CBasePlayer::RemoveAllItems(BOOL removeSuit)
{
	if (m_pActiveItem)
	{
		ResetAutoaim();
		m_pActiveItem->Holster();
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	int i;
	CBasePlayerItem *pPendingItem;
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		m_rgpPlayerItems[i] = NULL;
		while (m_pActiveItem)
		{
			pPendingItem = m_pActiveItem->m_pNext;
			m_pActiveItem->m_pPlayer = NULL;
			m_pActiveItem->Kill();
			m_pActiveItem = pPendingItem;
		}
	}
	m_pActiveItem = NULL;

	pev->viewmodel = 0;
	pev->weaponmodel = 0;

	if ( removeSuit )
		ClearWeaponOwn( false );
	else
		ClearWeaponOwn( true );

	for (i = 0; i < ZPAmmoTypes::AMMO_MAX; i++)
		m_rgAmmo[i] = 0;

	UpdateClientData();
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	MESSAGE_END();
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
entvars_t *g_pevLastInflictor; // Set in combat.cpp.  Used to pass the damage inflictor for death messages.
    // Better solution:  Add as parameter to all Killed() functions.

void CBasePlayer::Killed(entvars_t *pevAttacker, int iGib)
{
	CSound *pSound;

	bool bShouldGib = false;
	if ((pev->health < -40 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		// If the damage that killed us was blast or crush damage, we should gib
		// We don't want to give from bullets or being smacked by melee weapons...
		if ( ( m_bitsDamageType & DMG_BLAST )
			|| ( m_bitsDamageType & DMG_CRUSH )
			|| ( m_bitsDamageType & DMG_ALWAYSGIB ) )
			bShouldGib = true;
	}

	// Holster weapon immediately, to allow it to cleanup
	if (m_pActiveItem)
		m_pActiveItem->Holster();

	// Gibbed?
	if (bShouldGib)
		m_iDeathFlags |= PLR_DEATH_FLAG_GIBBED;
	// Headshot?
	if (m_LastHitGroup == HITGROUP_HEAD)
	{
		m_iDeathFlags |= PLR_DEATH_FLAG_HEADSHOT;
		switch (RANDOM_LONG(1, 2))
		{
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_AUTO, "player/pl_headshot.wav", 1, ATTN_NORM);
			break;
		case 2:
			EMIT_SOUND(ENT(pev), CHAN_AUTO, "player/pl_headshot2.wav", 1, ATTN_NORM);
			break;
		}
		SetBodygroup( BGROUP_HEAD, BGROUP_SUB_VALUE1 );
		SetBodygroup( BGROUP_EYEGLOW, BGROUP_SUB_DEFAULT );
		// Now create some blood n' shit!
		Vector headpos = pev->origin + Vector( 0, 0, 13 );
		if ( !bShouldGib )
			DoHeadshotExploded( headpos );
		CGib::SpawnStickyGibs( pev, headpos, RANDOM_LONG( 4, 8 ) );
	}

	// Is our attacker valid, and also dead?
	CBasePlayer *pKiller = dynamic_cast<CBasePlayer *>( CBaseEntity::Instance( pevAttacker ) );
	if ( pKiller )
	{
		if ( !pKiller->IsAlive() )
			m_iDeathFlags |= PLR_DEATH_FLAG_BEYOND_GRAVE;
		else
			pKiller->m_flLastKillCheck = gpGlobals->time + 0.55f;
	}

	g_pGameRules->PlayerKilled(this, pevAttacker, g_pevLastInflictor);

	GiveAchievement( EAchievements::CHILDOFGRAVE );

	bool bIsTeamKiller = false;
	if ( ( m_iDeathFlags & PLR_DEATH_FLAG_TEAMKILLER ) != 0 )
		bIsTeamKiller = true;

	// Don't allow this if this was a team kill,
	// and make sure we are on the zombie team.
	if ( pKiller && !bIsTeamKiller && pev->team == ZP::TEAM_ZOMBIE )
		pKiller->GiveAchievement( EAchievements::GENOCIDESTEP3 );

	if (m_pTank != NULL)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
	}

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(edict()));
	{
		if (pSound)
		{
			pSound->Reset();
		}
	}

	SetAnimation(PLAYER_DIE);

	m_flRespawnTimer = 0;
	m_flDeathAnimationStartTime = gpGlobals->time;

	pev->modelindex = g_ulModelIndexPlayer; // don't use eyes

	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	ClearBits(pev->flags, FL_ONGROUND);
	if (pev->velocity.z < 10)
		pev->velocity.z += RANDOM_FLOAT(0, 300);

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);

	// send "health" update message to zero
	m_iClientHealth = 0;
	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, pev);
	WRITE_BYTE(m_iClientHealth);
	MESSAGE_END();

	m_iClientAmmoType = 0;
	if ( gmsgAmmoBankUpdate > 0 )
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoBankUpdate, NULL, pev);
		WRITE_SHORT(m_iClientAmmoType);
		MESSAGE_END();
	}

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	MESSAGE_END();

	// reset FOV
	pev->fov = m_iFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
	WRITE_BYTE(0);
	MESSAGE_END();

	// Adrian: always make the players non-solid in multiplayer when they die
	if (g_pGameRules->IsMultiplayer())
	{
		pev->solid = SOLID_NOT;
	}

	// UNDONE: Put this in, but add FFADE_PERMANENT and make fade time 8.8 instead of 4.12
	// UTIL_ScreenFade( edict(), Vector(128,0,0), 6, 15, 255, FFADE_OUT | FFADE_MODULATE );

	if (bShouldGib)
	{
		pev->solid = SOLID_NOT;
		GibMonster(); // This clears pev->model
		pev->effects |= EF_NODRAW;
		return;
	}

	DeathSound();

	pev->angles.x = 0;
	pev->angles.z = 0;

	SetThink(&CBasePlayer::PlayerDeathThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

// Set the activity based on an event or current state
void CBasePlayer::SetAnimation(PLAYER_ANIM playerAnim)
{
	int animDesired;
	float speed;
	char szAnim[64];

	speed = pev->velocity.Length2D();

	if (pev->flags & FL_FROZEN)
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}

	switch (playerAnim)
	{
	case PLAYER_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;

	case PLAYER_RELOAD_EMPTY:
		m_IdealActivity = ACT_RELOAD_EMPTY;
		break;

	case PLAYER_RELOAD_START:
		m_IdealActivity = ACT_RELOAD_START;
		break;

	case PLAYER_RELOAD_END:
		m_IdealActivity = ACT_RELOAD_END;
		break;

	case PLAYER_PUMP:
		m_IdealActivity = ACT_PUMP;
		break;

	case PLAYER_DRAW:
		m_IdealActivity = ACT_ARM;
		break;

	case PLAYER_HOLSTER:
		m_IdealActivity = ACT_DISARM;
		break;

	case PLAYER_JUMP:
		m_IdealActivity = ACT_HOP;
		break;

	case PLAYER_SUPERJUMP:
		m_IdealActivity = ACT_LEAP;
		break;

	case PLAYER_BARRICADE:
		m_IdealActivity = ACT_BARRICADE;
		break;

	case PLAYER_BARRICADE_LONG:
		m_IdealActivity = ACT_BARRICADE_LONG;
		break;

	case PLAYER_BARRICADE_NAILGUN:
		m_IdealActivity = ACT_BARRICADE_NAILGUN;
		break;

	case PLAYER_DIE:
	{
		// Reset animation, so it won't break if we were in the middle of a reload or something
		m_Activity = ACT_RESET;
		m_IdealActivity = ACT_DIESIMPLE;
		m_IdealActivity = GetDeathActivity();
	}
	break;

	case PLAYER_ATTACK1:
		switch (m_Activity)
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_RANGE_ATTACK1;
			break;
		}
		break;

	case PLAYER_ATTACK2_PRE:
		switch (m_Activity)
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_MELEE_HEAVY_ATTACK_PRE;
			break;
		}
		break;

	case PLAYER_ATTACK2_HOLD:
		switch (m_Activity)
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_MELEE_HEAVY_ATTACK_LOOP;
			break;
		}
		break;

	case PLAYER_ATTACK2_POST:
		switch (m_Activity)
		{
		case ACT_HOVER:
		case ACT_SWIM:
		case ACT_HOP:
		case ACT_LEAP:
		case ACT_DIESIMPLE:
			m_IdealActivity = m_Activity;
			break;
		default:
			m_IdealActivity = ACT_MELEE_HEAVY_ATTACK_POST;
			break;
		}
		break;
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if (!FBitSet(pev->flags, FL_ONGROUND) && (m_Activity == ACT_HOP || m_Activity == ACT_LEAP)) // Still jumping
		{
			m_IdealActivity = m_Activity;
		}
		else if (pev->waterlevel > 1)
		{
			if (speed == 0)
				m_IdealActivity = ACT_HOVER;
			else
				m_IdealActivity = ACT_SWIM;
		}
		else
		{
			m_IdealActivity = ACT_WALK;
		}
		break;
	}

	switch (m_IdealActivity)
	{
	case ACT_HOVER:
	case ACT_LEAP:
	case ACT_SWIM:
	case ACT_HOP:
	case ACT_DIESIMPLE:
	default:
		if (m_Activity == m_IdealActivity)
			return;
		m_Activity = m_IdealActivity;

		animDesired = LookupActivity(m_Activity);
		// Already using the desired animation?
		if (pev->sequence == animDesired)
			return;

		pev->gaitsequence = 0;
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo();
		return;

	case ACT_RELOAD: animDesired = SetNewActivity( "_reload", true ); break;
	case ACT_RELOAD_EMPTY: animDesired = SetNewActivity( "_reload_empty", true ); break;
	case ACT_RELOAD_START: animDesired = SetNewActivity( "_reload_start", true ); break;
	case ACT_RELOAD_END: animDesired = SetNewActivity( "_reload_end", true ); break;
	case ACT_PUMP: animDesired = SetNewActivity( "_pump", true ); break;
	case ACT_ARM: animDesired = SetNewActivity( "_draw", true ); break;
	case ACT_DISARM: animDesired = SetNewActivity( "_holster", true ); break;
	case ACT_BARRICADE: animDesired = SetNewActivity( "barricade", false ); break;
	case ACT_BARRICADE_LONG: animDesired = SetNewActivity( "barricade_long", false ); break;
	case ACT_BARRICADE_NAILGUN: animDesired = SetNewActivity( "barricade_nailgun", false ); break;

	case ACT_RANGE_ATTACK1:
		// If m_szAnimExtention is empty, then we change it to use "empty"
		if ( m_szAnimExtention[0] == 0 )
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_empty");
			else
				UTIL_strcpy(szAnim, "ref_shoot_empty");
		}
		else
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_");
			else
				UTIL_strcpy(szAnim, "ref_shoot_");
		}
		strcat(szAnim, m_szAnimExtention);
		if ( FStrEq( m_szAnimExtention, "swipe" ) )
		{
			// Randomize the swipe attack animation
			// We have left, right and both swipe animations
			int swipeAnim = RANDOM_LONG( 1, 3 );
			switch ( swipeAnim )
			{
				case 1: strcat( szAnim, "_left" ); break;
			    case 2: strcat( szAnim, "_right" ); break;
			    case 3: strcat( szAnim, "_both" ); break;
			}
		}
		animDesired = SetNewActivity( szAnim, false );
		break;

	case ACT_MELEE_HEAVY_ATTACK_PRE:
		// If crowbar, then use specific animations
		if ( FStrEq( m_szAnimExtention, "crowbar" ) )
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_heavy_pre");
			else
				UTIL_strcpy(szAnim, "ref_shoot_heavy_pre");
		}
		else
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_heavy_");
			else
				UTIL_strcpy(szAnim, "ref_shoot_heavy_");
			strcat(szAnim, m_szAnimExtention);
			strcat(szAnim, "_pre");
		}
		animDesired = SetNewActivity( szAnim, false );
		break;

	case ACT_MELEE_HEAVY_ATTACK_LOOP:
		// If crowbar, then use specific animations
		if ( FStrEq( m_szAnimExtention, "crowbar" ) )
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_heavy_loop");
			else
				UTIL_strcpy(szAnim, "ref_shoot_heavy_loop");
		}
		else
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_heavy_");
			else
				UTIL_strcpy(szAnim, "ref_shoot_heavy_");
			strcat(szAnim, m_szAnimExtention);
			strcat(szAnim, "_loop");
		}
		animDesired = SetNewActivity( szAnim, false );
		break;

	case ACT_MELEE_HEAVY_ATTACK_POST:
		// If crowbar, then use specific animations
		if ( FStrEq( m_szAnimExtention, "crowbar" ) )
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_heavy_post");
			else
				UTIL_strcpy(szAnim, "ref_shoot_heavy_post");
		}
		else
		{
			if (FBitSet(pev->flags, FL_DUCKING)) // crouching
				UTIL_strcpy(szAnim, "crouch_shoot_heavy_");
			else
				UTIL_strcpy(szAnim, "ref_shoot_heavy_");
			strcat(szAnim, m_szAnimExtention);
			strcat(szAnim, "_post");
		}
		animDesired = SetNewActivity( szAnim, false );
		break;

	case ACT_WALK:
		if ( CanActiveInteruptAnimation() )
		{
			// If m_szAnimExtention is empty, then we change it to use "empty"
			if (m_szAnimExtention[0] == 0)
			{
				if (FBitSet(pev->flags, FL_DUCKING)) // crouching
					UTIL_strcpy(szAnim, "crouch_aim_empty");
				else
					UTIL_strcpy(szAnim, "ref_aim_empty");
			}
			else
			{
				if (FBitSet(pev->flags, FL_DUCKING)) // crouching
					UTIL_strcpy(szAnim, "crouch_aim_");
				else
					UTIL_strcpy(szAnim, "ref_aim_");
			}
			strcat(szAnim, m_szAnimExtention);
			animDesired = LookupSequence(szAnim);
			if (animDesired == -1)
				animDesired = 0;
			m_Activity = ACT_WALK;
		}
		else
		{
			animDesired = pev->sequence;
		}
	}

	if (FBitSet(pev->flags, FL_DUCKING))
	{
		if (speed == 0)
		{
			pev->gaitsequence = LookupActivity(ACT_CROUCHIDLE);
			// pev->gaitsequence	= LookupActivity( ACT_CROUCH );
		}
		else
		{
			pev->gaitsequence = LookupActivity(ACT_CROUCH);
		}
	}
	else if (speed > sv_player_runspeed_gait_speed.GetInt())
	{
		pev->gaitsequence = LookupActivity(ACT_RUN);
	}
	else if (speed > 0)
	{
		pev->gaitsequence = LookupActivity(ACT_WALK);
	}
	else
	{
		// pev->gaitsequence	= LookupActivity( ACT_WALK );
		pev->gaitsequence = LookupSequence("deep_idle");
	}

	// Already using the desired animation?
	if (pev->sequence == animDesired)
		return;

	//ALERT( at_console, "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	pev->sequence = animDesired;
	pev->frame = 0;
	ResetSequenceInfo();
}

int CBasePlayer::SetNewActivity(const char *szActivity, bool bUseExt)
{
	char szAnim[64];
	if ( bUseExt )
	{
		UTIL_strcpy( szAnim, m_szAnimExtention );
		strcat( szAnim, szActivity );
	}
	else
		UTIL_strcpy( szAnim, szActivity );

	int animDesired = LookupSequence(szAnim);
	if ( animDesired == -1 )
		animDesired = 0;

	if (pev->sequence != animDesired || !m_fSequenceLoops)
	{
		pev->frame = 0;
	}

	if (!m_fSequenceLoops)
	{
		pev->effects |= EF_NOINTERP;
	}

	m_Activity = m_IdealActivity;

	pev->sequence = animDesired;
	ResetSequenceInfo();
	return animDesired;
}

/*
===========
TabulateAmmo
This function is used to find and store 
all the ammo we have into the ammo vars.
============
*/
void CBasePlayer::TabulateAmmo()
{
	ammo_9mm = AmmoInventory(GetAmmoIndex("9mm"));
	ammo_556ar = AmmoInventory(GetAmmoIndex("556ar"));
	ammo_longrifle = AmmoInventory(GetAmmoIndex("longrifle"));
	ammo_357 = AmmoInventory(GetAmmoIndex("357"));
	ammo_argrens = AmmoInventory(GetAmmoIndex("ARgrenades"));
	ammo_bolts = AmmoInventory(GetAmmoIndex("bolts"));
	ammo_buckshot = AmmoInventory(GetAmmoIndex("buckshot"));
	ammo_rockets = AmmoInventory(GetAmmoIndex("rockets"));
	ammo_uranium = AmmoInventory(GetAmmoIndex("uranium"));
	ammo_hornets = AmmoInventory(GetAmmoIndex("Hornets"));
}

/*
===========
WaterMove
============
*/
#define AIRTIME 12 // lung full of air lasts this many seconds

void CBasePlayer::WaterMove()
{
	int air;

	if (pev->movetype == MOVETYPE_NOCLIP || pev->iuser1 != OBS_NONE)
		return;

	if (pev->health < 0)
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if (pev->waterlevel != 3)
	{
		// not underwater

		// play 'up for air' sound
		if (pev->air_finished < gpGlobals->time)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade1.wav", 1, ATTN_NORM);
		else if (pev->air_finished < gpGlobals->time + 9)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade2.wav", 1, ATTN_NORM);

		pev->air_finished = gpGlobals->time + AIRTIME;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if (m_idrowndmg > m_idrownrestored)
		{
			// set drowning damage bit.  hack - dmg_drownrecover actually
			// makes the time based damage code 'give back' health over time.
			// make sure counter is cleared so we start count correctly.

			// NOTE: this actually causes the count to continue restarting
			// until all drowning damage is healed.

			m_bitsDamageType |= DMG_DROWNRECOVER;
			m_bitsDamageType &= ~DMG_DROWN;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		}
	}
	else
	{ // fully under water
		// Only drown if we are a survivor
		if ( pev->team == ZP::TEAM_SURVIVIOR )
		{
			// stop restoring damage while underwater
			m_bitsDamageType &= ~DMG_DROWNRECOVER;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;

			if (pev->air_finished < gpGlobals->time) // drown!
			{
				if (pev->pain_finished < gpGlobals->time)
				{
					// take drowning damage
					pev->dmg += 1;
					if (pev->dmg > 5)
						pev->dmg = 5;
					TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), pev->dmg, DMG_DROWN);
					pev->pain_finished = gpGlobals->time + 1;

					// track drowning damage, give it back when
					// player finally takes a breath

					m_idrowndmg += pev->dmg;
				}
			}
			else
			{
				m_bitsDamageType &= ~DMG_DROWN;
			}
		}
	}

	if (!pev->waterlevel)
	{
		if (FBitSet(pev->flags, FL_INWATER))
		{
			ClearBits(pev->flags, FL_INWATER);
		}
		return;
	}

	// make bubbles

	air = (int)(pev->air_finished - gpGlobals->time);
	if (!RANDOM_LONG(0, 0x1f) && RANDOM_LONG(0, AIRTIME - 1) >= air)
	{
		switch (RANDOM_LONG(0, 3))
		{
		case 0:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim1.wav", 0.8, ATTN_NORM);
			break;
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim2.wav", 0.8, ATTN_NORM);
			break;
		case 2:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim3.wav", 0.8, ATTN_NORM);
			break;
		case 3:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_swim4.wav", 0.8, ATTN_NORM);
			break;
		}
	}

	if (pev->watertype == CONTENT_LAVA) // do damage
	{
		if (pev->dmgtime < gpGlobals->time)
			TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 10 * pev->waterlevel, DMG_BURN);
	}
	else if (pev->watertype == CONTENT_SLIME) // do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 4 * pev->waterlevel, DMG_ACID);
	}

	if (!FBitSet(pev->flags, FL_INWATER))
	{
		SetBits(pev->flags, FL_INWATER);
		pev->dmgtime = 0;
	}
}

// TRUE if the player is attached to a ladder
BOOL CBasePlayer::IsOnLadder(void)
{
	return (pev->movetype == MOVETYPE_FLY);
}

void CBasePlayer::PlayerDeathThink(void)
{
	float flForward;

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		flForward = pev->velocity.Length() - 20;
		if (flForward <= 0)
			pev->velocity = g_vecZero;
		else
			pev->velocity = flForward * pev->velocity.Normalized();
	}

	if ( HasWeapons() )
	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
		if ( pev->team == ZP::TEAM_SURVIVIOR )
			DropEverything( false );

		// Clear our items
		RemoveAllItems( TRUE );
	}

	// Clear inclination came from client view
	pev->angles.x = 0;

	if (pev->modelindex && (!m_fSequenceFinished) && (pev->deadflag == DEAD_DYING))
	{
		StudioFrameAdvance();
		m_flRespawnTimer += gpGlobals->frametime;

		if (!mp_respawn_fix.GetBool())
		{
			// HL25 behavior: wait until animation ends.
			// If animation is longer than 4 seconds, assume it finished.
			if (m_flRespawnTimer < 4.0f)
				return;
		}
	}

	if (mp_respawn_fix.GetBool())
	{
		// BHL behavior: wait for fixed time
		// time given to animate corpse and don't allow to respawn till this time ends
		if (gpGlobals->time < m_flDeathAnimationStartTime + mp_respawn_delay.GetFloat())
		{
			if (pev->deadflag == DEAD_DYING && m_fSequenceFinished)
			{
				pev->frame = 255;
				StopAnimation();
				pev->effects |= EF_NOINTERP;
			}

			return;
		}
	}

	if (mp_respawn_fix.GetBool() && pev->deadflag == DEAD_DYING && !m_fSequenceFinished)
	{
		if (SpawnAnimatedPlayerCorpse(this))
		{
			pev->modelindex = 0;
			StopAnimation();
			pev->effects |= EF_NOINTERP;
		}
	}

	// make sure players with high fps finish the animation
	if (pev->frame < 255)
		pev->frame = 255;

	if (pev->deadflag == DEAD_DYING)
	{
		//Once we finish animating, if we're in multiplayer just make a copy of our body right away.
		if (m_fSequenceFinished && g_pGameRules->IsMultiplayer() && pev->movetype == MOVETYPE_NONE)
		{
			CopyToBodyQue(pev);
			pev->modelindex = 0;
		}

		pev->deadflag = DEAD_DEAD;
	}

	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// this prevents a bug where the dead body would go to a player's head if he walked over it while the dead player was clicking their button to respawn
	if (pev->movetype != MOVETYPE_NONE && FBitSet(pev->flags, FL_ONGROUND))
		pev->movetype = MOVETYPE_NONE;

	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->effects &= ~EF_DIMLIGHT;
	m_bInZombieVision = false;
#if ZVISION_BLIGHT
	pev->effects &= ~EF_BRIGHTLIGHT;
#endif

	m_afButtonLast = pev->button;

	// wait for all buttons released
	if (pev->deadflag == DEAD_DEAD)
	{
		if (g_pGameRules->FPlayerCanRespawn(this))
		{
			if (!(m_afPhysicsFlags & PFLAG_OBSERVER)) // don't copy a corpse if we're in deathcam.
			{
				// make a copy of the dead body for appearances sake
				CopyToBodyQue(pev);
			}
			m_fDeadTime = gpGlobals->time + 2;
			pev->deadflag = DEAD_RESPAWNABLE;
		}
		return;
	}

#if 0
	// if the player has been dead for one second longer than allowed by forcerespawn,
	// forcerespawn isn't on. Send the player off to an intermission camera until they
	// choose to respawn.
	if (g_pGameRules->IsMultiplayer() && (gpGlobals->time > m_fDeadTime) && !(m_afPhysicsFlags & PFLAG_OBSERVER))
	{
		// go to dead camera.
		StartDeathCam();
	}
#endif

	// return if player is spectating
	if (pev->iuser1)
		return;

	// wait for the time to be up
	if ( m_fDeadTime > gpGlobals->time )
		return;

	pev->button = 0;
	m_flRespawnTimer = 0.0f;
	m_flDeathAnimationStartTime = 0;

	//ALERT(at_console, "Respawn\n");
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		// Change team now.
		g_pGameRules->ChangePlayerTeam( this, ZP::Teams[ m_bNoLives ? ZP::TEAM_OBSERVER : ZP::TEAM_ZOMBIE], FALSE, FALSE );
		if ( ZPGameRules() )
			ZPGameRules()->SendPlayerTeamInfo( this );
		// respawn player
		if ( m_bNoLives )
			StartObserver();
		else
		{
			Spawn();
			GiveAchievement( EAchievements::ILIVEAGAIN );
		}
	}
	else
	{ // restart the entire server
		SERVER_COMMAND("reload\n");
	}
	pev->nextthink = -1;
}

//=========================================================
// StartDeathCam - find an intermission spot and send the
// player off into observer mode
//=========================================================
void CBasePlayer::StartDeathCam(void)
{
	edict_t *pSpot, *pNewSpot;
	int iRand;

	if (m_afPhysicsFlags & PFLAG_OBSERVER)
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	CopyToBodyQue(pev);

	const int PLAYER_HEIGHT_ADJUSTMENT = 36;

	pSpot = FIND_ENTITY_BY_CLASSNAME(NULL, "info_player_observer");
	if (!FNullEnt(pSpot))
	{
		// at least one intermission spot in the world.
		iRand = RANDOM_LONG(0, 3);

		while (iRand > 0)
		{
			pNewSpot = FIND_ENTITY_BY_CLASSNAME(pSpot, "info_player_observer");

			if (pNewSpot)
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

		UTIL_SetOrigin(pev, pSpot->v.origin - Vector( 0, 0, PLAYER_HEIGHT_ADJUSTMENT ) );
		// Find target for intermission
		edict_t *pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pSpot->v.target));
		if (pTarget && !FNullEnt(pTarget))
		{
			// Calculate angles to look at camera target
			pev->angles = UTIL_VecToAngles(pTarget->v.origin - pSpot->v.origin);
			pev->angles.x = -pev->angles.x;
		}
		else
		{
			CBaseEntity *pDefaultSpawn = CBaseEntity::Instance(pSpot);
			if ( pDefaultSpawn )
				pev->angles = pDefaultSpawn->pev->angles;
			else
				pev->angles = pSpot->v.v_angle;
		}
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, 128), ignore_monsters, edict(), &tr);
		UTIL_SetOrigin(pev, tr.vecEndPos - Vector(0, 0, 10));
		pev->angles.x = pev->v_angle.x = 90;
	}

	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->view_ofs = g_vecZero;
	pev->fixangle = TRUE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NOCLIP; // HACK HACK: Player fall down with MOVETYPE_NONE
	pev->health = 1; // Let player stay vertically, not lie on a side
	pev->effects = EF_NODRAW; // Hide model. This is used instead of pev->modelindex = 0
	//pev->modelindex = 0;				// Commented to let view point be moved
}

void CBasePlayer::SendWeaponOwnUpdate( ZPWeaponID nID, const bool &state )
{
#if 1
	// NOTICE: We only use pev->weapons bitfield for now, as it will be removed
	// after the Steam release. The bitfield is limited to 32 weapons, since we only have a few weapons in the game currently,
	// it's not a problem. Once the rest of the melee weapons are added, we will use the MSG system instead.
	if ( state )
		pev->weapons |= (1 << nID);
	else
		pev->weapons &= ~(1 << nID);
#else
	MESSAGE_BEGIN( MSG_ALL, gmsgWeaponGiven );
	WRITE_BYTE( ENTINDEX(edict()) );
	WRITE_BYTE( nID );
	WRITE_BYTE( state );
	MESSAGE_END();
#endif
}

void CBasePlayer::StartWelcomeCam(void)
{
	if (m_bInWelcomeCam)
		return;

	m_bInWelcomeCam = TRUE;

	const int PLAYER_HEIGHT_ADJUSTMENT = 36;
	edict_t *pSpot, *pNewSpot;
	pSpot = FIND_ENTITY_BY_CLASSNAME(NULL, "info_player_observer");
	if (!FNullEnt(pSpot))
	{
		// at least one intermission spot in the world.
		int iRand = RANDOM_LONG(0, 3);

		while (iRand > 0)
		{
			pNewSpot = FIND_ENTITY_BY_CLASSNAME(pSpot, "info_player_observer");

			if (pNewSpot)
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

		UTIL_SetOrigin(pev, pSpot->v.origin - Vector( 0, 0, PLAYER_HEIGHT_ADJUSTMENT ) );
		// Find target for intermission
		edict_t *pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pSpot->v.target));
		if (pTarget && !FNullEnt(pTarget))
		{
			// Calculate angles to look at camera target
			pev->angles = UTIL_VecToAngles(pTarget->v.origin - pSpot->v.origin);
			pev->angles.x = -pev->angles.x;
		}
		else
		{
			CBaseEntity *pDefaultSpawn = CBaseEntity::Instance(pSpot);
			if ( pDefaultSpawn )
				pev->angles = pDefaultSpawn->pev->angles;
			else
				pev->angles = pSpot->v.v_angle;
		}
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down
		TraceResult tr;
		UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, 128), ignore_monsters, edict(), &tr);
		UTIL_SetOrigin(pev, tr.vecEndPos - Vector(0, 0, 10));
		pev->angles.x = pev->v_angle.x = 30;
	}

	// Remove crosshair
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	MESSAGE_END();

	m_iHideHUD = HIDEHUD_WEAPONS | HIDEHUD_HEALTH;
	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->view_ofs = g_vecZero;
	pev->fixangle = TRUE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NOCLIP; // HACK HACK: Player fall down with MOVETYPE_NONE
	pev->health = 1; // Let player stay vertically, not lie on a side
	pev->deadflag = DEAD_RESPAWNABLE;
	pev->effects = EF_NODRAW; // Hide model. This is used instead of pev->modelindex = 0
	pev->team = ZP::TEAM_OBSERVER;
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "zprl", "0" );

	if ( ZPGameRules() )
		ZPGameRules()->SendPlayerTeamInfo( this );
	else
	{
		// notify everyone's HUD of the team change
		MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
		WRITE_BYTE( entindex() );
		WRITE_STRING( pev->iuser1 ? "" : TeamID() );
		MESSAGE_END();

		SendScoreInfo();
	}

	// Reset on new round and/or new map.
	ResetParticipation();
}

void CBasePlayer::StopWelcomeCam(void)
{
	m_bInWelcomeCam = FALSE;

	m_iHideHUD = 0;

	Spawn();
	pev->nextthink = -1;

	// Let's tell our gamerules that we want to do an API call.
	// We only do ths once, since we want to make sure all initializations are done.
	char szPrivateKey[64];
	szPrivateKey[0] = 0;

	// Characters we can use in the private key.
	const char *szRandomChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#";

	// Generate a private key for this player.
	// We save this until we recieve it back from the client.
	int nAmount = RandomInt( 24, 60 );
	for ( int i = 0; i < nAmount; i++ )
		szPrivateKey[i] = szRandomChars[ RandomInt( 0, strlen( szRandomChars ) - 1 ) ];
	szPrivateKey[nAmount] = 0;

	// Save the private key.
	UTIL_strcpy( m_szAPIRetrieveKey, szPrivateKey );
	CLIENT_COMMAND( ENT(pev), "api_retrieve %s\n", szPrivateKey );

	// We are about to spawn the player, let's fix any weird mouse input issues.
	MESSAGE_BEGIN(MSG_ONE, gmsgMouseFix, NULL, pev);
	MESSAGE_END();
}

void CBasePlayer::SendScoreInfo()
{
	if ( ZPGameRules() && !ZPGameRules()->IsProcessingDeferredBroadcasts() )
	{
		ZPGameRules()->SendPlayerScoreInfo( this );
		return;
	}

	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
	WRITE_BYTE(ENTINDEX(edict())); // Player index
	WRITE_SHORT(pev->frags); // Score
	WRITE_SHORT(m_iDeaths); // Deaths
	WRITE_SHORT(0); // TFC class
	WRITE_SHORT(g_pGameRules->GetTeamIndex(TeamID())); // Team index
	MESSAGE_END();
}

//
// PlayerUse - handles USE keypress
//
#define PLAYER_SEARCH_RADIUS (float)64

float IntervalDistance( float x, float x0, float x1 )
{
	// swap so x0 < x1
	if ( x0 > x1 )
	{
		float tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	if ( x < x0 )
		return x0-x;
	else if ( x > x1 )
		return x - x1;
	return 0;
}

// Backported from Source SDK 2013, so we don't interact with crap behind walls etc.
CBaseEntity *CBasePlayer::FindUseEntity()
{
	CBaseEntity *pObject = NULL;
	CBaseEntity *pClosest = NULL;
	Vector vecLOS;
	float flMaxDot = VIEW_FIELD_NARROW;
	float flDot;

	UTIL_MakeVectors(pev->v_angle); // so we know which way we are facing

	TraceResult tr;

	// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
	Vector searchCenter = EyePosition();

	float nearestDist = FLT_MAX;

	const int NUM_TANGENTS = 8;
	// trace a box at successive angles down
	//							forward, 45 deg, 30 deg, 20 deg, 15 deg, 10 deg, -10, -15
	const float tangents[NUM_TANGENTS] = { 0, 1, 0.57735026919f, 0.3639702342f, 0.267949192431f, 0.1763269807f, -0.1763269807f, -0.267949192431f };
	for ( int i = 0; i < NUM_TANGENTS; i++ )
	{
		if ( i == 0 )
		{
			UTIL_TraceLine( searchCenter, searchCenter + gpGlobals->v_forward * 1024, dont_ignore_monsters, ENT(pev), &tr );
		}
		else
		{
			Vector down = gpGlobals->v_forward - tangents[i] * gpGlobals->v_up;
			VectorNormalize(down);
			UTIL_TraceHull( searchCenter, searchCenter + down * 72, dont_ignore_monsters, head_hull, ENT(pev), &tr );
		}
		pObject = CBaseEntity::Instance( tr.pHit );
		if ( !pObject ) continue;

		if (pObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE))
		{
			// GoldSrc does not have CollisionProp(), so we cannot use that.
			// Instead, we use our Quake BBox.
			Vector vOBB[2];
			ExtractBbox( pev->sequence, vOBB[0], vOBB[1] );

			Vector delta = tr.vecEndPos - searchCenter;
			float centerZ = Center().z;
			delta.z = IntervalDistance( tr.vecEndPos.z, centerZ + vOBB[0].z, centerZ + vOBB[1].z );
			float dist = delta.Length();
			if ( dist < PLAYER_SEARCH_RADIUS )
			{
				pClosest = pObject;
				
				// if this is directly under the cursor just return it now
				if ( i == 0 )
					return pObject;
			}
		}
	}

	while ((pObject = UTIL_FindEntityInSphere(pObject, pev->origin, PLAYER_SEARCH_RADIUS)) != NULL)
	{
		if (pObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE))
		{
			// Since this has purely been a radius search to this point, we now
			// make sure the object isn't behind glass or a grate.
			TraceResult trCheckOccluded;
			UTIL_TraceLine( searchCenter, pObject->Center(), dont_ignore_monsters, ENT(pev), &trCheckOccluded );

			CBaseEntity *pCheckHit = CBaseEntity::Instance( trCheckOccluded.pHit );
			if ( trCheckOccluded.flFraction == 1.0 || pCheckHit == pObject )
			{
				// !!!PERFORMANCE- should this check be done on a per case basis AFTER we've determined that
				// this object is actually usable? This dot is being done for every object within PLAYER_SEARCH_RADIUS
				// when player hits the use key. How many objects can be in that area, anyway? (sjb)
				vecLOS = (VecBModelOrigin(pObject->pev) - (pev->origin + pev->view_ofs));

				// This essentially moves the origin of the target to the corner nearest the player to test to see
				// if it's "hull" is in the view cone
				vecLOS = UTIL_ClampVectorToBox(vecLOS, pObject->pev->size * 0.5);

				flDot = DotProduct(vecLOS, gpGlobals->v_forward);
				if ( flDot > flMaxDot )
				{
					// only if the item is in front of the user
					pClosest = pObject;
					flMaxDot = flDot;
					//ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
				}
				//ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
		}
	}
	pObject = pClosest;

	return pObject;
}

void CBasePlayer::PlayerUse(void)
{
	if ( !CanProcessRoundGameplayInput( this ) )
		return;

	// Was use pressed or released?
	if (!((pev->button | m_afButtonPressed | m_afButtonReleased) & IN_USE))
		return;

	// Hit Use on a train?
	if (m_afButtonPressed & IN_USE)
	{
		if (m_pTank != NULL)
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use(this, this, USE_OFF, 0);
			return;
		}
		else
		{
			if (m_afPhysicsFlags & PFLAG_ONTRAIN)
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				CBaseEntity *pTrain = CBaseEntity::Instance(pev->groundentity);
				if (pTrain && (pTrain->Classify() == CLASS_VEHICLE))
					static_cast<CFuncVehicle *>(pTrain)->m_pDriver = NULL;
				return;
			}
			else
			{ // Start controlling the train!
				CBaseEntity *pTrain = CBaseEntity::Instance(pev->groundentity);

				if (pTrain && !(pev->button & IN_JUMP) && FBitSet(pev->flags, FL_ONGROUND) && (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls(pev))
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
					m_iTrain |= TRAIN_NEW;
					if (pTrain->Classify() == CLASS_VEHICLE)
					{
						EMIT_SOUND(ENT(pev), CHAN_ITEM, "plats/vehicle_ignition.wav", 0.8, ATTN_NORM);
						static_cast<CFuncVehicle *>(pTrain)->m_pDriver = this;
					}
					else
					{
						EMIT_SOUND(ENT(pev), CHAN_ITEM, "plats/train_use1.wav", 0.8, ATTN_NORM);
					}
					return;
				}
			}
		}
	}

	CBaseEntity *pObject = FindUseEntity();

	// Found an object
	if (pObject)
	{
		//!!!UNDONE: traceline here to prevent USEing buttons through walls
		int caps = pObject->ObjectCaps();

		if ( ( m_afButtonPressed & IN_USE ) && ( pev->team == ZP::TEAM_SURVIVIOR ) )
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/wpn_select.wav", 0.4, ATTN_NORM);

		if (((pev->button & IN_USE) && (caps & FCAP_CONTINUOUS_USE)) || ((m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE | FCAP_ONOFF_USE))))
		{
			if (caps & FCAP_CONTINUOUS_USE)
				m_afPhysicsFlags |= PFLAG_USING;

			pObject->Use(this, this, USE_SET, 1);
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ((m_afButtonReleased & IN_USE) && (pObject->ObjectCaps() & FCAP_ONOFF_USE)) // BUGBUG This is an "off" use
		{
			pObject->Use(this, this, USE_SET, 0);
		}
	}
	else
	{
		if (m_afButtonPressed & IN_USE)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4, ATTN_NORM);
			GiveAchievement( EAchievements::MMMWAP );
		}
	}
}

void CBasePlayer::Jump()
{
	Vector vecWallCheckDir; // direction we're tracing a line to find a wall when walljumping
	Vector vecAdjustedVelocity;
	Vector vecSpot;
	TraceResult tr;

	if (FBitSet(pev->flags, FL_WATERJUMP))
		return;

	if (pev->waterlevel >= 2)
	{
		return;
	}

	// jump velocity is sqrt( height * gravity * 2)

	// If this isn't the first frame pressing the jump button, break out.
	if (!FBitSet(m_afButtonPressed, IN_JUMP))
		return; // don't pogo stick

	if (!(pev->flags & FL_ONGROUND) || !pev->groundentity)
	{
		return;
	}

	// many features in this function use v_forward, so makevectors now.
	UTIL_MakeVectors(pev->angles);

	// ClearBits(pev->flags, FL_ONGROUND);		// don't stairwalk

	SetAnimation(PLAYER_JUMP);

	if (m_fLongJump && (pev->button & IN_DUCK) && (pev->flDuckTime > 0) && pev->velocity.Length() > 50)
	{
		SetAnimation(PLAYER_SUPERJUMP);
	}

	Vector MyVelocity = pev->velocity;

	// If we are a zombie, decrease our jump speed & max speed
	// Makes bhopping harder.
	if ( pev->team == ZP::TEAM_ZOMBIE )
		pev->fuser4 += 55.0f;
	else
		pev->fuser4 += 38.0f;

	entvars_t *pevGround = VARS(pev->groundentity);
#if defined( HL1 )
	// If you're standing on a conveyor, add it's velocity to yours (for momentum)
	if (pevGround && (pevGround->flags & FL_CONVEYOR))
		MyVelocity += pev->basevelocity;
#endif

	// JoshA: CS behaviour does this for tracktrain + train as well,
	// but let's just do this for func_vehicle to avoid breaking existing content.
	//
	// If you're standing on a moving train... then add the velocity of the train to yours.
	if (pevGround &&
		/*(
			!strcmp( "func_tracktrain", STRING(pevGround->classname)) ||
			!strcmp( "func_train", STRING(pevGround->classname))
		) ||*/
	    !strcmp("func_vehicle", STRING(pevGround->classname)))
	{
		MyVelocity += pevGround->velocity;
	}

	pev->velocity = MyVelocity;
}

// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
void FixPlayerCrouchStuck(edict_t *pPlayer)
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for (int i = 0; i < 18; i++)
	{
		UTIL_TraceHull(pPlayer->v.origin, pPlayer->v.origin, dont_ignore_monsters, head_hull, pPlayer, &trace);
		if (trace.fStartSolid)
			pPlayer->v.origin.z++;
		else
			break;
	}
}

void CBasePlayer::Duck()
{
	if (pev->button & IN_DUCK)
	{
		if (m_IdealActivity != ACT_LEAP)
		{
			SetAnimation(PLAYER_WALK);
		}
	}
}

//
// ID's player as such.
//
int CBasePlayer::Classify(void)
{
	return CLASS_PLAYER;
}

bool CBasePlayer::CanActiveInteruptAnimation()
{
	switch ( m_Activity )
	{
		case ACT_RELOAD:
		case ACT_RELOAD_EMPTY:
	    case ACT_RELOAD_START:
	    case ACT_RELOAD_END:
	    case ACT_PUMP:
	    case ACT_ARM:
	    case ACT_DISARM:
		case ACT_RANGE_ATTACK1:
		case ACT_RANGE_ATTACK2:
	    case ACT_BARRICADE:
	    case ACT_BARRICADE_LONG:
	    case ACT_BARRICADE_NAILGUN:
		case ACT_MELEE_HEAVY_ATTACK_PRE:
		case ACT_MELEE_HEAVY_ATTACK_LOOP:
		case ACT_MELEE_HEAVY_ATTACK_POST:
			return ( m_fSequenceFinished ) ? true : false;
	}
	return true;
}

void CBasePlayer::AddPoints(int score, BOOL bAllowNegativeScore)
{
	// Positive score always adds
	if (score < 0)
	{
		if (!bAllowNegativeScore)
		{
			if (pev->frags < 0) // Can't go more negative
				return;

			if (-score > pev->frags) // Will this go negative?
			{
				score = -pev->frags; // Sum will be 0
			}
		}
	}

	pev->frags += score;

	SendScoreInfo();
}

void CBasePlayer::AddPointsToTeam(int score, BOOL bAllowNegativeScore)
{
	int index = entindex();

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex(i);

		if (pPlayer && i != index)
		{
			if (g_pGameRules->PlayerRelationship(this, pPlayer) == GR_TEAMMATE)
			{
				pPlayer->AddPoints(score, bAllowNegativeScore);
			}
		}
	}
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0;
}

void CBasePlayer::UpdateStatusBar()
{
	int newSBarState[SBAR_END];
	char sbuf0[SBAR_STRING_SIZE];
	char sbuf1[SBAR_STRING_SIZE];

	memset(newSBarState, 0, sizeof(newSBarState));
	UTIL_strcpy(sbuf0, m_SbarString0);
	UTIL_strcpy(sbuf1, m_SbarString1);

	// Find an ID Target
	TraceResult tr;
	CBaseEntity *sourcePlayer = IsObserver() && m_hObserverTarget ? (CBaseEntity *)m_hObserverTarget : this;
	UTIL_MakeVectors(sourcePlayer->pev->v_angle + sourcePlayer->pev->punchangle);
	Vector vecSrc = sourcePlayer->EyePosition();
	Vector vecEnd = vecSrc + (gpGlobals->v_forward * MAX_ID_RANGE);
	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, sourcePlayer->edict(), &tr);

	if (tr.flFraction != 1.0)
	{
		if (!FNullEnt(tr.pHit))
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

			if (pEntity->Classify() == CLASS_PLAYER)
			{
				newSBarState[SBAR_ID_TARGETNAME] = ENTINDEX(pEntity->edict());
				UTIL_strcpy(sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%");

				// allies and medics get to see the targets health
				if (g_pGameRules->PlayerRelationship(this, pEntity) == GR_TEAMMATE)
				{
					newSBarState[SBAR_ID_TARGETHEALTH] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
					newSBarState[SBAR_ID_TARGETARMOR] = pEntity->pev->armorvalue; //No need to get it % based since 100 it's the max.
				}

				m_flStatusBarDisappearDelay = gpGlobals->time + 0.01;
			}
		}
		else if (m_flStatusBarDisappearDelay > gpGlobals->time)
		{
			// hold the values for a short amount of time after viewing the object
			newSBarState[SBAR_ID_TARGETNAME] = m_izSBarState[SBAR_ID_TARGETNAME];
			newSBarState[SBAR_ID_TARGETHEALTH] = m_izSBarState[SBAR_ID_TARGETHEALTH];
			newSBarState[SBAR_ID_TARGETARMOR] = m_izSBarState[SBAR_ID_TARGETARMOR];
		}
	}

	BOOL bForceResend = FALSE;

	if (strcmp(sbuf0, m_SbarString0))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusText, NULL, pev);
		WRITE_BYTE(0);
		WRITE_STRING(sbuf0);
		MESSAGE_END();

		UTIL_strcpy(m_SbarString0, sbuf0);

		// make sure everything's resent
		bForceResend = TRUE;
	}

	if (strcmp(sbuf1, m_SbarString1))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusText, NULL, pev);
		WRITE_BYTE(1);
		WRITE_STRING(sbuf1);
		MESSAGE_END();

		UTIL_strcpy(m_SbarString1, sbuf1);

		// make sure everything's resent
		bForceResend = TRUE;
	}

	// Check values and send if they don't match
	for (int i = 1; i < SBAR_END; i++)
	{
		if (newSBarState[i] != m_izSBarState[i] || bForceResend)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgStatusValue, NULL, pev);
			WRITE_BYTE(i);
			WRITE_SHORT(newSBarState[i]);
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
}

bool CBasePlayer::IsInPanic()
{
	return ( m_flPanicTime - gpGlobals->time > 0 ) ? true : false;
}

bool CBasePlayer::CanPanicSinceLastTime()
{
	return ( m_flLastPanic - gpGlobals->time <= 0 ) ? true : false;
}

void CBasePlayer::WantsToSuicide()
{
	// If the value is -1, stop.
	if ( m_flSuicideTimer == -1 ) return;

	// Already dead? stop.
	if ( !IsAlive() )
	{
		m_flSuicideTimer = -1;
		return;
	}

	// If we reach 0 (or less), stop and kill.
	if ( m_flSuicideTimer - gpGlobals->time <= 0 )
	{
		m_flSuicideTimer = -1;
		// have the player kill themself
		pev->health = 0;
		Killed( pev, GIB_NEVER );
		GiveAchievement( EAchievements::ZMASH );
	}
}

void CBasePlayer::JustKilledWithExplosives()
{
	// If the value is -1, stop.
	if ( m_flResetExplosiveKillNotice == -1 ) return;

	// Already dead? stop.
	if ( !IsAlive() )
	{
		m_flResetExplosiveKillNotice = -1;
		return;
	}

	// If we reach 0 (or less), turn off.
	if ( m_flResetExplosiveKillNotice - gpGlobals->time <= 0 )
	{
		m_flResetExplosiveKillNotice = -1;
		m_bJustKilledWithExplosive = false;
	}
}

void CBasePlayer::UpdateFatigue()
{
	// We only care if we are on the ground
	if (!(pev->flags & FL_ONGROUND) || !pev->groundentity) return;
	// Our fatigue just got increased, reset the timer.
	if ( m_bFatigueUpdated )
	{
		m_flLastFatigue = gpGlobals->time + 1.5f;
		m_bFatigueUpdated = false;
		return;
	}
	if ( m_flLastFatigue - gpGlobals->time > 0 ) return;

	float flValue = pev->fuser4;
	if ( pev->team == ZP::TEAM_ZOMBIE )
		flValue -= 2.25f;
	else
		flValue -= 1.35f;
	if ( flValue <= 0 ) flValue = 0;
	pev->fuser4 = flValue;
}

int CBasePlayer::GetMaxHealth()
{
	if ( pev->team == ZP::TEAM_ZOMBIE ) return ZP::MaxHealth[1];
	if ( m_bPunishLateJoiner ) return 45;
	return ZP::MaxHealth[0];
}

void CBasePlayer::UpdateHealthRegen()
{
	// Dead?
	if ( !IsAlive() ) return;
	// Zombie?
	if ( pev->team != ZP::TEAM_ZOMBIE ) return;
	// We recently got hurt, reset the timer.
	if ( m_bRegenUpdated )
	{
		bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
		m_flRegenTime = bIsInHardcore ? 1.75f : 3.0f;
		m_flLastRegen = gpGlobals->time + m_flRegenTime;
		m_bRegenUpdated = false;
		return;
	}
	if ( m_flLastRegen - gpGlobals->time > 0 ) return;
	m_flLastRegen = gpGlobals->time + m_flRegenTime;
	// Reduce the m_flRegenTime, but not too much.
	float flCurrentReduce = m_flRegenTime;
	flCurrentReduce -= 0.25f;
	m_flRegenTime = clamp( flCurrentReduce, 0.25f, 3.0f );
	float flHP = pev->health;
	float flHPMax = pev->max_health;
	if ( flHP >= flHPMax ) return;
	flHP += 5;
	pev->health = clamp( flHP, 1, flHPMax );
	GiveAchievement( EAchievements::REGEN_10K );
}

void CBasePlayer::DoBloodLossDecal( float flDelay )
{
	if ( m_flBloodLoss - gpGlobals->time > 0 ) return;
	TraceResult tr;
	const Vector start( pev->origin );
	const Vector end( pev->origin + Vector( 0, 0, -128 ) );
	UTIL_TraceLine( start, end, ignore_monsters, pev->owner, &tr );
	UTIL_DecalTrace( &tr, DECAL_BLEEDING1 + RANDOM_LONG(0, 4) );
	m_flBloodLoss = gpGlobals->time + flDelay;

	// Loose blood, but only if we have more than 1hp
	if ( m_bIsBleeding && pev->health > 1 )
	{
		int iHealth = pev->health;
		iHealth -= 1;
		if ( iHealth < 1 ) iHealth = 1;
		pev->health = iHealth;
		DoScreenTint( true );
		pev->punchangle.x = -2;
	}
}

void CBasePlayer::DoScreenTint( bool bDamage )
{
	ClientAPIData_t apiData = ZPGameRules()->GetClientAPI( this );
	bool bDoTint = apiData.DoScreenTint;
	if ( !bDoTint ) return;
	UTIL_ScreenFade( this, bDamage ? Vector(128, 0, 0) : Vector(0, 255, 255), 2, 0.1, 10, FFADE_IN );
}

bool CBasePlayer::GotBandage( bool bGiveHealth )
{
	int iHealthGive = bGiveHealth ? TakeHealth( 10, DMG_GENERIC ) : 0;
	bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	if ( bIsInHardcore && iHealthGive > 0 )
		GiveAchievement( HC_STOP_THE_BLEEDIN );

	m_bIsBleeding = false;
	m_bGotBandage = true;
	return bGiveHealth ? iHealthGive : true;
}

bool CBasePlayer::GotPainKiller()
{
	if ( pev->health >= pev->max_health ) return false;
	m_iPillsTaken++;
	if ( m_iPillsTaken == 3 )
		GiveAchievement( INTERFECTUM );
	m_bGotPainkiller = true;
	m_flPainPillHealthDelay = gpGlobals->time + 2.0f;
	m_iPillAmount = 0;
	return TakeHealth( 5, DMG_GENERIC );
}

void CBasePlayer::DoBloodLoss()
{
	if ( pev->team != ZP::TEAM_SURVIVIOR ) return;
	if ( !IsAlive() ) return;

	const int iHealth = pev->health;
	float flDelay = 0;

	if ( m_bIsBleeding )
		flDelay = 3.5f;
	else
	{
		// A lot of blood
		if ( iHealth <= 10 ) flDelay = 0.5f;
		else if ( iHealth <= 15 ) flDelay = 0.8f;
		else if ( iHealth <= 20 ) flDelay = 1.0f;
		else if ( iHealth <= 25 ) flDelay = 1.3f;
		else if ( iHealth <= 30 ) flDelay = 1.6f;
		else if ( iHealth <= 35 ) flDelay = 2.0f;
		/*
		else if ( iHealth <= 40 ) flDelay = 2.3f;
		else if ( iHealth <= 45 ) flDelay = 2.6f;
		else if ( iHealth <= 49 ) flDelay = 3.0f;
		*/
		else
		{
			m_bGotBandage = false;
			return;
		}
	}

	if ( m_bGotBandage ) return;

	// Aim straight down, and do some blood
	DoBloodLossDecal( flDelay );

	// Got painkiller?
	if ( m_bGotPainkiller ) return;

	if ( m_flImHurtDelay - gpGlobals->time > 0 ) return;
	m_flImHurtDelay = gpGlobals->time + 60.0f;
	DoVocalize( PlayerVocalizeType::VOCALIZE_AUTO_HURT, true );
}

void CBasePlayer::GiveAchievement( EAchievements eAchivement )
{
	// Only if we're connected
	if ( !IsConnected() ) return;
	if ( !ZPGameRules() ) return;
	if ( ZPGameRules()->IsTestModeActive() )
	{
		DialogAchievementData data = GetAchievementByID( eAchivement );
		Msg( "[DEBUG] GiveAchievement: %i [%s]\n", data.GetAchievementID(), data.GetAchievementName() );
	}
#ifndef _DEBUG
	if ( ZPGameRules()->WasCheatsOnThisSession() ) return;
#endif
	MESSAGE_BEGIN( MSG_ONE, gmsgAchievement, NULL, pev );
	WRITE_SHORT( eAchivement );
	MESSAGE_END();
}

void CBasePlayer::NotifyOfEarnedAchivement( int eAchivement )
{
	// Only if we're connected
	if ( !IsConnected() ) return;
	MESSAGE_BEGIN( MSG_ALL, gmsgAchEarned );
	WRITE_SHORT( entindex() );
	WRITE_SHORT( eAchivement );
	MESSAGE_END();
}

void CBasePlayer::GiveAchievementsFromKill( CBasePlayer *pKiller, const char *szWeapon )
{
	if ( !pKiller || !szWeapon ) return;

	// Make sure we have a valid gamemode, since some achievements require certain gamemodes.
	IGameModeBase *pGameMode = ZP::GetCurrentGameMode();
	if ( !pGameMode ) return;

	// If szWeapon is "swipe", make sure the victim was a survivor while the killer is a zombie.
	if ( !strcmp( szWeapon, "swipe" ) )
	{
		if ( pev->team != ZP::TEAM_SURVIVIOR && pKiller->pev->team != ZP::TEAM_ZOMBIE )
			return;
	}

	// Increase by one for each kill
	pKiller->m_iWeaponKillCount++;
	pKiller->m_iKillsThisLife[0]++;

	bool bNeedHeadshot = ( ( m_iDeathFlags & PLR_DEATH_FLAG_HEADSHOT ) != 0 );
	std::vector<KillAchievementData_t> vList;
	GetKillAchievementsByWeapon( szWeapon, bNeedHeadshot, vList );

	if ( bNeedHeadshot )
		pKiller->m_iKillsThisLife[1]++;

	if ( bNeedHeadshot
		&& pGameMode->GetGameModeType() == ZP::GAMEMODE_HARDCORE
		&& m_iKillsThisLife[1] == 6 )
		pKiller->GiveAchievement( HC_HEADSHOTFEST );

	for ( size_t i = 0; i < vList.size(); i++ )
	{
		KillAchievementData_t &data = vList[ i ];

		// If special conditions are required, check those.
		bool bConditionsMet = true;
		if ( data.ID == INLINEP2 && pKiller->m_iWeaponKillCount <= 5 )
			bConditionsMet = false;
		else if ( data.ID == RABBITBEAST && pKiller->m_iWeaponKillCount <= 2 )
			bConditionsMet = false;
		else if ( data.ID == DBARREL_2KILL && pKiller->m_iWeaponKillCount <= 1 )
			bConditionsMet = false;
		else if ( data.ID == CUTYOUDOWN && pKiller->m_iWeaponKillCount <= 3 )
			bConditionsMet = false;
		else if ( data.ID == HC_SNACKTIME && pGameMode->GetGameModeType() != ZP::GAMEMODE_HARDCORE )
			bConditionsMet = false;

		// If we don't meet the conditions, skip this achievement.
		if ( !bConditionsMet ) continue;

		// Give the achievement for this kill, if we meet the requirements.
		pKiller->GiveAchievement( data.ID );

		// A special case for the "You will die with me" achievement, which can be given if we manage to get an explosive kill while being killed ourselves within a short time after.
		if ( ( data.ID == KILLS_SATCHEL || data.ID == KILLS_TNT ) )
		{
			if ( pKiller->m_bJustKilledWithExplosive )
				pKiller->GiveAchievement( YOU_WILL_DIE_WITH_ME );
			else
			{
				pKiller->m_bJustKilledWithExplosive = true;
				pKiller->m_flResetExplosiveKillNotice = gpGlobals->time + 3.0f;
			}
		}

		// If hardcore mode, and we managed to get a melee kill while having a backpack.
		if ( pGameMode
			&& pGameMode->GetGameModeType() == ZP::GAMEMODE_HARDCORE
			&& pKiller->HasBackpack()
			&& data.NeedMelee )
			pKiller->GiveAchievement( HC_OVERWEIGHTKILLER );

		// Another special case for non-melee kills. If we have 0 bullets left in our clip after getting this kill, give the "Last Bullet" achievement.
		if ( data.NeedMelee ) continue;
		CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>( pKiller->m_pActiveItem );
		const char *pWeaponName = pWeapon ? pWeapon->pszName() : nullptr;
		if ( pWeaponName && strncmp( pWeaponName, "weapon_", 7 ) == 0 )
			pWeaponName += 7;
		if ( pWeapon
		    && pWeaponName
			&& FStrEq( pWeaponName, szWeapon )
			&& pWeapon->m_iClip == 0
			&& pWeapon->iMaxClip() > 0 )
			pKiller->GiveAchievement( LAST_BULLET );
	}

	// Check for the "Jack of all trades" achievement, which is given for getting a kill with every weapon. We check this after giving the kill achievements, since some of those achievements require a certain number of kills with a weapon, and we want to make sure we give those first before checking for Jack of all trades.
	pKiller->GiveAchievement( JACKOFTRADES );

	// If the victim was in panic, give the "Scream for me" achievement to the killer. If the killer was in panic and the victim was a zombie, give the "Panic rush" achievement to the killer.
	if ( IsInPanic() )
		pKiller->GiveAchievement( SCREAM4ME );
	else if ( pKiller->IsInPanic() && pev->team == ZP::TEAM_ZOMBIE )
		pKiller->GiveAchievement( PANICRUSH );

	// We just gibbed, give the "It's a massacre" achievement.
	if ( ( m_iDeathFlags & PLR_DEATH_FLAG_GIBBED ) != 0 )
		pKiller->GiveAchievement( ITS_A_MASSACRE );
}

void CBasePlayer::SetTheCorrectPlayerModel()
{
	int iTeam = pev->team;
	const char *szModel = nullptr;
	bool bIsZombie = (iTeam == ZP::TEAM_ZOMBIE) ? true : false;

	PlayerCharacter iPrefferedChar = PlayerCharacter::ANY;
	ClientAPIData_t apiData = ZPGameRules()->GetClientAPI( this );
	iPrefferedChar = GetCharacter( apiData.Character.c_str() );

	if ( iPrefferedChar == -1 )
		m_iCharacter = (PlayerCharacter)RANDOM_LONG( PlayerCharacter::SURVIVOR1, PlayerCharacter::MAX_SURVIVORS );
	else
		m_iCharacter = iPrefferedChar;

	PlayerCharacterType PlayerType = GetPlayerCharacterType();
	szModel = bIsZombie ? PlayerType.Zombie : PlayerType.Survivor;

	g_engfuncs.pfnSetClientKeyValue( entindex(), g_engfuncs.pfnGetInfoKeyBuffer( edict() ), "model", szModel );

	szModel = UTIL_VarArgs( "models/player/%s/%s.mdl", szModel, szModel );

	SET_MODEL(ENT(pev), szModel );
	pev->model = MODEL_INDEX( szModel );

	SetBodygroup( BGROUP_HEAD, BGROUP_SUB_DEFAULT );
	SetBodygroup( BGROUP_EYEGLOW, BGROUP_SUB_DEFAULT );

	if ( iTeam == ZP::TEAM_SURVIVIOR )
		pev->maxspeed = ZP::MaxSpeeds[0];
	else
		pev->maxspeed = ZP::MaxSpeeds[1];

	// Player models can have up to 5 random skins.
	// Mostly used by the new "undead" model.
	pev->skin = RANDOM_LONG( 0, 4 );
}

PlayerCharacter CBasePlayer::GetCharacter( const char *szType )
{
	for ( int i = 0; i < ARRAYSIZE( g_CharacterTypes ); i++ )
	{
		if (FStrEq(szType, g_CharacterTypes[i].Name))
			return g_CharacterTypes[i].Type;
	}
	return PlayerCharacter::ANY;
}

PlayerCharacterType CBasePlayer::GetPlayerCharacterType()
{
	for ( int i = 0; i < ARRAYSIZE( g_CharacterTypes ); i++ )
	{
		if ( m_iCharacter == g_CharacterTypes[i].Type )
			return g_CharacterTypes[i];
	}
	return g_CharacterTypes[0];
}

void CBasePlayer::InAchievementTrigger( string_t iszAchievementID )
{
	if ( !IsAlive() ) return;
	const char *szAchievementID = STRING( iszAchievementID );
	if ( !szAchievementID || !szAchievementID[0] ) return;
	if ( FStrEq( szAchievementID, "DANCE_FLOOR" ) )
	{
		// Did we manage to kill a zombie while on the dance floor?
		if ( m_flLastKillCheck - gpGlobals->time > 0 )
			GiveAchievement( EAchievements::DANCE_FLOOR );
	}
	else if ( FStrEq( szAchievementID, "HOUSEOFHORRORS" ) )
	{
		// Give the achievement to all players who participated in finding them.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsConnected() && pPlayer->HasParticipated( EAchievements::HOUSEOFHORRORS ) )
				pPlayer->GiveAchievement( EAchievements::HOUSEOFHORRORS );
		}
	}
}

void CBasePlayer::SelectWeaponFromSlot( int iSlot )
{
	// Selects the weapon from the slot they were assigned
	CBasePlayerWeapon *pWeapon = GetWeaponFromSlot( iSlot );
	if ( !pWeapon ) return;
	SelectWeapon( pWeapon );
}

void CBasePlayer::SelectNextSlot()
{
	CWeaponBase *pBaseWeapon = dynamic_cast<CWeaponBase *>( m_pActiveItem );
	int iCurrentSlot = pBaseWeapon ? pBaseWeapon->m_iAssignedSlotPosition : 0;
	int iStartingSlot = -1;
	// Select the next available slot, wrapping around to the start if necessary.
	// We break the moment we find a weapon in a slot, even if it's our current weapon.
	while ( true )
	{
		if ( iStartingSlot == -1 )
			iStartingSlot = iCurrentSlot;
		else
		{
			if ( iCurrentSlot == iStartingSlot )
				break; // We've looped all the way around, no other weapons found.
		}
		iCurrentSlot++;
		bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
		int iMaxSlots = bIsInHardcore ? MAX_WEAPON_SLOTS_HARDCORE : MAX_WEAPON_SLOTS;
		if ( bIsInHardcore && HasBackpack() )
			iMaxSlots += BACKPACK_EXTRA_SLOTS;
		if ( iCurrentSlot >= iMaxSlots )
			iCurrentSlot = 0;
		SelectWeaponFromSlot( iCurrentSlot );
	}
}

void CBasePlayer::SelectPreviousSlot()
{
	CWeaponBase *pBaseWeapon = dynamic_cast<CWeaponBase *>( m_pActiveItem );
	int iCurrentSlot = pBaseWeapon ? pBaseWeapon->m_iAssignedSlotPosition : 0;
	int iStartingSlot = -1;
	// Select the previous available slot, wrapping around to the end if necessary.
	// We break the moment we find a weapon in a slot, even if it's our current weapon.
	while ( true )
	{
		if ( iStartingSlot == -1 )
			iStartingSlot = iCurrentSlot;
		else
		{
			if ( iCurrentSlot == iStartingSlot )
				break; // We've looped all the way around, no other weapons found.
		}
		iCurrentSlot--;
		bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
		int iMaxSlots = bIsInHardcore ? MAX_WEAPON_SLOTS_HARDCORE : MAX_WEAPON_SLOTS;
		if ( bIsInHardcore && HasBackpack() )
			iMaxSlots += BACKPACK_EXTRA_SLOTS;
		if ( iCurrentSlot < 0 )
			iCurrentSlot = iMaxSlots - 1;
		SelectWeaponFromSlot( iCurrentSlot );
	}
}

CBasePlayerWeapon *CBasePlayer::GetWeaponFromSlot( int iSlot )
{
	// Selects the weapon from the slot they were assigned
	CBasePlayerWeapon *pWeapon = nullptr;
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			pWeapon = (CBasePlayerWeapon *)m_rgpPlayerItems[i];
			while (pWeapon)
			{
				if ( pWeapon->m_iAssignedSlotPosition == iSlot )
				{
					// Select this weapon
					return pWeapon;
				}
				pWeapon = (CBasePlayerWeapon *)pWeapon->m_pNext;
			}
		}
	}
	return nullptr;
}

bool CBasePlayer::HasAvailableWeaponSlots( bool bIsDoubleSlot )
{
	bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	int iMaxSlots = bIsInHardcore ? MAX_WEAPON_SLOTS_HARDCORE : MAX_WEAPON_SLOTS;
	if ( bIsInHardcore && HasBackpack() )
		iMaxSlots += BACKPACK_EXTRA_SLOTS;
	bool bHasAvailableSlot = false;
	// Now check how many we have used
	for ( int i = 0; i < iMaxSlots; i++ )
	{
		if ( bIsDoubleSlot )
		{
			if ( i+1 == iMaxSlots ) break; // No space for double slot here
			if ( !WeaponSlots[i] && !WeaponSlots[i+1] )
			{
				bHasAvailableSlot = true;
			    break;
			}
		}
		else
			if ( !WeaponSlots[i] )
			{
				bHasAvailableSlot = true;
			    break;
			}
	}
	return bHasAvailableSlot;
}

int CBasePlayer::GetBestSlotPosition( bool bIsDoubleSlot )
{
	bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	int iMaxSlots = bIsInHardcore ? MAX_WEAPON_SLOTS_HARDCORE : MAX_WEAPON_SLOTS;
	if ( bIsInHardcore && HasBackpack() )
		iMaxSlots += BACKPACK_EXTRA_SLOTS;
	int iAvailableSlot = 0;
	for ( int i = 0; i < iMaxSlots; i++ )
	{
		if ( bIsDoubleSlot )
		{
			if ( !WeaponSlots[i] && !WeaponSlots[i+1] )
			{
				iAvailableSlot = i;
				break;
			}
		}
		else
		{
			if ( !WeaponSlots[i] )
			{
				iAvailableSlot = i;
				break;
			}
		}
	}
	return iAvailableSlot;
}

bool CBasePlayer::CanSelectNewWeapon( bool bPrintMsg )
{
	bool bCanSwitch = (m_flNextWeaponSwitch - gpGlobals->time <= 0) ? true : false;
	if ( !bCanSwitch && bPrintMsg )
		UTIL_SayText( CHAT_FILTER_SERVERMSG, "#ZP_Deny_Switching", this );
	return bCanSwitch;
}

bool CBasePlayer::CanDropWeapon( bool bPrintMsg )
{
	bool bCanSwitch = (m_flLastWeaponDrop - gpGlobals->time <= 0) ? true : false;
	if ( !bCanSwitch && bPrintMsg )
		UTIL_SayText( CHAT_FILTER_SERVERMSG, "#ZP_DenyDrop_Sorting", this );
	return bCanSwitch;
}

void CBasePlayer::SelectWeapon( CBasePlayerWeapon *pWeapon )
{
	if ( pWeapon == m_pActiveItem ) return;

	CWeaponBase *pBaseWeapon = dynamic_cast<CWeaponBase *>( m_pActiveItem );
	if ( !pBaseWeapon )
	{
		m_pLastItem = nullptr;
		// We have no active weapon, so we can select this one instantly.
		SelectNewActiveWeapon( pWeapon );
		return;
	}
	if ( pBaseWeapon->IsHolstering() ) return;
	if ( !pBaseWeapon->CanHolster() ) return;

	// Check if this is a CWeaponBaseSingleAction, if so,
	// make sure we can't switch if we are pumping the weapon.
	CWeaponBaseSingleAction *pSingleAction = dynamic_cast<CWeaponBaseSingleAction *>( pBaseWeapon );
	if ( pSingleAction )
	{
		// If we are pumping, don't allow switching.
		if ( pSingleAction->PumpIsRequired() ) return;
	}

	ResetAutoaim();

	// Wait a little longer before the player can switch weapons again
	m_flNextWeaponSwitch = gpGlobals->time + 1.0f;

	// FIX, this needs to queue them up and delay
	if ( pBaseWeapon )
	{
		// If throwable and active, get rid of it.
		ThrowableDropState throwablestate = IsThrowableAndActive( pBaseWeapon, false );
		if ( throwablestate == DELETE_ITEM_AND_ACTIVE )
		{
			m_pActiveItem = nullptr;
			pev->viewmodel = 0;
			pev->weaponmodel = 0;

			// Remove from the player
			WeaponSlotSet( pBaseWeapon, false );
			RemovePlayerItem( pBaseWeapon );
			UTIL_Remove( pBaseWeapon );

			m_pLastItem = nullptr;
			SelectNewActiveWeapon( pWeapon );
			return;
		}
		if ( pBaseWeapon->IsThrowable() )
			pBaseWeapon->DeactivateThrow();
		pBaseWeapon->BeginHolster( pWeapon );
		m_flLastWeaponDrop = gpGlobals->time + pBaseWeapon->GetHolsterTime() + 0.5f;
	}
	RefuseWeaponAudioCalls( 3.0f );
	m_pLastItem = m_pActiveItem;
}

void CBasePlayer::SelectNewActiveWeapon( CBasePlayerWeapon *pWeapon )
{
	RefuseWeaponAudioCalls( 0.1f );
	m_pActiveItem = pWeapon;
	if ( m_pActiveItem )
	{
		if ( pev->team == ZP::TEAM_SURVIVIOR )
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/wpn_select.wav", 1, ATTN_NORM);
		m_pActiveItem->UpdateItemInfo();
		m_pActiveItem->DoDeployAnimation();
		m_flLastWeaponDrop = gpGlobals->time + 0.5f;
	}
}

void CBasePlayer::WeaponSlotSet(CBasePlayerWeapon *pWeapon, bool bState)
{
	WeaponSlots[pWeapon->m_iAssignedSlotPosition] = bState;
	if ( pWeapon->bDoubleSlot() )
		WeaponSlots[pWeapon->m_iAssignedSlotPosition + 1] = bState;
}

void CBasePlayer::DropEverything( bool bDontDropPrimary )
{
	// Remember our active weapon so we can switch back to it.
	CBasePlayerWeapon *pActive = (CBasePlayerWeapon *)m_pActiveItem;
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)m_rgpPlayerItems[i];
			while (pWeapon)
			{
				CBasePlayerWeapon *pNext = (CBasePlayerWeapon *)pWeapon->m_pNext;
				bool bCanDrop = true;
				if ( bDontDropPrimary && ( pActive && pWeapon->GetWeaponID() == pActive->GetWeaponID() ) )
					bCanDrop = false;
				if ( bCanDrop )
					DropWeapon( pWeapon, false, true );
				pWeapon = pNext;
			}
		}
	}

	// Spawn a backpack if we had one.
	if ( HasBackpack() && !bDontDropPrimary )
		CBaseEntity::Create( "item_backpack", pev->origin, pev->angles, nullptr );

	// Now drop all ammo types we no longer need.
	DropUnuseableAmmo();
}

void CBasePlayer::DropWeapon( CBasePlayerWeapon *pWeapon, bool bAutoSwitch, bool pukevel )
{
	if ( ZP::GetCurrentRoundState() < ZP::RoundState::RoundState_RoundHasBegun ) return;
	if ( !pWeapon ) return;
	if ( !pWeapon->CanHolster() ) return;
	m_flLastWeaponDrop = gpGlobals->time + 0.5f;

	string_t weaponname = ALLOC_STRING( pWeapon->GetData().Classname ); // Make a copy of the classname;
	if ( FStringNull( weaponname ) )
	{
		Msg( "Failed to drop a weapon with NULL classname!\n" );
		return;
	}

	// You ain't allowed to drop shit.
	if ( pWeapon->IsThrowable() && pWeapon->m_iClip == 0 ) return;

	// Remove from the player
	WeaponSlotSet( pWeapon, false );

	if ( bAutoSwitch )
	{
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
		m_pActiveItem = NULL;
	}

	// Check if this is an explosive, and if armed, decrease the value if we have ammo for it.
	// If not, delete and explode.
	ThrowableDropState throwablestate = IsThrowableAndActive( pWeapon, true );

	int nClipWeHad = pWeapon->m_iClip;
	int nDefAmmo = pWeapon->m_iDefaultAmmo;
	// If revolver, then check if we unloaded it or not.
	bool bHasUnloadedRevolver = false;
	CWeaponSideArmRevolver *pRevolver = dynamic_cast< CWeaponSideArmRevolver* >( pWeapon );
	if ( pRevolver )
		bHasUnloadedRevolver = pRevolver->HasBeenUnloaded();

	if ( bAutoSwitch )
		g_pGameRules->GetNextBestWeapon( this, pWeapon );
	RemovePlayerItem( pWeapon );
	UTIL_Remove( pWeapon );

	// Don't create a new weapon, we go boom!
	if ( throwablestate == DELETE_ITEM_AND_ACTIVE ) return;

	// Make sure the v_forward is from us!
	UTIL_MakeVectors(pev->angles);
	Vector vecDir = gpGlobals->v_forward;

	CWeaponBase *pNewWeapon = (CWeaponBase *)CBaseEntity::Create((char *)STRING(weaponname), pev->origin + vecDir, pev->angles, nullptr);
	if ( !pNewWeapon )
	{
		Msg( "Failed to create weapon %s!\n", STRING(weaponname) );
		return;
	}

	//pNewWeapon->DisallowPickupFor( 2.5f );
	pNewWeapon->m_iDefaultAmmo = nDefAmmo;
	pNewWeapon->m_iClip = nClipWeHad;
	pNewWeapon->pev->playerclass = (nClipWeHad == 0) ? 1 : 0; // if iuser1 is 1, it's empty
	pNewWeapon->pev->angles.x = 0;
	pNewWeapon->pev->angles.z = 0;
	pNewWeapon->m_bHasDropped = true;
	pNewWeapon->m_flNextBounceSound = 0.0f; // Allow impact sound on player drop
	if ( pukevel )
		pNewWeapon->pev->velocity = vecDir * 250 + RandomVector( -200, 200 );
	else
		pNewWeapon->pev->velocity = vecDir * 300;
	pNewWeapon->CheckIfStuckInWorld();

	// Check revolver unloaded state
	pRevolver = dynamic_cast< CWeaponSideArmRevolver* >( pNewWeapon );
	if ( pRevolver )
		pRevolver->m_bHasUnloaded = bHasUnloadedRevolver;

	// Play drop sound
	EMIT_SOUND(ENT(pNewWeapon->pev), CHAN_VOICE, "player/pl_drop_weapon.wav", 1, ATTN_NORM);
}

void CBasePlayer::SetBackpackState( bool bState )
{
	SetWeaponOwn( WEAPON_BACKPACK, bState );
}

#define CLIMB_SHAKE_FREQUENCY 22 // how many frames in between screen shakes when climbing
#define MAX_CLIMB_SPEED       200 // fastest vertical climbing speed possible
#define CLIMB_SPEED_DEC       15 // climbing deceleration rate
#define CLIMB_PUNCH_X         -7 // how far to 'punch' client X axis when climbing
#define CLIMB_PUNCH_Z         7 // how far to 'punch' client Z axis when climbing

void CBasePlayer::PreThink(void)
{
	int buttonsChanged = (m_afButtonLast ^ pev->button); // These buttons have changed this frame

	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed = buttonsChanged & pev->button; // The changed ones still down are "pressed"
	m_afButtonReleased = buttonsChanged & (~pev->button); // The ones not down are "released"

	g_pGameRules->PlayerThink(this);

	if (g_fGameOver)
		return; // intermission or finale

	UTIL_MakeVectors(pev->v_angle); // is this still used?

	ItemPreFrame();
	WaterMove();

	if (g_pGameRules && g_pGameRules->FAllowFlashlight())
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;

	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();

	CheckTimeBasedDamage();

	CheckSuitUpdate();

	// Observer Button Handling
	if (IsObserver())
	{
		Observer_HandleButtons();
		Observer_CheckTarget();

		pev->impulse = 0;
		return;
	}

	// Welcome cam buttons handling
	if (m_bInWelcomeCam)
	{
		if (m_flNextAttack > 0)
			return;

		if (m_bIsBot)
		{
			StopWelcomeCam();
		}
		else if (m_afButtonPressed & IN_ATTACK)
		{
			StopWelcomeCam();
		}
		return;
	}

	if (pev->deadflag >= DEAD_DYING)
	{
		PlayerDeathThink();
		return;
	}

	// So the correct flags get sent to client asap.
	//
	if (m_afPhysicsFlags & PFLAG_ONTRAIN)
		pev->flags |= FL_ONTRAIN;
	else
		pev->flags &= ~FL_ONTRAIN;

	// Train speed control
	if (m_afPhysicsFlags & PFLAG_ONTRAIN)
	{
		CBaseEntity *pTrain = CBaseEntity::Instance(pev->groundentity);
		float vel;

		if (!pTrain)
		{
			TraceResult trainTrace;
			// Maybe this is on the other side of a level transition
			UTIL_TraceLine(pev->origin, pev->origin + Vector(0, 0, -38), ignore_monsters, ENT(pev), &trainTrace);

			// HACKHACK - Just look for the func_tracktrain classname
			if (trainTrace.flFraction != 1.0 && trainTrace.pHit)
				pTrain = CBaseEntity::Instance(trainTrace.pHit);

			if (!pTrain || !(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) || !pTrain->OnControls(pev))
			{
				//ALERT( at_error, "In train mode with no train!\n" );
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW | TRAIN_OFF;
				if (pTrain->Classify() == CLASS_VEHICLE)
					static_cast<CFuncVehicle *>(pTrain)->m_pDriver = NULL;
				return;
			}
		}
		else if (
			!FBitSet(pev->flags, FL_ONGROUND) ||
			FBitSet(pTrain->pev->spawnflags, SF_TRACKTRAIN_NOCONTROL) ||
			((pev->button & (IN_MOVELEFT | IN_MOVERIGHT)) && pTrain->Classify() != CLASS_VEHICLE))
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW | TRAIN_OFF;
			return;
		}

		pev->velocity = g_vecZero;
		vel = 0;

		if (pTrain->Classify() == CLASS_VEHICLE)
		{
			if (pev->button & IN_FORWARD)
			{
				vel = 1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_BACK)
			{
				vel = -1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_MOVELEFT)
			{
				vel = 20;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			if (pev->button & IN_MOVERIGHT)
			{
				vel = 30;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
		}
		else
		{
			if (m_afButtonPressed & IN_FORWARD)
			{
				vel = 1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
			else if (m_afButtonPressed & IN_BACK)
			{
				vel = -1;
				pTrain->Use(this, this, USE_SET, (float)vel);
			}
		}

		if (vel)
		{
			m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
			m_iTrain |= TRAIN_ACTIVE | TRAIN_NEW;
		}
	}
	else if (m_iTrain & TRAIN_ACTIVE)
		m_iTrain = TRAIN_NEW; // turn off train

	if (pev->button & IN_JUMP)
	{
		// If on a ladder, jump off the ladder
		// else Jump
		Jump();
	}

	// If trying to duck, already ducked, or in the process of ducking
	if ((pev->button & IN_DUCK) || FBitSet(pev->flags, FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING))
		Duck();

	if (!FBitSet(pev->flags, FL_ONGROUND))
	{
		m_flFallVelocity = -pev->velocity.z;
	}

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;

	if (m_afPhysicsFlags & PFLAG_ONBARNACLE)
	{
		pev->velocity = g_vecZero;
	}
}
/* Time based Damage works as follows: 
	1) There are several types of timebased damage:

		#define DMG_PARALYZE		(1 << 14)	// slows affected creature down
		#define DMG_NERVEGAS		(1 << 15)	// nerve toxins, very bad
		#define DMG_POISON			(1 << 16)	// blood poisioning
		#define DMG_RADIATION		(1 << 17)	// radiation exposure
		#define DMG_DROWNRECOVER	(1 << 18)	// drown recovery
		#define DMG_ACID			(1 << 19)	// toxic chemicals or acid burns
		#define DMG_SLOWBURN		(1 << 20)	// in an oven
		#define DMG_SLOWFREEZE		(1 << 21)	// in a subzero freezer

	2) A new hit inflicting tbd restarts the tbd counter - each monster has an 8bit counter,
		per damage type. The counter is decremented every second, so the maximum time 
		an effect will last is 255/60 = 4.25 minutes.  Of course, staying within the radius
		of a damaging effect like fire, nervegas, radiation will continually reset the counter to max.

	3) Every second that a tbd counter is running, the player takes damage.  The damage
		is determined by the type of tdb.  
			Paralyze		- 1/2 movement rate, 30 second duration.
			Nervegas		- 5 points per second, 16 second duration = 80 points max dose.
			Poison			- 2 points per second, 25 second duration = 50 points max dose.
			Radiation		- 1 point per second, 50 second duration = 50 points max dose.
			Drown			- 5 points per second, 2 second duration.
			Acid/Chemical	- 5 points per second, 10 second duration = 50 points max.
			Burn			- 10 points per second, 2 second duration.
			Freeze			- 3 points per second, 10 second duration = 30 points max.

	4) Certain actions or countermeasures counteract the damaging effects of tbds:

		Armor/Heater/Cooler - Chemical(acid),burn, freeze all do damage to armor power, then to body
							- recharged by suit recharger
		Air In Lungs		- drowning damage is done to air in lungs first, then to body
							- recharged by poking head out of water
							- 10 seconds if swiming fast
		Air In SCUBA		- drowning damage is done to air in tanks first, then to body
							- 2 minutes in tanks. Need new tank once empty.
		Radiation Syringe	- Each syringe full provides protection vs one radiation dosage
		Antitoxin Syringe	- Each syringe full provides protection vs one poisoning (nervegas or poison).
		Health kit			- Immediate stop to acid/chemical, fire or freeze damage.
		Radiation Shower	- Immediate stop to radiation damage, acid/chemical or fire damage.
		
	
*/

// If player is taking time based damage, continue doing damage to player -
// this simulates the effect of being poisoned, gassed, dosed with radiation etc -
// anything that continues to do damage even after the initial contact stops.
// Update all time based damage counters, and shut off any that are done.

// The m_bitsDamageType bit MUST be set if any damage is to be taken.
// This routine will detect the initial on value of the m_bitsDamageType
// and init the appropriate counter.  Only processes damage every second.

//#define PARALYZE_DURATION	30		// number of 2 second intervals to take damage
//#define PARALYZE_DAMAGE		0.0		// damage to take each 2 second interval

//#define NERVEGAS_DURATION	16
//#define NERVEGAS_DAMAGE		5.0

//#define POISON_DURATION		25
//#define POISON_DAMAGE		2.0

//#define RADIATION_DURATION	50
//#define RADIATION_DAMAGE	1.0

//#define ACID_DURATION		10
//#define ACID_DAMAGE			5.0

//#define SLOWBURN_DURATION	2
//#define SLOWBURN_DAMAGE		1.0

//#define SLOWFREEZE_DURATION	1.0
//#define SLOWFREEZE_DAMAGE	3.0

/* */

void CBasePlayer::CheckTimeBasedDamage()
{
	int i;
	BYTE bDuration = 0;

	static float gtbdPrev = 0.0;

	if (!(m_bitsDamageType & DMG_TIMEBASED))
		return;

	// only check for time based damage approx. every 2 seconds
	if (fabs(gpGlobals->time - m_tbdPrev) < 2.0)
		return;

	m_tbdPrev = gpGlobals->time;

	for (i = 0; i < CDMG_TIMEBASED; i++)
	{
		// make sure bit is set for damage type
		if (m_bitsDamageType & (DMG_PARALYZE << i))
		{
			switch (i)
			{
			case itbd_Paralyze:
				// UNDONE - flag movement as half-speed
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
				//				TakeDamage(pev, pev, NERVEGAS_DAMAGE, DMG_GENERIC);
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_Poison:
				// Only survivor die from poison
				if ( pev->team == ZP::TEAM_SURVIVIOR )
					TakeDamage(pev, pev, POISON_DAMAGE, DMG_GENERIC);
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
				//				TakeDamage(pev, pev, RADIATION_DAMAGE, DMG_GENERIC);
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// NOTE: this hack is actually used to RESTORE health
				// after the player has been drowning and finally takes a breath
				if (m_idrowndmg > m_idrownrestored)
				{
					int idif = min(m_idrowndmg - m_idrownrestored, 10);
					if ( pev->team == ZP::TEAM_SURVIVIOR )
						TakeHealth(idif, DMG_GENERIC);
					m_idrownrestored += idif;
				}
				bDuration = 4; // get up to 5*10 = 50 points back
				break;
			case itbd_Acid:
				//				TakeDamage(pev, pev, ACID_DAMAGE, DMG_GENERIC);
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
				//				TakeDamage(pev, pev, SLOWBURN_DAMAGE, DMG_GENERIC);
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
				//				TakeDamage(pev, pev, SLOWFREEZE_DAMAGE, DMG_GENERIC);
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if (m_rgbTimeBasedDamage[i])
			{
				// use up an antitoxin on poison or nervegas after a few seconds of damage
				if (((i == itbd_NerveGas) && (m_rgbTimeBasedDamage[i] < NERVEGAS_DURATION)) || ((i == itbd_Poison) && (m_rgbTimeBasedDamage[i] < POISON_DURATION)))
				{
					if (m_rgItems[ITEM_ANTIDOTE])
					{
						m_rgbTimeBasedDamage[i] = 0;
						m_rgItems[ITEM_ANTIDOTE]--;
						SetSuitUpdate("!HEV_HEAL4", FALSE, SUIT_REPEAT_OK);
					}
				}

				// decrement damage duration, detect when done.
				if (!m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0)
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~(DMG_PARALYZE << i);
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

/*
THE POWER SUIT

The Suit provides 3 main functions: Protection, Notification and Augmentation. 
Some functions are automatic, some require power. 
The player gets the suit shortly after getting off the train in C1A0 and it stays
with him for the entire game.

Protection

	Heat/Cold
		When the player enters a hot/cold area, the heating/cooling indicator on the suit 
		will come on and the battery will drain while the player stays in the area. 
		After the battery is dead, the player starts to take damage. 
		This feature is built into the suit and is automatically engaged.
	Radiation Syringe
		This will cause the player to be immune from the effects of radiation for N seconds. Single use item.
	Anti-Toxin Syringe
		This will cure the player from being poisoned. Single use item.
	Health
		Small (1st aid kits, food, etc.)
		Large (boxes on walls)
	Armor
		The armor works using energy to create a protective field that deflects a
		percentage of damage projectile and explosive attacks. After the armor has been deployed,
		it will attempt to recharge itself to full capacity with the energy reserves from the battery.
		It takes the armor N seconds to fully charge. 

Notification (via the HUD)

x	Health
x	Ammo  
x	Automatic Health Care
		Notifies the player when automatic healing has been engaged. 
x	Geiger counter
		Classic Geiger counter sound and status bar at top of HUD 
		alerts player to dangerous levels of radiation. This is not visible when radiation levels are normal.
x	Poison
	Armor
		Displays the current level of armor. 

Augmentation 

	Reanimation (w/adrenaline)
		Causes the player to come back to life after he has been dead for 3 seconds. 
		Will not work if player was gibbed. Single use.
	Long Jump
		Used by hitting the ??? key(s). Caused the player to further than normal.
	SCUBA	
		Used automatically after picked up and after player enters the water. 
		Works for N seconds. Single use.	
	
Things powered by the battery

	Armor		
		Uses N watts for every M units of damage.
	Heat/Cool	
		Uses N watts for every second in hot/cold area.
	Long Jump	
		Uses N watts for every jump.
	Alien Cloak	
		Uses N watts for each use. Each use lasts M seconds.
	Alien Shield	
		Augments armor. Reduces Armor drain by one half
 
*/
#if 0
// if in range of radiation source, ping geiger counter

#define GEIGERDELAY 0.25

void CBasePlayer ::UpdateGeigerCounter(void)
{
	BYTE range;

	// delay per update ie: don't flood net with these msgs
	if (gpGlobals->time < m_flgeigerDelay)
		return;

	m_flgeigerDelay = gpGlobals->time + GEIGERDELAY;

	// send range to radition source to client

	range = (BYTE)(m_flgeigerRange / 4);

	if (range != m_igeigerRangePrev)
	{
		m_igeigerRangePrev = range;

		MESSAGE_BEGIN(MSG_ONE, gmsgGeigerRange, NULL, pev);
		WRITE_BYTE(range);
		MESSAGE_END();
	}

	// reset counter and semaphore
	if (!RANDOM_LONG(0, 3))
		m_flgeigerRange = 1000;
}
#endif
/*
================
CheckSuitUpdate

Play suit update if it's time
================
*/

#define SUITUPDATETIME      3.5
#define SUITFIRSTUPDATETIME 0.1

void CBasePlayer::CheckSuitUpdate()
{
	int i;
	int isentence = 0;
	int isearch = m_iSuitPlayNext;

	// Ignore suit updates if no suit
	if (!GetWeaponOwn( WEAPON_SUIT ))
		return;
#if 0
	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();
#endif
	if (g_pGameRules->IsMultiplayer())
	{
		// don't bother updating HEV voice in multiplayer.
		return;
	}

	if (gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0)
	{
		// play a sentence off of the end of the queue
		for (i = 0; i < CSUITPLAYLIST; i++)
		{
			if (isentence = m_rgSuitPlayList[isearch])
				break;

			if (++isearch == CSUITPLAYLIST)
				isearch = 0;
		}

		if (isentence)
		{
			m_rgSuitPlayList[isearch] = 0;
			if (isentence > 0)
			{
				// play sentence number

				char sentence[CBSENTENCENAME_MAX + 1];
				UTIL_strcpy(sentence, "!");
				strcat(sentence, gszallsentencenames[isentence]);
				EMIT_SOUND_SUIT(ENT(pev), sentence);
			}
			else
			{
				// play sentence group
				EMIT_GROUPID_SUIT(ENT(pev), -isentence);
			}
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
		}
		else
			// queue is empty, don't check
			m_flSuitUpdate = 0;
	}
}

// add sentence to suit playlist queue. if fgroup is true, then
// name is a sentence group (HEV_AA), otherwise name is a specific
// sentence name ie: !HEV_AA0.  If iNoRepeat is specified in
// seconds, then we won't repeat playback of this word or sentence
// for at least that number of seconds.

void CBasePlayer::SetSuitUpdate(char *name, int fgroup, int iNoRepeatTime)
{
	int i;
	int isentence;
	int iempty = -1;

	// Ignore suit updates if no suit
	if (!GetWeaponOwn( WEAPON_SUIT ))
		return;

	if (g_pGameRules->IsMultiplayer())
	{
		// due to static channel design, etc. We don't play HEV sounds in multiplayer right now.
		return;
	}

	// if name == NULL, then clear out the queue

	if (!name)
	{
		for (i = 0; i < CSUITPLAYLIST; i++)
			m_rgSuitPlayList[i] = 0;
		return;
	}
	// get sentence or group number
	if (!fgroup)
	{
		isentence = SENTENCEG_Lookup(name, NULL);
		if (isentence < 0)
			return;
	}
	else
		// mark group number as negative
		isentence = -SENTENCEG_GetIndex(name);

	// check norepeat list - this list lets us cancel
	// the playback of words or sentences that have already
	// been played within a certain time.

	for (i = 0; i < CSUITNOREPEAT; i++)
	{
		if (isentence == m_rgiSuitNoRepeat[i])
		{
			// this sentence or group is already in
			// the norepeat list

			if (m_rgflSuitNoRepeatTime[i] < gpGlobals->time)
			{
				// norepeat time has expired, clear it out
				m_rgiSuitNoRepeat[i] = 0;
				m_rgflSuitNoRepeatTime[i] = 0.0;
				iempty = i;
				break;
			}
			else
			{
				// don't play, still marked as norepeat
				return;
			}
		}
		// keep track of empty slot
		if (!m_rgiSuitNoRepeat[i])
			iempty = i;
	}

	// sentence is not in norepeat list, save if norepeat time was given

	if (iNoRepeatTime)
	{
		if (iempty < 0)
			iempty = RANDOM_LONG(0, CSUITNOREPEAT - 1); // pick random slot to take over
		m_rgiSuitNoRepeat[iempty] = isentence;
		m_rgflSuitNoRepeatTime[iempty] = iNoRepeatTime + gpGlobals->time;
	}

	// find empty spot in queue, or overwrite last spot

	m_rgSuitPlayList[m_iSuitPlayNext++] = isentence;
	if (m_iSuitPlayNext == CSUITPLAYLIST)
		m_iSuitPlayNext = 0;

	if (m_flSuitUpdate <= gpGlobals->time)
	{
		if (m_flSuitUpdate == 0)
			// play queue is empty, don't delay too long before playback
			m_flSuitUpdate = gpGlobals->time + SUITFIRSTUPDATETIME;
		else
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
	}
}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
static void
CheckPowerups(entvars_t *pev)
{
	if (pev->health <= 0)
		return;

	pev->modelindex = g_ulModelIndexPlayer; // don't use eyes
}

//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
void CBasePlayer ::UpdatePlayerSound(void)
{
	int iBodyVolume;
	int iVolume;
	CSound *pSound;

	pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt ::ClientSoundIndex(edict()));

	if (!pSound)
	{
		ALERT(at_console, "Client lost reserved sound!\n");
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		iBodyVolume = pev->velocity.Length();

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast.
		if (iBodyVolume > 512)
		{
			iBodyVolume = 512;
		}
	}
	else
	{
		iBodyVolume = 0;
	}

	if (pev->button & IN_JUMP)
	{
		iBodyVolume += 100;
	}

	// convert player move speed and actions into sound audible by monsters.
	if (m_iWeaponVolume > iBodyVolume)
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player.
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if (m_iWeaponVolume < 0)
	{
		iVolume = 0;
	}

	// if target volume is greater than the player sound's current volume, we paste the new volume in
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if (m_iTargetVolume > iVolume)
	{
		iVolume = m_iTargetVolume;
	}
	else if (iVolume > m_iTargetVolume)
	{
		iVolume -= 250 * gpGlobals->frametime;

		if (iVolume < m_iTargetVolume)
		{
			iVolume = 0;
		}
	}

	if (m_fNoPlayerSound)
	{
		// debugging flag, lets players move around and shoot without monsters hearing.
		iVolume = 0;
	}

	if (gpGlobals->time > m_flStopExtraSoundTime)
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if (pSound)
	{
		pSound->m_vecOrigin = pev->origin;
		pSound->m_iType |= (bits_SOUND_PLAYER | m_iExtraSoundTypes);
		pSound->m_iVolume = iVolume;
	}

	// keep track of virtual muzzle flash
	m_iWeaponFlash -= 256 * gpGlobals->frametime;
	if (m_iWeaponFlash < 0)
		m_iWeaponFlash = 0;

	//UTIL_MakeVectors ( pev->angles );
	//gpGlobals->v_forward.z = 0;

	// Below are a couple of useful little bits that make it easier to determine just how much noise the
	// player is making.
	// UTIL_ParticleEffect ( pev->origin + gpGlobals->v_forward * iVolume, g_vecZero, 255, 25 );
	//ALERT ( at_console, "%d/%d\n", iVolume, m_iTargetVolume );
}

void CBasePlayer::PostThink()
{
	if (g_fGameOver)
		goto pt_end; // intermission or finale

	if (!IsAlive() || m_bInWelcomeCam)
		goto pt_end;

	// Handle Tank controlling
	if (m_pTank != NULL)
	{ // if they've moved too far from the gun,  or selected a weapon, unuse the gun
		if (m_pTank->OnControls(pev) && !pev->weaponmodel)
		{
			m_pTank->Use(this, this, USE_SET, 2); // try fire the gun
		}
		else
		{ // they've moved off the platform
			m_pTank->Use(this, this, USE_OFF, 0);
		}
	}

	// Check & Set our PlayerGaitState
	if ( IsInPanic() )
		pev->iuser4 = PlayerGaitState::GAIT_STATE_PANIC;
	else
	{
		if ( pev->maxspeed > 100 )
			pev->iuser4 = PlayerGaitState::GAIT_STATE_RUN;
		else
			pev->iuser4 = PlayerGaitState::GAIT_STATE_WALK;
	}

	// do weapon stuff
	ItemPostFrame();

	// Update our fatigue
	UpdateFatigue();

	// Update zombie health regen
	UpdateHealthRegen();

	// Are we hurt? Shit out blood
	DoBloodLoss();

	// Got backpack? Then make sure we have the right bodygroup applied
	if ( HasBackpack() )
		SetBodygroup( BGROUP_BACKPACK, BGROUP_SUB_VALUE1 );
	else
		SetBodygroup( BGROUP_BACKPACK, BGROUP_SUB_DEFAULT );

	// Pain killer stuff
	if ( pev->health > 0 && m_flPainPillHealthDelay != -1 )
	{
		if ( m_flPainPillHealthDelay - gpGlobals->time <= 0 )
		{
			m_iPillAmount++;
			int iHealth = pev->health;
			iHealth += 1;
			if ( iHealth > pev->max_health )
			{
				iHealth = pev->max_health;
				m_iPillAmount = 25;
			}
			pev->health = iHealth;

			if ( m_iPillAmount == 25 )
				m_flPainPillHealthDelay = -1;
			else
				m_flPainPillHealthDelay = gpGlobals->time + 2.0f;
		}
	}

	// If the player is falling to their death (survivor only) cause them to scream in agony!!
	// And make sure they are not on the ground
	if ( !(FBitSet(pev->flags, FL_ONGROUND)) &&
		(pev->health > 0) &&
		m_flFallVelocity >= PLAYER_FATAL_FALL_SPEED &&
		!m_bFallingToMyDeath &&
		pev->team == ZP::TEAM_SURVIVIOR
		)
	{
		float flFallDamage = m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		if ( flFallDamage > pev->health )
		{
			m_bFallingToMyDeath = true;
			DoVocalize( PlayerVocalizeType::VOCALIZE_AUTO_DEATH_FALL, true );
		}
	}

	if ( pev->health > 0 )
	{
		if ( ( m_flLastPlayerIdleAudio - gpGlobals->time <= 0 ) )
		{
			// If zombie, then moan
			if ( pev->team == ZP::TEAM_ZOMBIE )
			{
				m_flLastPlayerIdleAudio = gpGlobals->time + 8.5f;
				EMIT_SOUND( ENT(pev), CHAN_VOICE, UTIL_VarArgs( "player/zomambient%i.wav", RANDOM_LONG(1, 15) ), 1, ATTN_NORM );
			}
			// If survivor, do camp noises. But we only do that if we aren't moving.
			else if ( pev->team == ZP::TEAM_SURVIVIOR )
			{
				m_flLastPlayerIdleAudio = gpGlobals->time + 30.0f;
				DoVocalize( PlayerVocalizeType::VOCALIZE_AUTO_CAMP, true );
			}
		}
		else
		{
			// Camp stuff, if not moving (and also the round hasn't begun),
			// then reset the timer if we are a survivor
			if ( pev->team == ZP::TEAM_SURVIVIOR )
			{
				bool bIsMoving = false;
				// Are we moving, or pressing any buttons while being on the ground?
				if ( (pev->button & (IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT)) )
					bIsMoving = true;
				if ( ZP::GetCurrentRoundState() != ZP::RoundState::RoundState_RoundHasBegun )
					bIsMoving = true;
				if ( bIsMoving )
					m_flLastPlayerIdleAudio = gpGlobals->time + 30.0f;
			}
		}
	}

	// check to see if player landed hard enough to make a sound
	// falling farther than half of the maximum safe distance, but not as far a max safe distance will
	// play a bootscrape sound, and no damage will be inflicted. Fallling a distance shorter than half
	// of maximum safe distance will make no sound. Falling farther than max safe distance will play a
	// fallpain sound, and damage will be inflicted based on how far the player fell

	if ((FBitSet(pev->flags, FL_ONGROUND)) && (pev->health > 0) && m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD)
	{
		// ALERT ( at_console, "%f\n", m_flFallVelocity );

		if (pev->watertype == CONTENT_WATER)
		{
			// Did he hit the world or a non-moving entity?
			// BUG - this happens all the time in water, especially when
			// BUG - water has current force
			// if ( !pev->groundentity || VARS(pev->groundentity)->velocity.z == 0 )
			// EMIT_SOUND(ENT(pev), CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM);
		}
		else if (m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED)
		{ // after this point, we start doing damage

			bool bFallingToDeath = m_bFallingToMyDeath;

			m_bFallingToMyDeath = false;
			float flFallDamage = g_pGameRules->FlPlayerFallDamage(this);

			if (flFallDamage > pev->health)
			{ //splat
				// note: play on item channel because we play footstep landing on body channel
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat.wav", 1, ATTN_NORM);
			}

			if (flFallDamage > 0)
			{
				TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), flFallDamage, DMG_FALL);
				pev->punchangle.x = 0;
				if ( bFallingToDeath )
				{
					m_iDeathFlags |= PLR_DEATH_FLAG_FELL;
					GiveAchievement( EAchievements::I_FELL );
				}
			}
		}

		if (IsAlive())
		{
			SetAnimation(PLAYER_WALK);
		}
	}

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		if (m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer())
		{
			CSoundEnt::InsertSound(bits_SOUND_PLAYER, pev->origin, m_flFallVelocity, 0.2);
			// ALERT( at_console, "fall %f\n", m_flFallVelocity );
		}
		m_flFallVelocity = 0;
	}

	// select the proper animation for the player character
	if (IsAlive())
	{
		// Got no active weapon? Clear animation extension
		if ( !m_pActiveItem )
			m_szAnimExtention[0] = 0;

		if (!pev->velocity.x && !pev->velocity.y)
			SetAnimation(PLAYER_IDLE);
		else if ((pev->velocity.x || pev->velocity.y) && (FBitSet(pev->flags, FL_ONGROUND)))
			SetAnimation(PLAYER_WALK);
		else if (pev->waterlevel > 1)
			SetAnimation(PLAYER_WALK);
	}

	StudioFrameAdvance();
	CheckPowerups(pev);

	UpdatePlayerSound();

	// ZOMBIE PANIC -- START
	if ( pev->team == ZP::TEAM_ZOMBIE )
		SetBodygroup( BGROUP_EYEGLOW, m_bInZombieVision ? BGROUP_SUB_VALUE1 : BGROUP_SUB_DEFAULT );
	UpdatePlayerMaxSpeed();
	// ZOMBIE PANIC -- END

pt_end:
	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = pev->button;

	// Don't allow dead model to rotate until DeathCam or Spawn happen (CopyToBodyQue)
	if (pev->deadflag == DEAD_NO || (m_afPhysicsFlags & PFLAG_OBSERVER) || pev->fixangle)
		m_vecLastViewAngles = pev->angles;
	else
		pev->angles = m_vecLastViewAngles;

	// Do the player want to die?
	WantsToSuicide();

	// Recently killed someone with explosives
	JustKilledWithExplosives();

#if defined(CLIENT_WEAPONS)
	// Decay timers on weapons
	// go through all of the weapons and make a list of the ones to pack
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

			while (pPlayerItem)
			{
				CBasePlayerWeapon *gun;

				gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

				if (gun && gun->UseDecrement())
				{
					gun->m_flNextPrimaryAttack = max(gun->m_flNextPrimaryAttack - gpGlobals->frametime, -1.0f);
					gun->m_flNextSecondaryAttack = max(gun->m_flNextSecondaryAttack - gpGlobals->frametime, -0.001f);

					if (gun->m_flTimeWeaponIdle != 1000)
					{
						gun->m_flTimeWeaponIdle = max(gun->m_flTimeWeaponIdle - gpGlobals->frametime, -0.001f);
					}

					if (gun->pev->fuser1 != 1000)
					{
						gun->pev->fuser1 = max(gun->pev->fuser1 - gpGlobals->frametime, -0.001f);
					}

					// Only decrement if not flagged as NO_DECREMENT
					//if ( gun->m_flPumpTime != 1000 )
					//{
					//	gun->m_flPumpTime	= max( gun->m_flPumpTime - gpGlobals->frametime, -0.001f );
					//}
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	m_flNextAttack -= gpGlobals->frametime;
	if (m_flNextAttack < -0.001)
		m_flNextAttack = -0.001;

	if (m_flNextAmmoBurn != 1000)
	{
		m_flNextAmmoBurn -= gpGlobals->frametime;

		if (m_flNextAmmoBurn < -0.001)
			m_flNextAmmoBurn = -0.001;
	}

	if (m_flAmmoStartCharge != 1000)
	{
		m_flAmmoStartCharge -= gpGlobals->frametime;

		if (m_flAmmoStartCharge < -0.001)
			m_flAmmoStartCharge = -0.001;
	}
#endif
}

struct PlayerLastSpawnPoint
{
	int Player;
	int TeamID;
	std::vector<int> Spawns;
};
static std::vector<PlayerLastSpawnPoint *> m_PlayerSpawnList;
PlayerLastSpawnPoint *GrabLastSpawn( CBaseEntity *pPlayer )
{
	for ( size_t i = 0; i < m_PlayerSpawnList.size(); i++ )
	{
		PlayerLastSpawnPoint *found = m_PlayerSpawnList[i];
		if ( found->Player == pPlayer->entindex() )
			return found;
	}
	PlayerLastSpawnPoint *item = new PlayerLastSpawnPoint;
	item->Player = pPlayer->entindex();
	item->TeamID = pPlayer->pev->team;
	m_PlayerSpawnList.push_back( item );
	return item;
}

// Called from zp_gamerules.cpp
void RemovePlayerLastSpawnPointData( CBasePlayer *pPlayer )
{
	PlayerLastSpawnPoint *found = nullptr;
	int y = -1;
	for ( size_t i = 0; i < m_PlayerSpawnList.size(); i++ )
	{
		found = m_PlayerSpawnList[i];
		if ( found->Player == pPlayer->entindex() )
		{
			y = i;
			break;
		}
	}
	if ( y == -1 ) return;
	if ( found )
		delete found;
	found = nullptr;
	m_PlayerSpawnList.erase( m_PlayerSpawnList.begin() + y );
}

bool FoundSpawnPoint( PlayerLastSpawnPoint *item, CBaseEntity *pSpot )
{
	for ( size_t i = 0; i < item->Spawns.size(); i++ )
	{
		int found = item->Spawns[i];
		if ( found == pSpot->entindex() )
			return true;
	}
	return false;
}

bool IsSpawnPointValid(CBaseEntity *pPlayer, CBaseEntity *pSpot)
{
	PlayerLastSpawnPoint *item = GrabLastSpawn(pPlayer);
	return !FoundSpawnPoint(item, pSpot);
}

void ShouldClearSpawnChecks( CBaseEntity* pPlayer, const char *szSpawnLocation )
{
	PlayerLastSpawnPoint *item = GrabLastSpawn(pPlayer);
	bool bShouldClearSpawns = false;

	// Only used after we found a valid spawn.
	// Fallback spawns are ignored.
	if ( szSpawnLocation )
	{
		int iSpawnPoints = 0;
		CBaseEntity *pEnt = UTIL_FindEntityByClassname( NULL, szSpawnLocation );

		// Check if we have this spawn in our list!
		while ( pEnt )
		{
			iSpawnPoints++;
			pEnt = UTIL_FindEntityByClassname( pEnt, szSpawnLocation );
		}
		//Msg( "Spawn: %s - %i/%i\n", szSpawnLocation, item->Spawns.size(), iSpawnPoints );

		// We found way more or equal to our size value, clear our spawns!
		if ( item->Spawns.size() >= iSpawnPoints )
			bShouldClearSpawns = true;
	}

	// Not on the same team anymore? Clear it!
	if ( item->TeamID != pPlayer->pev->team )
		bShouldClearSpawns = true;

	// Clear our spawns!
	if ( bShouldClearSpawns )
		item->Spawns.clear();
}

bool UTIL_CanPlayerSeeThisSpawn( CBasePlayer* pPlayer, CBaseEntity* pEntSee )
{
	// Player is dead, ignore.
	if ( !pPlayer->IsAlive() ) return false;

	// Let's do a normal trace with FVisible.
	if ( pPlayer->FVisible( pEntSee ) ) return true;

	// Nope, can't see it normally, so let's try specific points.

	Vector vecOrigin = pEntSee->pev->origin;
	Vector vecMins = VEC_HULL_MIN;
	Vector vecMaxs = VEC_HULL_MAX;

	// Check all 9 points of the hull.
	// Example:
	// top left, top middle, top right
	// middle left, middle middle, middle right
	// bottom left, bottom middle, bottom right

	const Vector vecPoints[9] =
	{
		Vector( vecMins.x, vecMins.y, 0 ),
		Vector( 0, vecMins.y, 0 ),
		Vector( vecMaxs.x, vecMins.y, 0 ),
		Vector( vecMins.x, 0, 0 ),
		Vector( 0, 0, 0 ),
		Vector( vecMaxs.x, 0, 0 ),
		Vector( vecMins.x, vecMaxs.y, 0 ),
		Vector( 0, vecMaxs.y, 0 ),
		Vector( vecMaxs.x, vecMaxs.y, 0 ),
	};

	for ( int x = 0; x <= 8; x++ )
	{
		Vector checkPoint = vecOrigin + vecPoints[x];
		if ( pPlayer->FVisible( checkPoint ) ) return true;
		// Try slightly above ground
		checkPoint.z += 16;
		if ( pPlayer->FVisible( checkPoint ) ) return true;
	}

	// Let's increase the hull size by 32 units in each direction and try again.
	vecMins = vecMins - Vector( 32, 32, 0 );
	vecMaxs = vecMaxs + Vector( 32, 32, 0 );
	
	// Same as above, but bigger hull.
	const Vector vecPointsBigger[9] = {
		Vector( vecMins.x, vecMins.y, 0 ),
		Vector( 0, vecMins.y, 0 ),
		Vector( vecMaxs.x, vecMins.y, 0 ),
		Vector( vecMins.x, 0, 0 ),
		Vector( 0, 0, 0 ),
		Vector( vecMaxs.x, 0, 0 ),
		Vector( vecMins.x, vecMaxs.y, 0 ),
		Vector( 0, vecMaxs.y, 0 ),
		Vector( vecMaxs.x, vecMaxs.y, 0 ),
	};

	for ( int x = 0; x <= 8; x++ )
	{
		Vector checkPoint = vecOrigin + vecPointsBigger[x];
		if ( pPlayer->FVisible( checkPoint ) ) return true;
		// Try slightly above ground
		checkPoint.z += 16;
		if ( pPlayer->FVisible( checkPoint ) ) return true;
	}

	// Nope, can't see it at all.
	return false;
}

// checks if the spot is clear of players
static SpawnPointValidity CheckSpawnPointValidity( CBaseEntity *pPlayer, CBaseEntity *pSpot )
{
	// The entity is invalid, that's not good.
	if ( !pSpot ) return SpawnPointValidity::NonValid;

	// Make sure this is a proper spawn point entity
	CBasePlayerSpawnPoint *spawnPoint = dynamic_cast<CBasePlayerSpawnPoint *>(pSpot);
	if ( !spawnPoint ) return SpawnPointValidity::NonValid;

	if ( spawnPoint->pev->origin == Vector(0, 0, 0) )
		return SpawnPointValidity::NonValid;

	// Have we already spawned here before?
	if ( !IsSpawnPointValid( pPlayer, spawnPoint ) )
		return SpawnPointValidity::NonValidAlreadyUsed;

	// Let's do a trace hull, so we aren't spawning inside another player.
	// Because that would be bad...
	TraceResult tr;
	Vector vecSrc = spawnPoint->Center() + Vector( 0, 0, 16 );
	Vector VecEnd = vecSrc - Vector( 0, 0, 64 ); // Make it look down only
	UTIL_TraceHull( vecSrc, VecEnd, dont_ignore_monsters, human_hull, spawnPoint->edict(), &tr );
	if ( tr.flFraction != 1.0 )
	{
		// We hit something, is it a player?
		CBaseEntity *pHitEnt = CBaseEntity::Instance( tr.pHit );
		if ( pHitEnt && pHitEnt->IsPlayer() )
		{
			// Yup, can't spawn here.
			return SpawnPointValidity::NonValidOccupied;
		}
	}

	// Do another hull trace, but this is using UTIL_EntitiesInBox using our player hull size (well, double the hull size)
	// We don't want to spawn inside a player that is standing on top of this spawn point,
	// or if this spawn is in a vent, and the player is falling from the spawn point, etc.
	CBaseEntity *pEntList[15];
	Vector vecSpawnPointOrigin = spawnPoint->pev->origin;
	Vector vecSpawnPointMins = vecSpawnPointOrigin + ( VEC_HULL_MIN + VEC_HULL_MIN );
	Vector vecSpawnPointMaxs = vecSpawnPointOrigin + ( VEC_HULL_MAX + VEC_HULL_MAX );
	int iEntsInBox = UTIL_EntitiesInBox( pEntList, 15, vecSpawnPointMins, vecSpawnPointMaxs, FL_CLIENT | FL_MONSTER );
	for ( int i = 0; i < iEntsInBox; i++ )
	{
		CBaseEntity *pEnt = pEntList[i];
		if ( pEnt && pEnt->IsPlayer() )
		{
			// OI THERE GOVNA, YOU CAN'T SPAWN HERE, SOMEONE'S IN THE WAY.
			return SpawnPointValidity::NonValidOccupied;
		}
	}

	// Disabled spawn point?
	if ( !spawnPoint->IsEnabled() )
		return SpawnPointValidity::NonValidDisabled;

	// Has someone already spawned here?
	if ( spawnPoint->HasSpawned() )
		return SpawnPointValidity::NonValidWasRecentlyUsed;

	// Human spawn point? Return valid.
	if ( spawnPoint->IsHumanSpawnPoint() )
		return SpawnPointValidity::Valid;

	// This is a copy of the monster.cpp CBaseMonster::Look function.
	// We are simply checking if there are any players in the area,
	// and of course, we also check if the player within this can actually see us.
	// If they can see us, then we can't spawn here.
	// Because that would be bad...
	{
		const int iDistance = 2500;
		Vector delta = Vector( iDistance, iDistance, iDistance );
		CBaseEntity *pList[100];
		CBasePlayer *pSightEnt = NULL; // the current visible entity that we're dealing with
		int count = UTIL_EntitiesInBox( pList, 100, spawnPoint->pev->origin - delta, spawnPoint->pev->origin + delta, FL_CLIENT );
		for ( int i = 0; i < count; i++ )
		{
			pSightEnt = dynamic_cast<CBasePlayer *>( pList[i] );
			// Zombie spawn, and we aren't a human? Allow spawn.
			// This means zombies can spawn anywhere if a zombie or spectators can see this spawn entity.
			if ( !spawnPoint->IsHumanSpawnPoint() && pSightEnt->pev->team != ZP::TEAM_SURVIVIOR )
				continue;
			// if this is not the spot entity, and the entity can see me.
			if ( UTIL_CanPlayerSeeThisSpawn( pSightEnt, spawnPoint ) )
				return SpawnPointValidity::NonValidInPVS;
		}
	}

	return SpawnPointValidity::Valid;
}

DLL_GLOBAL CBaseEntity *g_pLastSpawn;
inline int FNullEnt(CBaseEntity *ent) { return (ent == NULL) || FNullEnt(ent->edict()); }

static bool m_bTriedFallback = false;
static CBaseEntity *EntSelectSpawnPointZPFallback(CBaseEntity *pPlayer, const char *szSpawnLocation)
{
	m_bTriedFallback = false;
	CBaseEntity *pSpot;

	int nNumRandomSpawnsToTry = 10;

	// choose a spawn point
	if (NULL == g_pLastSpawn)
	{
		int nNumSpawnPoints = 0;
		CBaseEntity *pEnt = UTIL_FindEntityByClassname(NULL, szSpawnLocation);
		while (NULL != pEnt)
		{
			nNumSpawnPoints++;
			pEnt = UTIL_FindEntityByClassname(pEnt, szSpawnLocation);
		}
		nNumRandomSpawnsToTry = nNumSpawnPoints;
	}

	pSpot = g_pLastSpawn;
	// Randomize the start spot
	for (int i = RANDOM_LONG(1, nNumRandomSpawnsToTry - 1); i > 0; i--)
		pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnLocation);
	if (FNullEnt(pSpot)) // skip over the null point
		pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnLocation);

	CBaseEntity *pFirstSpot = pSpot;
	{
		// try to find team spawn
		CBaseEntity *pFirstSpot = pSpot;

		do
		{
			if (pSpot)
			{
				// check if pSpot is valid
				if (CheckSpawnPointValidity(pPlayer, pSpot) == SpawnPointValidity::Valid &&
					g_pGameRules->GetTeamIndex(pPlayer->TeamID()) != pSpot->pev->team)
				{
					if (pSpot->pev->origin == Vector(0, 0, 0))
					{
						pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnLocation);
						continue;
					}

					// if so, go to pSpot
					return pSpot;
				}
			}
			// increment pSpot
			pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnLocation);
		} while (pSpot != pFirstSpot); // loop if we're not back to the start
	}

	do
	{
		if (pSpot)
		{
			// check if pSpot is valid
			if (CheckSpawnPointValidity(pPlayer, pSpot) == SpawnPointValidity::Valid)
			{
				if (pSpot->pev->origin == Vector(0, 0, 0))
				{
					pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnLocation);
					continue;
				}

				// if so, go to pSpot
				return pSpot;
			}
		}
		// increment pSpot
		pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnLocation);
	} while (pSpot != pFirstSpot); // loop if we're not back to the start

	// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
	if (!FNullEnt(pSpot))
		return pSpot;

	return nullptr;
}

ConVar sv_player_spawndebug( "sv_player_spawndebug", "0", FCVAR_CHEATS );

static CBaseEntity *EntSelectSpawnPointZP(CBaseEntity *pPlayer)
{
	std::vector<CBaseEntity *> spotInfos;

	// Find all spawn spots
	CBaseEntity *pSpot = nullptr;

	int iTeamNum = (int)clamp(pPlayer->pev->team, ZP::TEAM_NONE, ZP::TEAM_ZOMBIE);
	static const char *TeamSpawnLocations[ZP::MAX_TEAM] = {
		"info_player_observer",
		"info_player_observer",
		"info_player_team1",
		"info_player_team2"
	};
	const char *szSpawnLocation = TeamSpawnLocations[iTeamNum];

retry_spawns:

	// try to find team spawn
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnLocation)))
	{
		SpawnPointValidity nSpotCheck = CheckSpawnPointValidity(pPlayer, pSpot);
		if ( sv_player_spawndebug.GetBool() )
		{
			switch ( nSpotCheck )
			{
				case SpawnPointValidity::NonValid: Msg( "Spawn %s is not valid!\n", STRING(pSpot->pev->targetname) ); break;
				case SpawnPointValidity::NonValidDisabled: Msg( "Spawn %s is disabled!\n", STRING(pSpot->pev->targetname) ); break;
				case SpawnPointValidity::NonValidWasRecentlyUsed: Msg( "Spawn %s was recently used!\n", STRING(pSpot->pev->targetname) ); break;
				case SpawnPointValidity::NonValidAlreadyUsed: Msg( "Spawn %s is already used!\n", STRING(pSpot->pev->targetname) ); break;
				case SpawnPointValidity::NonValidOccupied: Msg( "Spawn %s is already occupied!\n", STRING(pSpot->pev->targetname) ); break;
				case SpawnPointValidity::NonValidInPVS: Msg( "Spawn %s can be seen by the player!\n", STRING(pSpot->pev->targetname) ); break;
				case SpawnPointValidity::Valid: Msg( "Spawn %s is valid!\n", STRING(pSpot->pev->targetname) ); break;
			}
		}

		if ( nSpotCheck != SpawnPointValidity::Valid )
			continue;

		spotInfos.push_back( pSpot );
	}

	// Sort them
	int validSpots = spotInfos.size();
	int limit = validSpots;

	if ( sv_player_spawndebug.GetBool() )
		Msg( "FindSpawn: %s - Limit: %i\n", szSpawnLocation, limit );

	// Still zero? Something must be wrong with the map,
	// or we are missing spawn locations.
	if ( limit == 0 )
	{
		// We hit the limit, clear olur spawn checks!
		ShouldClearSpawnChecks( pPlayer, nullptr );

		// If true, we have no spawns at all, so just fallback to normal spawn selection.
		// This will cause the player to spawn in front of others, but at least they will spawn.
		if ( m_bTriedFallback )
		{
			Msg( "Using fallback spawns...\n" );
			return EntSelectSpawnPointZPFallback( pPlayer, szSpawnLocation );
		}

		// Avoid looping
		m_bTriedFallback = true;
		if ( sv_player_spawndebug.GetBool() )
			Msg( "No spawns found, resetting and trying fallback...\n" );
		goto retry_spawns;
	}

	int take = rand() % limit;

	// If we are on the zombie team, pick a spawn that is closest to a survivor
	if ( pPlayer->pev->team == ZP::TEAM_ZOMBIE && validSpots > 1 )
	{
		float closestDist = 99999;
		int closestIndex = take;
		for ( int i = 0; i < validSpots; i++ )
		{
			CBaseEntity *ent = nullptr;
			float closestDistForThis = 99999;
			while ( ( ent = UTIL_FindEntityInSphere( ent, spotInfos[i]->pev->origin, 3000 ) ) != NULL )
			{
				if ( ent->IsPlayer() && ent->pev->team == ZP::TEAM_SURVIVIOR )
				{
					float dist = ( ent->pev->origin - spotInfos[i]->pev->origin ).Length();
					if ( dist < closestDistForThis )
						closestDistForThis = dist;
				}
			}
			if ( closestDistForThis < closestDist )
			{
				closestDist = closestDistForThis;
				closestIndex = i;
			}
		}
		take = closestIndex;
	}

	// Make sure we add the spawn!
	PlayerLastSpawnPoint *item = GrabLastSpawn(pPlayer);
	item->Spawns.push_back( spotInfos[take]->entindex() );

	// We hit the limit? Clear it!
	ShouldClearSpawnChecks( pPlayer, szSpawnLocation );

	return spotInfos[take];
}

/*
============
EntSelectSpawnPoint

Returns the entity to spawn at

USES AND SETS GLOBAL g_pLastSpawn
============
*/
edict_t* EntSelectSpawnPoint(CBasePlayer* pPlayer)
{
	CBaseEntity *pSpot = EntSelectSpawnPointZP( pPlayer );

	if (!pSpot)
	{
		// If startspot is set, (re)spawn there.
		if (FStringNull(gpGlobals->startspot) || !strlen(STRING(gpGlobals->startspot)))
		{
			pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
			UTIL_LogPrintf( "Failed to spawn player at team location, picking 'info_player_start' instead\n" );
		}
		else
		{
			pSpot = UTIL_FindEntityByTargetname(NULL, STRING(gpGlobals->startspot));
			UTIL_LogPrintf( "Failed to spawn player at team location, picking '%s' instead\n", STRING(gpGlobals->startspot) );
		}
	}

	if (FNullEnt(pSpot))
	{
		ALERT(at_error, "PutClientInServer: no info_player_start on level");
		return INDEXENT(0);
	}

	g_pLastSpawn = pSpot;
	return pSpot->edict();
}

void CBasePlayer::Spawn(void)
{
	m_flLastVocalize = gpGlobals->time + 1.0f;
	m_flBloodLoss = gpGlobals->time + 1.0f;
	m_flImHurtDelay = gpGlobals->time + 1.0f;
	m_flStartCharge = gpGlobals->time;
	m_flRefuseWeaponAudioCalls = gpGlobals->time + 0.5f;
	m_bConnected = TRUE;

	m_iDeathFlags = 0;
	ClientAPIData_t apiData = ZPGameRules()->GetClientAPI( this );
	m_bInZombieVision = apiData.KeepZVision;
	m_bBuddhaMode = false;

	m_iWeaponKillCount = 0;
	m_iKillsThisLife[0] = 0;
	m_iKillsThisLife[1] = 0;

	// We just spawned, allow auto weapon switch
	m_bJustSpawned = true;

	// We are no longer bleeding.
	for ( int i = 0; i < 4; i++ )
		m_iBleedHit[i] = 0;
	m_bIsBleeding = false;
	m_bGotBandage = false;
	// No pills
	m_bGotPainkiller = false;
	m_flPainPillHealthDelay = -1;
	m_iPillAmount = 0;
	m_iPillsTaken = 0;

	// Clear it
	m_AssistedDamage.clear();

	pev->classname = MAKE_STRING("player");
	pev->health = GetMaxHealth();
	pev->armorvalue = 0;
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = pev->health;
	pev->flags &= FL_PROXY; // keep proxy flag set by engine, clear others
	pev->flags |= FL_CLIENT;
	// Set or remove zombie player flag
	if ( pev->team == ZP::TEAM_ZOMBIE )
		pev->flags |= FL_ZOMBIE_PLAYER;
	pev->air_finished = gpGlobals->time + 12;
	pev->dmg = 2; // initial water damage
	pev->effects = 0;
	pev->deadflag = DEAD_NO;
	pev->dmg_take = 0;
	pev->dmg_save = 0;
	pev->friction = 1.0;
	pev->gravity = 1.0;
	m_bitsHUDDamage = -1;
	m_bitsDamageType = 0;
	m_afPhysicsFlags = 0;
	m_fLongJump = FALSE; // no longjump module.

	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "0");
	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "hl", "1");
	UpdateRoundInputLockPhysicsKey( this );

	pev->fov = m_iFOV = 0; // init field of view.
	m_iClientFOV = -1; // make sure fov reset is sent

	m_flNextDecalTime = 0; // let this player decal as soon as he spawns.
#if 0
	m_flgeigerDelay = gpGlobals->time + 2.0; // wait a few seconds until user-defined message registrations
	    // are recieved by all clients
#endif
	m_flTimeStepSound = 0;
	m_iStepLeft = 0;
	m_flFieldOfView = 0.5; // some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor = BLOOD_COLOR_RED;
	m_flNextAttack = UTIL_WeaponTimeBase();
	StartSneaking();

	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message

	// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam(this);
	if ( !m_bSpawnInPlace )
		g_pGameRules->GetPlayerSpawnSpot(this);
	m_bSpawnInPlace = false;

	// Move all player spectators to new target origin (bugfix for pmove/PAS issue)
	CBasePlayer *plr;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (!plr || plr->pev->iuser1 == 0 || plr->pev->iuser1 == OBS_ROAMING || plr->pev->iuser1 == OBS_MAP_FREE || plr->pev->iuser2 == 0 || plr->m_hObserverTarget != this)
			continue;

		UTIL_SetOrigin(plr->pev, pev->origin);
	}

	SetTheCorrectPlayerModel();
	SetBackpackState( false );
	SetBodygroup( BGROUP_BACKPACK, BGROUP_SUB_DEFAULT );

	g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence = LookupActivity(ACT_IDLE);

	SetPlayerHullSize();

	pev->view_ofs = VEC_VIEW;
	Precache();
	m_HackedGunPos = Vector(0, 32, 0);

	// Reset view entity
	SET_VIEW(edict(), edict());

	if (m_iPlayerSound == SOUNDLIST_EMPTY)
	{
		ALERT(at_console, "Couldn't alloc player sound slot!\n");
	}

	m_fNoPlayerSound = FALSE; // normal sound behavior.

	m_pLastItem = NULL;
	m_fInitHUD = TRUE;
	m_iClientHideHUD = -1; // force this to be recalculated
	m_fWeapon = FALSE;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;

	// Reset all the weapon slots
	for ( int x = 0; x < MAX_WEAPON_SLOTS; x++ )
		WeaponSlots[x] = false;

	// reset all ammo values to 0
	for (int i = 0; i < ZPAmmoTypes::AMMO_MAX; i++)
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0; // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	m_lastx = m_lasty = 0;

	m_iChatFlood = 0;
	m_flNextChatTime = 0; // Not using gpGlobals->time - see Host_Say
	m_flNextFullupdate[0] = gpGlobals->time;
	m_flNextFullupdate[1] = gpGlobals->time;

	// Just to make sure we can't spam the crap
	m_flNextWeaponSwitch = gpGlobals->time;
	m_flLastWeaponDrop = gpGlobals->time;
	m_flLastAmmoDrop = gpGlobals->time;
	m_flLastPanic = gpGlobals->time;
	m_flPanicTime = gpGlobals->time;
	m_flLastUnusedDrop = gpGlobals->time;
	m_flLastPlayerIdleAudio = gpGlobals->time + RANDOM_FLOAT( 6, 10 );
	m_bFallingToMyDeath = false;
	m_iAmmoTypeToDrop = 0;

	// Always set it to 20 seconds per spawn.
	// This will be reset back to 20 when the round begins
	m_flCanSuicide = gpGlobals->time + 20.0f;
	m_flSuicideTimer = -1;

	m_flResetExplosiveKillNotice = -1;
	m_bJustKilledWithExplosive = false;

	g_pGameRules->PlayerSpawn(this);

	// Make sure to always update it!
	PlayerLastSpawnPoint *item = GrabLastSpawn(this);
	item->TeamID = pev->team;

	// Clear it
	ShouldClearSpawnChecks( this, nullptr );

	// Stop current voiceline
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "common/null.wav", 0, ATTN_NORM, 0, 100 );
}

void CBasePlayer ::Precache(void)
{
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if (WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet)
	{
		if (!WorldGraph.FSetGraphPointers())
		{
			ALERT(at_console, "**Graph pointers were not set!\n");
		}
		else
		{
			ALERT(at_console, "**Graph Pointers Set!\n");
		}
	}

	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	// init geiger counter vars during spawn and each time
	// we cross a level transition
#if 0
	m_flgeigerRange = 1000;
	m_igeigerRangePrev = 1000;
#endif
	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;

	m_iClientBattery = -1;

	m_iTrain = TRAIN_NEW;

	// Make sure any necessary user messages have been registered
	LinkUserMessages();

	m_iUpdateTime = 5; // won't update for 1/2 a second

	if (gInitHUD)
		m_fInitHUD = TRUE;
}

int CBasePlayer::Save(CSave &save)
{
	if (!CBaseMonster::Save(save))
		return 0;

	return save.WriteFields("PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData));
}

//
// Marks everything as new so the player will resend this to the hud.
//
void CBasePlayer::RenewItems(void)
{
}

int CBasePlayer::Restore(CRestore &restore)
{
	if (!CBaseMonster::Restore(restore))
		return 0;

	int status = restore.ReadFields("PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData));

	m_bConnected = TRUE;

	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;
	// landmark isn't present.
	if (!pSaveData->fUseLandmark)
	{
		ALERT(at_console, "No Landmark:%s\n", pSaveData->szLandmarkName);

		// default to normal spawn
		edict_t *pentSpawnSpot = EntSelectSpawnPoint(this);
		pev->origin = VARS(pentSpawnSpot)->origin + Vector(0, 0, 1);
		pev->angles = VARS(pentSpawnSpot)->angles;
	}
	pev->v_angle.z = 0; // Clear out roll
	pev->angles = pev->v_angle;

	pev->fixangle = TRUE; // turn this way immediately

	// Copied from spawn() for now
	m_bloodColor = BLOOD_COLOR_RED;

	g_ulModelIndexPlayer = pev->modelindex;

	SetPlayerHullSize();

	if ( pev->team == ZP::TEAM_ZOMBIE )
		pev->flags |= FL_ZOMBIE_PLAYER;

	g_engfuncs.pfnSetPhysicsKeyValue(edict(), "hl", "1");

	if (m_fLongJump)
	{
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "1");
	}
	else
	{
		g_engfuncs.pfnSetPhysicsKeyValue(edict(), "slj", "0");
	}

	RenewItems();

	// Resync ammo data so you can reload - Solokiller
	TabulateAmmo();

#if defined(CLIENT_WEAPONS)
	// HACK:	This variable is saved/restored in CBaseMonster as a time variable, but we're using it
	//			as just a counter.  Ideally, this needs its own variable that's saved as a plain float.
	//			Barring that, we clear it out here instead of using the incorrect restored time value.
	m_flNextAttack = UTIL_WeaponTimeBase();
#endif

	// Force a flashlight update for the HUD
	if (m_flFlashLightTime == 0)
	{
		m_flFlashLightTime = 1;
	}

	return status;
}

void CBasePlayer::SelectNextItem(int iItem)
{
	CBasePlayerItem *pItem;

	pItem = m_rgpPlayerItems[iItem];

	if (!pItem)
		return;

	if ( !CanSelectNewWeapon( true ) ) return;

	if (pItem == m_pActiveItem)
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext;
		if (!pItem)
		{
			return;
		}

		CBasePlayerItem *pLast;
		pLast = pItem;
		while (pLast->m_pNext)
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext = m_pActiveItem;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[iItem] = pItem;
	}

	if ( !pItem ) return;
	CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>( pItem );
	if ( pWeapon )
		SelectWeapon( pWeapon );
}

void CBasePlayer::SelectItem(const char *pstr)
{
	if ( !CanProcessRoundGameplayInput( this ) )
		return;

	if (!pstr)
		return;
	if ( !CanSelectNewWeapon( true ) ) return;

	CBasePlayerItem *pItem = NULL;

	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			pItem = m_rgpPlayerItems[i];

			while (pItem)
			{
				if (FClassnameIs(pItem->pev, pstr))
					break;
				pItem = pItem->m_pNext;
			}
		}

		if (pItem)
			break;
	}

	if (!pItem)
		return;

	CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>( pItem );
	if ( pWeapon )
		SelectWeapon( pWeapon );
}

void CBasePlayer::SelectLastItem(void)
{
	if ( !CanProcessRoundGameplayInput( this ) )
		return;

	if ( !m_pLastItem ) return;
	if ( m_pActiveItem && !m_pActiveItem->CanHolster() ) return;
	if ( !CanSelectNewWeapon( true ) ) return;
	CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>( m_pLastItem );
	if ( pWeapon )
		SelectWeapon( pWeapon );
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
BOOL CBasePlayer::HasWeapons(void)
{
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			return TRUE;
		}
	}

	return FALSE;
}

void CBasePlayer::SelectPrevItem(int iItem)
{
}

const char *CBasePlayer::TeamID(void)
{
	if (pev == NULL) // Not fully connected yet
		return "";
	int iTeamNum = pev->team;
	if ( iTeamNum < ZP::TEAM_NONE ) iTeamNum = ZP::TEAM_NONE;
	else if ( iTeamNum > ZP::TEAM_ZOMBIE ) iTeamNum = ZP::TEAM_ZOMBIE;
	return ZP::Teams[iTeamNum];
}

//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void Spawn(entvars_t *pevOwner);
	void Think(void);

	virtual int ObjectCaps(void) { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn(entvars_t *pevOwner)
{
	pev->origin = pevOwner->origin + Vector(0, 0, 32);
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);
	pev->frame = 0;

	pev->nextthink = gpGlobals->time + 0.1;
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/sprayer.wav", 1, ATTN_NORM);
}

void CSprayCan::Think(void)
{
	TraceResult tr;
	int playernum;
	int nFrames;
	CBasePlayer *pPlayer;

	pPlayer = (CBasePlayer *)GET_PRIVATE(pev->owner);

	if (pPlayer)
		nFrames = pPlayer->GetCustomDecalFrames();
	else
		nFrames = -1;

	playernum = ENTINDEX(pev->owner);

	// ALERT(at_console, "Spray by player %i, %i of %i\n", playernum, (int)(pev->frame + 1), nFrames);

	UTIL_MakeVectors(pev->angles);
	UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr);

	// No customization present.
	if (nFrames == -1)
	{
		UTIL_DecalTrace(&tr, DECAL_LAMBDA6);
		UTIL_Remove(this);
	}
	else
	{
		UTIL_PlayerDecalTrace(&tr, playernum, pev->frame, TRUE);
		// Just painted last custom frame.
		if (pev->frame++ >= (nFrames - 1))
			UTIL_Remove(this);
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

class CBloodSplat : public CBaseEntity
{
public:
	void Spawn(entvars_t *pevOwner);
	void Spray(void);
};

void CBloodSplat::Spawn(entvars_t *pevOwner)
{
	pev->origin = pevOwner->origin + Vector(0, 0, 32);
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);

	SetThink(&CBloodSplat::Spray);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBloodSplat::Spray(void)
{
	TraceResult tr;

	if (g_Language != LANGUAGE_GERMAN)
	{
		UTIL_MakeVectors(pev->angles);
		UTIL_TraceLine(pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, &tr);

		UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_RED, true);
	}
	SetThink(&CBloodSplat::SUB_Remove);
	pev->nextthink = gpGlobals->time + 0.1;
}

//==============================================

void CBasePlayer::GiveNamedItem( const char *pszName )
{
	if ( !IsAlive() ) return;
	edict_t *pent;

	char weaponName[ 256 ];
	if ( V_strncasecmp( pszName, "weapon_", 7 ) == 0 )
		V_strncpy( weaponName, pszName, sizeof( weaponName ) );
	else if ( V_strncasecmp( pszName, "item_", 5 ) == 0 )
		V_strncpy( weaponName, pszName, sizeof( weaponName ) );
	else if ( V_strncasecmp( pszName, "ammo_", 5 ) == 0 )
		V_strncpy( weaponName, pszName, sizeof( weaponName ) );
	else
		V_snprintf( weaponName, sizeof( weaponName ), "weapon_%s", pszName );
	V_strlower( weaponName );

	pent = CREATE_NAMED_ENTITY( ALLOC_STRING( weaponName ) );
	if ( FNullEnt( pent ) )
	{
		Msg( "NULL Ent in GiveNamedItem! %s tried to give themselves [%s]\n", GetPlayerName(), weaponName );
		return;
	}

	VARS( pent )->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;
	if ( FStrEq( weaponName, "weapon_fafo" ) && pev->team == ZP::TEAM_ZOMBIE )
		pent->v.team = ZP::TEAM_ZOMBIE; // Set team for FAFO if zombie

	DispatchSpawn( pent );

	// Make sure this is set, so we can delete it if used
	CBaseEntity *pItem = CBaseEntity::Instance( pent );
	pItem->SetSpawnedTroughRandomEntity( true );

	DispatchUse( pent, ENT( pev ) );
}

CBaseEntity *FindEntityForward(CBaseEntity *pMe)
{
	TraceResult tr;

	UTIL_MakeVectors(pMe->pev->v_angle);
	UTIL_TraceLine(pMe->pev->origin + pMe->pev->view_ofs, pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192, dont_ignore_monsters, pMe->edict(), &tr);
	if (tr.flFraction != 1.0 && !FNullEnt(tr.pHit))
	{
		CBaseEntity *pHit = CBaseEntity::Instance(tr.pHit);
		return pHit;
	}
	return NULL;
}

BOOL CBasePlayer ::FlashlightIsOn(void)
{
	bool bIsZombie = (pev->team == ZP::TEAM_ZOMBIE) ? true : false;
	if ( bIsZombie )
#if ZVISION_BLIGHT
		return FBitSet(pev->effects, EF_BRIGHTLIGHT);
#else
		return m_bInZombieVision;
#endif
	return FBitSet(pev->effects, EF_DIMLIGHT);
}

void CBasePlayer ::FlashlightTurnOn(void)
{
	if (GetWeaponOwn( WEAPON_SUIT ))
	{
		bool bIsZombie = (pev->team == ZP::TEAM_ZOMBIE) ? true : false;
		if ( !bIsZombie )
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_AUTO, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM);
			SetBits(pev->effects, EF_DIMLIGHT);
		}
		else
#if ZVISION_BLIGHT
			SetBits(pev->effects, EF_BRIGHTLIGHT);
#else
			m_bInZombieVision = true;
#endif
		MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
		WRITE_BYTE(1);
		WRITE_BYTE((pev->team == ZP::TEAM_ZOMBIE) ? 1 : 0);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();

		m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
	}
}

void CBasePlayer ::FlashlightTurnOff(void)
{
	bool bIsZombie = (pev->team == ZP::TEAM_ZOMBIE) ? true : false;
	if ( !bIsZombie )
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_AUTO, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM);
		ClearBits(pev->effects, EF_DIMLIGHT);
	}
	else
#if ZVISION_BLIGHT
		ClearBits(pev->effects, EF_BRIGHTLIGHT);
#else
		m_bInZombieVision = false;
#endif
	MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
	WRITE_BYTE(0);
	WRITE_BYTE(0);
	WRITE_BYTE(m_iFlashBattery);
	MESSAGE_END();

	m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
}

/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
void CBasePlayer ::ForceClientDllUpdate(void)
{
	m_iClientHideHUD = -1;
	m_iClientFOV = -1; // make sure fov reset is sent
	m_iClientHealth = -1;
	m_iClientAmmoType = -1;
	m_iClientBattery = -1;
	m_iTrain |= TRAIN_NEW; // Force new train message.
	m_fWeapon = FALSE; // Force weapon send
	m_fKnownItem = FALSE; // Force weaponinit messages.
	m_fInitHUD = TRUE; // Force HUD gmsgResetHUD message

	// client ammo values also have to be reset
	for (int i = 0; i < ZPAmmoTypes::AMMO_MAX; i++)
	{
		m_rgAmmoLast[i] = 0;
	}

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();
}

/*
============
ImpulseCommands
============
*/

void CBasePlayer::ImpulseCommands()
{
	if ( !CanProcessRoundGameplayInput( this ) )
	{
		pev->impulse = 0;
		return;
	}

	TraceResult tr; // UNDONE: kill me! This is temporary for PreAlpha CDs

	// Handle use events
	PlayerUse();

	int iImpulse = (int)pev->impulse;
	switch (iImpulse)
	{
	case 99:
	{
		gLogoState = !gLogoState;
		const int iOn = gLogoState;

		ASSERT(gmsgLogo > 0);
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgLogo, NULL, pev);
		WRITE_BYTE(iOn);
		MESSAGE_END();
		break;
	}
	case 100:
		// temporary flashlight for level designers
		if (FlashlightIsOn())
		{
			FlashlightTurnOff();
		}
		else
		{
			FlashlightTurnOn();
		}
		break;

	case 201: // paint decal

		if (gpGlobals->time < m_flNextDecalTime)
		{
			// too early!
			break;
		}

		// Do we allow player decals?
		if ( !sv_allow_player_decals.GetBool() ) break;

		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine(pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0)
		{ // line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + decalfrequency.value;
			CSprayCan *pCan = GetClassPtr((CSprayCan *)NULL);
			pCan->Spawn(pev);
		}

		break;

	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands(iImpulse);
		break;
	}

	pev->impulse = 0;
}

//=========================================================
//=========================================================
void CBasePlayer::CheatImpulseCommands(int iImpulse)
{
#if !defined(HLDEMO_BUILD)
	bool bIsCheatsEnabled = CVAR_GET_FLOAT("sv_cheats") >= 1 ? true : false;
	if ( !bIsCheatsEnabled ) return;

	CBaseEntity *pEntity;
	TraceResult tr;

	switch (iImpulse)
	{
	case 101:
		ClientPrint(pev, HUD_PRINTCONSOLE, UTIL_VarArgs("Nice try.\n"));
		break;

	case 102:
		// Gibbage!!!
		CGib::SpawnRandomGibs(pev, 1, 1);
		break;

	case 104:
		// Dump all of the global state varaibles (and global entity names)
		gGlobalState.DumpGlobals();
		break;

	case 106:
		// Give me the classname and targetname of this entity.
		pEntity = FindEntityForward(this);
		if (pEntity)
		{
			ALERT(at_console, "Classname: %s", STRING(pEntity->pev->classname));

			if (!FStringNull(pEntity->pev->targetname))
			{
				ALERT(at_console, " - Targetname: %s\n", STRING(pEntity->pev->targetname));
			}
			else
			{
				ALERT(at_console, " - TargetName: No Targetname\n");
			}

			ALERT(at_console, "Model: %s\n", STRING(pEntity->pev->model));
			if (pEntity->pev->globalname)
				ALERT(at_console, "Globalname: %s\n", STRING(pEntity->pev->globalname));
		}
		break;

	case 107:
	{
		TraceResult tr;

		edict_t *pWorld = g_engfuncs.pfnPEntityOfEntIndex(0);

		Vector start = pev->origin + pev->view_ofs;
		Vector end = start + gpGlobals->v_forward * 1024;
		UTIL_TraceLine(start, end, ignore_monsters, edict(), &tr);
		if (tr.pHit)
			pWorld = tr.pHit;
		const char *pTextureName = TRACE_TEXTURE(pWorld, start, end);
		if (pTextureName)
			Msg( "Texture: %s\n", pTextureName );
	}
	break;
	case 195: // show shortest paths for entire level to nearest node
	{
		Create("node_viewer_fly", pev->origin, pev->angles);
	}
	break;
	case 196: // show shortest paths for entire level to nearest node
	{
		Create("node_viewer_large", pev->origin, pev->angles);
	}
	break;
	case 197: // show shortest paths for entire level to nearest node
	{
		Create("node_viewer_human", pev->origin, pev->angles);
	}
	break;
	case 199: // show nearest node and all connections
	{
		ALERT(at_console, "%d\n", WorldGraph.FindNearestNode(pev->origin, bits_NODE_GROUP_REALM));
		WorldGraph.ShowNodeConnections(WorldGraph.FindNearestNode(pev->origin, bits_NODE_GROUP_REALM));
	}
	break;
	case 202: // Random blood splatter
		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine(pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), &tr);

		if (tr.flFraction != 1.0)
		{ // line hit something, so paint a decal
			CBloodSplat *pBlood = GetClassPtr((CBloodSplat *)NULL);
			pBlood->Spawn(pev);
		}
		break;
	case 203: // remove creature.
		pEntity = FindEntityForward(this);
		if (pEntity)
		{
			if (pEntity->pev->takedamage)
				pEntity->SetThink(&CBasePlayer::SUB_Remove);
		}
		break;
	}
#endif // HLDEMO_BUILD
}

bool CBasePlayer::AlreadyOwnWeapon( CBasePlayerItem *pWeapon )
{
	if ( pWeapon->iFlags() & ITEM_FLAG_ALLOWDUPLICATE ) return false;
	return m_bOwnWeaponID[ pWeapon->GetWeaponID() ];
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
int CBasePlayer::AddPlayerItem(CBasePlayerItem *pItem)
{
	CWeaponBase *pWeapon = dynamic_cast<CWeaponBase *>( pItem );
	if ( !pWeapon ) return FALSE;

	CBasePlayerItem *pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];

	while (pInsert)
	{
		if (FClassnameIs(pInsert->pev, STRING(pItem->pev->classname)))
		{
			// Already own it? don't pick it up!
			// But do grab it's ammo, its very yummy for us!
			if ( AlreadyOwnWeapon( pItem ) )
			{
				if ( !pWeapon->AllowAmmoSteal() ) return FALSE;
				int iClip = pWeapon->m_iClip;
				if ( iClip > 0 )
				{
					AmmoData nAmmoData = GetAmmoByName( pWeapon->GetData().Ammo1 );
					int iAmount = PickupAmmo( iClip, nAmmoData );
					if ( iAmount > 0 )
					{
						EMIT_SOUND( ENT(pWeapon->pev), CHAN_ITEM, "items/ammo_pickup.wav", 1, ATTN_NORM );
						// Nudge the weapon towards me!
						Vector vNewVel( 0, 0, 15 );
						UTIL_MakeVectors( pev->v_angle );
						Vector vecThrow = gpGlobals->v_forward * 25;
						vNewVel -= vecThrow;
						pWeapon->pev->velocity = vNewVel;
						pWeapon->m_iClip -= iAmount;
						pWeapon->pev->playerclass = (pWeapon->m_iClip == 0) ? 1 : 0; // if iuser1 is 1, it's empty
						// Nab the ammo, so we have nothing inside the cylinder.
						// Omnomnom, delicious ammo!
						CWeaponSideArmRevolver *pRevolver = dynamic_cast<CWeaponSideArmRevolver *>( pWeapon );
						if ( pRevolver )
							pRevolver->m_bHasUnloaded = true;
					}
				}
				return FALSE;
			}
			if (pItem->AddDuplicate(pInsert))
			{
				g_pGameRules->PlayerGotWeapon(this, pItem);
				pItem->CheckRespawn();

				// ugly hack to update clip w/o an update clip message
				pInsert->UpdateItemInfo();
				if (m_pActiveItem)
					m_pActiveItem->UpdateItemInfo();

				pItem->Kill();
			}
			else if (gEvilImpulse101)
			{
				// FIXME: remove anyway for deathmatch testing
				pItem->Kill();
			}
			return FALSE;
		}
		pInsert = pInsert->m_pNext;
	}

	bool bCanPlayerPickup = false;
	if ( pev->team == ZP::TEAM_SURVIVIOR )
		bCanPlayerPickup = pItem->AddToPlayer(this);
	else if ( pev->team == ZP::TEAM_ZOMBIE )
		bCanPlayerPickup = pItem->pev->team == ZP::TEAM_ZOMBIE ? pItem->AddToPlayer(this) : false;

	if ( bCanPlayerPickup )
	{
		g_pGameRules->PlayerGotWeapon(this, pItem);

		pItem->m_pNext = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		// should we switch to this item?
		if (g_pGameRules->FShouldSwitchWeapon(this, pItem))
		{
			if ( !m_bJustSpawned )
			{
				bool bRet = m_pActiveItem ? true : false;
				ClientAPIData_t apiData = ZPGameRules()->GetClientAPI( this );
				// If false, then do not auto switch to the weapon
				if ( !apiData.AutoSwitchOnPickup && bRet ) return TRUE;
			}
			SelectWeapon( pWeapon );
			m_bJustSpawned = false;
		}

		return TRUE;
	}
	else if (gEvilImpulse101)
	{
		// FIXME: remove anyway for deathmatch testing
		pItem->Kill();
	}
	return FALSE;
}

int CBasePlayer::RemovePlayerItem(CBasePlayerItem *pItem)
{
	pItem->pev->nextthink = 0; // crowbar may be trying to swing again, etc.
	pItem->SetThink(NULL);
	pItem->SetTouch(NULL);

	if (m_pActiveItem == pItem)
	{
		ResetAutoaim();
		if (pItem->m_pPlayer) // Ugly way to distinguish between calls from PackWeapon and DestroyItem
			pItem->Holster();
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}

	if (m_pLastItem == pItem)
		m_pLastItem = NULL;

	pItem->m_pPlayer = NULL;

	int slotId = pItem->iItemSlot();
	CBasePlayerItem *pPrev = m_rgpPlayerItems[slotId];

	if (pPrev == pItem)
	{
		SetWeaponOwn( pItem->GetWeaponID(), false );
		m_rgpPlayerItems[slotId] = pItem->m_pNext;
		return TRUE;
	}
	else
	{
		while (pPrev && pPrev->m_pNext != pItem)
		{
			pPrev = pPrev->m_pNext;
		}
		if (pPrev)
		{
			SetWeaponOwn( pItem->GetWeaponID(), false );
			pPrev->m_pNext = pItem->m_pNext;
			return TRUE;
		}
	}
	return FALSE;
}

int CBasePlayer::GiveAmmo( int iAmount, ZPAmmoTypes ammotype )
{
	AmmoData data = GetAmmoByAmmoID( ammotype );
	int iMaxCarry = data.MaxCarry;
	bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	// If in hardcore mode and don't have a backpack, halve the max carry.
	if ( bIsInHardcore && !HasBackpack() )
		iMaxCarry = (int)( (float)iMaxCarry * 0.5f );

	if (!g_pGameRules->CanHaveAmmo( this, data.AmmoName, iMaxCarry ) )
	{
		// game rules say I can't have any more of this ammo type.
		return -1;
	}

	int i = 0;

	i = GetAmmoIndex( data.AmmoName );

	if (i < 0 || i >= ZPAmmoTypes::AMMO_MAX)
		return -1;

	int iAdd = min(iAmount, iMaxCarry - m_rgAmmo[i]);
	if (iAdd < 1)
		return i;

	m_rgAmmo[i] += iAdd;

	if (gmsgAmmoPickup) // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoPickup, NULL, pev);
		WRITE_BYTE(ammotype); // ammo ID
		WRITE_BYTE(iAdd); // amount
		MESSAGE_END();

		if (!this->m_fInitHUD)
		{
			// Send to all spectating players
			CBasePlayer *plr;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (!plr || plr->pev->iuser1 != OBS_IN_EYE || plr->m_hObserverTarget != this)
					continue;

				MESSAGE_BEGIN(MSG_ONE, gmsgAmmoPickup, NULL, plr->pev);
				WRITE_BYTE(ammotype); // ammo ID
				WRITE_BYTE(iAdd); // amount
				MESSAGE_END();
			}
		}
	}

	TabulateAmmo();

	return i;
}

//
// Returns the unique ID for the ammo, or -1 if error
//
int CBasePlayer ::GiveAmmo(int iCount, char *szName)
{
	if (!szName)
	{
		// no ammo.
		return -1;
	}
	return GiveAmmo( iCount, GetAmmoByName( szName ).AmmoType );
}

/*
============
ItemPreFrame

Called every frame by the player PreThink
============
*/
void CBasePlayer::ItemPreFrame()
{
	if ( m_flRefuseWeaponAudioCalls - gpGlobals->time <= 0 )
	{
		if ( m_pActiveItem )
			m_pActiveItem->DoAudioFrame();
	}
#if defined( CLIENT_WEAPONS )
	if ( m_flNextAttack > 0 )
#else
	if ( gpGlobals->time < m_flNextAttack )
#endif
		return;
	if ( !m_pActiveItem ) return;
	m_pActiveItem->ItemPreFrame();
}

/*
============
ItemPostFrame

Called every frame by the player PostThink
============
*/
void CBasePlayer::ItemPostFrame()
{
	static int fInSelect = FALSE;

	if ( !CanProcessRoundGameplayInput( this ) )
	{
		pev->impulse = 0;
		return;
	}

	// check if the player is using a tank
	if (m_pTank != NULL)
		return;

	ImpulseCommands();

#if defined(CLIENT_WEAPONS)
	if (m_flNextAttack > 0)
#else
	if (gpGlobals->time < m_flNextAttack)
#endif
	{
		return;
	}

	if ( !m_pActiveItem ) return;
	m_pActiveItem->ItemPostFrame();
}

int CBasePlayer::AmmoInventory(int iAmmoIndex)
{
	if ( iAmmoIndex < 0 || iAmmoIndex >= ZPAmmoTypes::AMMO_MAX )
	{
		ALERT(at_console, "Bad Ammo Index %d requested\n", iAmmoIndex);
		return -1;
	}
	return m_rgAmmo[iAmmoIndex];
}

int CBasePlayer::GetAmmoIndex( const char *psz )
{
	return GetAmmoByName( psz ).AmmoType;
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
// If this player is spectating someone target will be in pPlayer
void CBasePlayer::SendAmmoUpdate(CBasePlayer *pPlayer)
{
	for (int i = 0; i < ZPAmmoTypes::AMMO_MAX; i++)
	{
		if (this->m_rgAmmoLast[i] != pPlayer->m_rgAmmo[i])
		{
			this->m_rgAmmoLast[i] = pPlayer->m_rgAmmo[i];

			ASSERT(pPlayer->m_rgAmmo[i] >= 0);
			ASSERT(pPlayer->m_rgAmmo[i] < 255);

			// send "Ammo" update message
			MESSAGE_BEGIN(MSG_ONE, gmsgAmmoX, NULL, pev);
			WRITE_BYTE(i);
			WRITE_BYTE(max(min(pPlayer->m_rgAmmo[i], 254), 0)); // clamp the value to one byte
			MESSAGE_END();
		}
	}
}

/*
=========================================================
	UpdateClientData

resends any changed player HUD info to the client.
Called every frame by PlayerPreThink
Also called at start of demo recording and playback by
ForceClientDllUpdate to ensure the demo gets messages
reflecting all of the HUD state info.
=========================================================
*/
void CBasePlayer ::UpdateClientData(void)
{
	if (m_fInitHUD)
	{
		m_fInitHUD = FALSE;
		gInitHUD = FALSE;

		MESSAGE_BEGIN(MSG_ONE, gmsgResetHUD, NULL, pev);
		WRITE_BYTE(0);
		MESSAGE_END();

		if (!m_fGameHUDInitialized)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgInitHUD, NULL, pev);
			MESSAGE_END();

			// Switch to first person mode
			MESSAGE_BEGIN(MSG_ONE, gmsgViewMode, NULL, pev);
			MESSAGE_END();

			// Send spectator statuses
			CBasePlayer *plr;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (!plr || !plr->IsObserver())
					continue;

				MESSAGE_BEGIN(MSG_ONE, gmsgSpectator, NULL, pev);
				WRITE_BYTE(ENTINDEX(plr->edict())); // index number of primary entity
				WRITE_BYTE(1);
				MESSAGE_END();
			}

			// Send fog message
			CClientFog *pFog = (CClientFog *)UTIL_FindEntityByClassname( nullptr, "env_fog" );
			while ( pFog )
			{
				if ( pFog->m_bActive )
					pFog->SendInitMessages( this );
				pFog = (CClientFog *)UTIL_FindEntityByClassname( pFog, "env_fog" );
			}

			g_pGameRules->InitHUD(this);
			m_fGameHUDInitialized = TRUE;
			m_iObserverMode = OBS_ROAMING;
			if (g_pGameRules->IsMultiplayer())
			{
				FireTargets("game_playerjoin", this, this, USE_TOGGLE, 0);
			}
		}

		// Send flashlight status
		MESSAGE_BEGIN(MSG_ONE, gmsgFlashlight, NULL, pev);
		WRITE_BYTE(FlashlightIsOn() ? 1 : 0);
		WRITE_BYTE((pev->team == ZP::TEAM_ZOMBIE && m_bInZombieVision) ? 1 : 0);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();

		InitStatusBar();
	}

	CBasePlayer *pPlayer = this;
	// We will take spectating target player status if we have it
	if (pev->iuser1 == OBS_IN_EYE && m_hObserverTarget)
	{
		// This will fix angles, so pain display will show correct direction
		pev->angles = pev->v_angle = m_hObserverTarget->pev->angles;
		pev->fixangle = TRUE;
		pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(ENTINDEX(m_hObserverTarget->edict()));
		if (!pPlayer)
			pPlayer = this;
	}

	if (pPlayer->m_iHideHUD != m_iClientHideHUD)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgHideWeapon, NULL, pev);
		WRITE_BYTE(pPlayer->m_iHideHUD);
		MESSAGE_END();

		m_iClientHideHUD = pPlayer->m_iHideHUD;
	}

	if (pPlayer->m_iFOV != m_iClientFOV)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, pev);
		WRITE_BYTE(pPlayer->m_iFOV);
		MESSAGE_END();

		// cache FOV change at end of function, so weapon updates can see that FOV has changed
	}

	// HACKHACK -- send the message to display the game title
	if (gDisplayTitle)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgShowGameTitle, NULL, pev);
		WRITE_BYTE(0);
		MESSAGE_END();
		gDisplayTitle = 0;
	}

	if ( m_iAmmoTypeToDrop != m_iClientAmmoType && gmsgAmmoBankUpdate > 0 )
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoBankUpdate, NULL, pev);
		WRITE_SHORT(m_iAmmoTypeToDrop);
		MESSAGE_END();
		m_iClientAmmoType = m_iAmmoTypeToDrop;
	}

	float fHealth = pPlayer->pev->health;
	int iHealth = fHealth <= 0.0 ? 0 : (fHealth <= 1.0 ? 1 : (fHealth > 255.0 ? 255 : (int)fHealth));
	if (iHealth != m_iClientHealth)
	{
		// send "health" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, pev);
		WRITE_BYTE(iHealth);
		MESSAGE_END();

		m_iClientHealth = iHealth;
	}

	float fArmor = pPlayer->pev->armorvalue;
	int iArmor = fArmor <= 0.0 ? 0 : (fArmor <= 1.0 ? 1 : (fArmor > 32767.0 ? 32767 : (int)fArmor));
	if (iArmor != m_iClientBattery)
	{
		ASSERT(gmsgBattery > 0);
		// send "armor" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgBattery, NULL, pev);
		WRITE_SHORT(iArmor);
		MESSAGE_END();

		m_iClientBattery = iArmor;
	}

	if (pev->dmg_take || pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = pev->origin;
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		edict_t *other = pev->dmg_inflictor;
		if (other)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(other);
			if (pEntity)
				damageOrigin = pEntity->Center();
		}

		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;

		// Send this player's damage to all his specators
		CBasePlayer *plr;
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
			if (!plr || !plr->IsObserver() || plr->m_hObserverTarget != this)
				continue;

			MESSAGE_BEGIN(MSG_ONE, gmsgDamage, NULL, plr->pev);
			WRITE_BYTE(pev->dmg_save);
			WRITE_BYTE(pev->dmg_take);
			WRITE_LONG(visibleDamageBits);
			WRITE_COORD(damageOrigin.x);
			WRITE_COORD(damageOrigin.y);
			WRITE_COORD(damageOrigin.z);
			MESSAGE_END();
		}

		MESSAGE_BEGIN(MSG_ONE, gmsgDamage, NULL, pev);
		WRITE_BYTE(pev->dmg_save);
		WRITE_BYTE(pev->dmg_take);
		WRITE_LONG(visibleDamageBits);
		WRITE_COORD(damageOrigin.x);
		WRITE_COORD(damageOrigin.y);
		WRITE_COORD(damageOrigin.z);
		MESSAGE_END();

		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}

	// Update Flashlight
	if ((m_flFlashLightTime) && (m_flFlashLightTime <= gpGlobals->time))
	{
		if (FlashlightIsOn())
		{
			if (m_iFlashBattery)
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				if ( pev->team != ZP::TEAM_ZOMBIE )
					m_iFlashBattery--;

				if (!m_iFlashBattery)
					FlashlightTurnOff();
			}
		}
		else
		{
			if (m_iFlashBattery < 100)
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
				m_iFlashBattery++;
			}
			else
				m_flFlashLightTime = 0;
		}

		MESSAGE_BEGIN(MSG_ONE, gmsgFlashBattery, NULL, pev);
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();
	}

	if (m_iTrain & TRAIN_NEW)
	{
		ASSERT(gmsgTrain > 0);
		// send "train" update message
		MESSAGE_BEGIN(MSG_ONE, gmsgTrain, NULL, pev);
		WRITE_BYTE(m_iTrain & 0xF);
		MESSAGE_END();

		m_iTrain &= ~TRAIN_NEW;
	}

	//
	// New Weapon?
	//
	if (!m_fKnownItem)
	{
		m_fKnownItem = TRUE;

		// WeaponInit Message
		// byte  = # of weapons
		//
		// for each weapon:
		// byte		name str length (not including null)
		// bytes	name
		// byte		Ammo Type
		// byte		Ammo2 Type
		// byte		bucket
		// byte		bucket pos
		// byte		flags
		// ????		Icons

		// Send ALL the weapon info now
		for (int i = 0; i < MAX_WEAPONS; i++)
		{
			ItemInfo &II = CBasePlayerItem::ItemInfoArray[i];

			if (!II.iId)
				continue;

			const char *pszName;
			if (!II.pszName)
				pszName = "Empty";
			else
				pszName = II.pszName;

			MESSAGE_BEGIN(MSG_ONE, gmsgWeaponList, NULL, pev);
			WRITE_STRING(pszName); // string	weapon name
			WRITE_BYTE(GetAmmoIndex(II.pszAmmo1)); // byte		Ammo Type
			WRITE_BYTE(GetAmmoIndex(II.pszAmmo2)); // byte		Ammo2 Type
			WRITE_BYTE(II.iSlot); // byte		bucket
			//WRITE_BYTE(II.iPosition); // byte		bucket pos
			WRITE_BYTE(II.iId); // byte		id (bit index into pev->weapons)
			WRITE_BYTE(II.iFlags); // byte		Flags
			WRITE_BYTE(II.iWeight); // byte		Weight
			WRITE_BYTE(II.bDoubleSlot ? 1 : 0); // byte		Double Slot
			MESSAGE_END();
		}
	}

	SendAmmoUpdate(pPlayer);

	// Update all the items
	for (int i = 0; i < MAX_WEAPON_SLOTS; i++)
	{
		CBasePlayerWeapon *pWeapon = GetWeaponFromSlot( i );
		if ( !pWeapon ) continue;
		pWeapon->UpdateClientData( this );
	}

	if (m_pClientActiveItem != pPlayer->m_pActiveItem)
	{
		if (pPlayer->m_pActiveItem == NULL)
		{
			// If no weapon, we have to send update here
			CBasePlayer *plr;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (!plr || !plr->IsObserver() || plr->m_hObserverTarget != pPlayer)
					continue;

				MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, plr->pev);
				WRITE_BYTE(0);
				WRITE_BYTE(0);
				WRITE_BYTE(0);
				WRITE_BYTE(0);
				MESSAGE_END();
			}

			MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pPlayer->pev);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			MESSAGE_END();
		}
		else if (this != pPlayer)
		{
			// Special case for spectator
			CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayer->m_pActiveItem->GetWeaponPtr();
			if (gun)
			{
				int state;
				if (pPlayer->m_fOnTarget)
					state = WEAPON_IS_ONTARGET;
				else
					state = 1;

				MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, pev);
				WRITE_BYTE(state);
				WRITE_BYTE(gun->GetWeaponID());
				WRITE_BYTE(gun->m_iClip);
				WRITE_BYTE(gun->GetData().MaxClip);
				MESSAGE_END();
			}
		}
	}

	// Cache fov and client weapon change
	m_pClientActiveItem = pPlayer->m_pActiveItem;
	m_iClientFOV = pPlayer->m_iFOV;

	// Update Status Bar
	if (m_flNextSBarUpdateTime < gpGlobals->time)
	{
		UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.01;
	}
}

CBasePlayerWeapon *CBasePlayer::GetWeaponFromID( int iID )
{
	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		CBasePlayerItem *pItem = m_rgpPlayerItems[i];
		while ( pItem )
		{
			if ( pItem->GetWeaponID() == iID )
				return (CBasePlayerWeapon *)pItem;
			pItem = pItem->m_pNext;
		}
	}
	return nullptr;
}

void CBasePlayer::NotifyOfWeaponPickup(CBasePlayerWeapon *pWeapon)
{
	MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pev);
	WRITE_BYTE(pWeapon->GetWeaponID());
	WRITE_BYTE(pWeapon->m_iAssignedSlotPosition);
	WRITE_BYTE(pWeapon->m_iClip); // Fixes the clip value being maxed out
	MESSAGE_END();
}

void CBasePlayer::IncreaseBleed(int iIndex)
{
	int iAmountHit = 0;
	for ( int i = 0; i < 4; i++ )
	{
		if ( m_iBleedHit[i] == iIndex ) return;
		if ( m_iBleedHit[i] != 0 )
		{
			iAmountHit++;
			continue;
		}
		m_iBleedHit[i] = iIndex;
		iAmountHit++;
		break;
	}
	if ( iAmountHit == 4 )
		GiveAchievement( HC_BLOODHARVEST );
}

void CBasePlayer::DoVocalize( PlayerVocalizeType nType, bool bForced )
{
	if ( nType == VOCALIZE_NONE ) return;
	if ( pev->team != ZP::TEAM_SURVIVIOR ) return;
	bool bCanSpeak = ( m_flLastVocalize - gpGlobals->time <= 0 ) ? true : false;
	if ( bForced ) bCanSpeak = true;
	if ( !bCanSpeak ) return;
	const float flDelay = bForced ? 1.0f : 5.0f;
	m_flLastVocalize = gpGlobals->time + flDelay;
	MESSAGE_BEGIN( MSG_ALL, gmsgVocalize );
	WRITE_SHORT( entindex() );
	WRITE_SHORT( nType );
	MESSAGE_END();
	VocalizeData data = GetVocalizeData( m_iCharacter, nType );
	if ( data.Type == VOCALIZE_NONE ) return;
	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, data.VoiceLine.c_str(), 1, ATTN_NORM, 0, 100 );
}

void CBasePlayer ::SetPrefsFromUserinfo(char *infobuffer)
{
	const char *pszKeyVal;

	// Set autoswitch preference
	pszKeyVal = g_engfuncs.pfnInfoKeyValue(infobuffer, "cl_autowepswitch");
	if (FStrEq(pszKeyVal, ""))
	{
		m_iAutoWepSwitch = 1;
	}
	else
	{
		m_iAutoWepSwitch = atoi(pszKeyVal);
	}
}

//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
BOOL CBasePlayer ::FBecomeProne(void)
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer ::BarnacleVictimBitten(entvars_t *pevBarnacle)
{
	TakeDamage(pevBarnacle, pevBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB);
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns.
//=========================================================
void CBasePlayer ::BarnacleVictimReleased(void)
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}

//=========================================================
// Illumination
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer ::Illumination(void)
{
	int iIllum = CBaseEntity::Illumination();

	iIllum += m_iWeaponFlash;
	if (iIllum > 255)
		return 255;
	return iIllum;
}

void CBasePlayer ::EnableControl(BOOL fControl)
{
	if (!fControl)
		pev->flags |= FL_FROZEN;
	else
		pev->flags &= ~FL_FROZEN;
}

int CBasePlayer::PickupAmmo( int iAmount, AmmoData data )
{
	int iMaxCarry = data.MaxCarry;
	bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	// If in hardcore mode and don't have a backpack, halve the max carry.
	if ( bIsInHardcore && !HasBackpack() && iMaxCarry > 1 )
		iMaxCarry = (int)( (float)iMaxCarry * 0.5f );

	if (!g_pGameRules->CanHaveAmmo( this, data.AmmoName, iMaxCarry ) )
	{
		// game rules say I can't have any more of this ammo type.
		return -1;
	}

	int i = data.AmmoType;
	if ( i < 0 || i >= ZPAmmoTypes::AMMO_MAX ) return -1;

	int iAdd = min(iAmount, iMaxCarry - m_rgAmmo[i]);
	if ( iAdd < 1 ) return 0;

	m_rgAmmo[i] += iAdd;

	if (gmsgAmmoPickup) // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN(MSG_ONE, gmsgAmmoPickup, NULL, pev);
		WRITE_BYTE(data.AmmoType); // ammo ID
		WRITE_BYTE(iAdd); // amount
		MESSAGE_END();

		if (!this->m_fInitHUD)
		{
			// Send to all spectating players
			CBasePlayer *plr;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (!plr || plr->pev->iuser1 != OBS_IN_EYE || plr->m_hObserverTarget != this)
					continue;

				MESSAGE_BEGIN(MSG_ONE, gmsgAmmoPickup, NULL, plr->pev);
				WRITE_BYTE(data.AmmoType); // ammo ID
				WRITE_BYTE(iAdd); // amount
				MESSAGE_END();
			}
		}
	}

	TabulateAmmo();
	return iAdd;
}

bool CBasePlayer::CanGiveAmmo(int iAmount, AmmoData data)
{
	int iMaxCarry = data.MaxCarry;
	bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	// If in hardcore mode and don't have a backpack, halve the max carry.
	if ( bIsInHardcore && !HasBackpack() )
		iMaxCarry = (int)( (float)iMaxCarry * 0.5f );

	if ( !g_pGameRules->CanHaveAmmo( this, data.AmmoName, iMaxCarry ) )
		return false;

	int i = data.AmmoType;
	if ( i < 0 || i >= ZPAmmoTypes::AMMO_MAX ) return -1;

	int iAdd = min(iAmount, iMaxCarry - m_rgAmmo[i]);
	if ( iAdd < 1 ) return false;

	return true;
}

#define DOT_1DEGREE  0.9998476951564
#define DOT_2DEGREE  0.9993908270191
#define DOT_3DEGREE  0.9986295347546
#define DOT_4DEGREE  0.9975640502598
#define DOT_5DEGREE  0.9961946980917
#define DOT_6DEGREE  0.9945218953683
#define DOT_7DEGREE  0.9925461516413
#define DOT_8DEGREE  0.9902680687416
#define DOT_9DEGREE  0.9876883405951
#define DOT_10DEGREE 0.9848077530122
#define DOT_15DEGREE 0.9659258262891
#define DOT_20DEGREE 0.9396926207859
#define DOT_25DEGREE 0.9063077870367

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector CBasePlayer ::GetAutoaimVector(float flDelta)
{
	if (g_iSkillLevel == SKILL_HARD)
	{
		UTIL_MakeVectors(pev->v_angle + pev->punchangle);
		return gpGlobals->v_forward;
	}

	Vector vecSrc = GetGunPosition();
	float flDist = 8192;

	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (1 || g_iSkillLevel == SKILL_MEDIUM)
	{
		m_vecAutoAim = Vector(0, 0, 0);
		// flDelta *= 0.5;
	}

	BOOL m_fOldTargeting = m_fOnTarget;
	Vector angles = AutoaimDeflection(vecSrc, flDist, flDelta);

	// update ontarget if changed
	if (!g_pGameRules->AllowAutoTargetCrosshair())
		m_fOnTarget = 0;
	else if (m_fOldTargeting != m_fOnTarget)
	{
		m_pActiveItem->UpdateItemInfo();
	}

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;

	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (0 || g_iSkillLevel == SKILL_EASY)
	{
		m_vecAutoAim = m_vecAutoAim * 0.67 + angles * 0.33;
	}
	else
	{
		m_vecAutoAim = angles * 0.9;
	}

	// m_vecAutoAim = m_vecAutoAim * 0.99;

	// Don't send across network if sv_aim is 0
	if (g_psv_aim->value != 0 && (!g_psv_allow_autoaim || g_psv_allow_autoaim->value != 0))
	{
		if (m_vecAutoAim.x != m_lastx || m_vecAutoAim.y != m_lasty)
		{
			SET_CROSSHAIRANGLE(edict(), -m_vecAutoAim.x, m_vecAutoAim.y);

			m_lastx = m_vecAutoAim.x;
			m_lasty = m_vecAutoAim.y;
		}
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors(pev->v_angle + pev->punchangle + m_vecAutoAim);
	return gpGlobals->v_forward;
}

Vector CBasePlayer ::AutoaimDeflection(Vector &vecSrc, float flDist, float flDelta)
{
	edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex(1);
	CBaseEntity *pEntity;
	float bestdot;
	Vector bestdir;
	edict_t *bestent;
	TraceResult tr;

	if (g_psv_aim->value == 0 && (!g_psv_allow_autoaim || g_psv_allow_autoaim->value != 0))
	{
		m_fOnTarget = FALSE;
		return g_vecZero;
	}

	UTIL_MakeVectors(pev->v_angle + pev->punchangle + m_vecAutoAim);

	// try all possible entities
	bestdir = gpGlobals->v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	m_fOnTarget = FALSE;

	UTIL_TraceLine(vecSrc, vecSrc + bestdir * flDist, dont_ignore_monsters, edict(), &tr);

	if (tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
	{
		// don't look through water
		if (!((pev->waterlevel != 3 && tr.pHit->v.waterlevel == 3)
		        || (pev->waterlevel == 3 && tr.pHit->v.waterlevel == 0)))
		{
			if (tr.pHit->v.takedamage == DAMAGE_AIM)
				m_fOnTarget = TRUE;

			return m_vecAutoAim;
		}
	}

	for (int i = 1; i < gpGlobals->maxEntities; i++, pEdict++)
	{
		Vector center;
		Vector dir;
		float dot;

		if (pEdict->free) // Not in use
			continue;

		if (pEdict->v.takedamage != DAMAGE_AIM)
			continue;
		if (pEdict == edict())
			continue;
		//		if (pev->team > 0 && pEdict->v.team == pev->team)
		//			continue;	// don't aim at teammate
		if (!g_pGameRules->ShouldAutoAim(this, pEdict))
			continue;

		pEntity = Instance(pEdict);
		if (pEntity == NULL)
			continue;

		if (!pEntity->IsAlive())
			continue;

		// don't look through water
		if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3)
		    || (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
			continue;

		center = pEntity->BodyTarget(vecSrc);

		dir = (center - vecSrc).Normalized();

		// make sure it's in front of the player
		if (DotProduct(dir, gpGlobals->v_forward) < 0)
			continue;

		dot = fabs(DotProduct(dir, gpGlobals->v_right))
		    + fabs(DotProduct(dir, gpGlobals->v_up)) * 0.5;

		// tweek for distance
		dot *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);

		if (dot > bestdot)
			continue; // to far to turn

		UTIL_TraceLine(vecSrc, center, dont_ignore_monsters, edict(), &tr);
		if (tr.flFraction != 1.0 && tr.pHit != pEdict)
		{
			// ALERT( at_console, "hit %s, can't see %s\n", STRING( tr.pHit->v.classname ), STRING( pEdict->v.classname ) );
			continue;
		}

		// don't shoot at friends
		if (IRelationship(pEntity) < 0)
		{
			if (!pEntity->IsPlayer() && !g_pGameRules->IsDeathmatch())
				// ALERT( at_console, "friend\n");
				continue;
		}

		// can shoot at this one
		bestdot = dot;
		bestent = pEdict;
		bestdir = dir;
	}

	if (bestent)
	{
		bestdir = UTIL_VecToAngles(bestdir);
		bestdir.x = -bestdir.x;
		bestdir = bestdir - pev->v_angle - pev->punchangle;

		if (bestent->v.takedamage == DAMAGE_AIM)
			m_fOnTarget = TRUE;

		return bestdir;
	}

	return Vector(0, 0, 0);
}

void CBasePlayer ::ResetAutoaim()
{
	if (m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0)
	{
		m_vecAutoAim = Vector(0, 0, 0);
		SET_CROSSHAIRANGLE(edict(), 0, 0);
	}
	m_fOnTarget = FALSE;
}

/*
=============
SetCustomDecalFrames

  UNDONE:  Determine real frame limit, 8 is a placeholder.
  Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer ::SetCustomDecalFrames(int nFrames)
{
	if (nFrames > 0 && nFrames < 8)
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer ::GetCustomDecalFrames(void)
{
	return m_nCustomSprayFrames;
}

//=========================================================
// DropPlayerItem - drop the named item, or if no name,
// the active item.
//=========================================================
void CBasePlayer::DropPlayerItem(char *pszItemName)
{
	if ( pev->team != ZP::TEAM_SURVIVIOR ) return;

	CBasePlayerItem *pWeapon;
	if (!strlen(pszItemName))
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		pWeapon = m_pActiveItem;
	}
	else
	{
		// try to match by name.
		bool match = false;
		for (int i = 0; i < MAX_ITEM_TYPES && !match; i++)
		{
			pWeapon = m_rgpPlayerItems[i];
			while (pWeapon)
			{
				if (!strcmp(pszItemName, STRING(pWeapon->pev->classname)))
				{
					match = true;
					break;
				}

				pWeapon = pWeapon->m_pNext;
			}
		}
		if (!match)
			return;
	}

	// Return if we didn't find a weapon to drop
	if (!pWeapon)
		return;

	g_pGameRules->GetNextBestWeapon(this, pWeapon);

	UTIL_MakeVectors(pev->angles);

	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create("weaponbox", pev->origin + gpGlobals->v_forward * 10, pev->angles, edict());
	pWeaponBox->pev->angles.x = 0;
	pWeaponBox->pev->angles.z = 0;
	pWeaponBox->PackWeapon(pWeapon);
	pWeaponBox->pev->velocity = gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;

	// Stays forever
	pWeaponBox->pev->nextthink = 0;

	// drop half of the ammo for this weapon.
	int iAmmoIndex = GetAmmoByName(pWeapon->GetData().Ammo1).AmmoType;

	if (iAmmoIndex != -1)
	{
		// this weapon weapon uses ammo, so pack an appropriate amount.
		if (pWeapon->iFlags() & ITEM_FLAG_EXHAUSTIBLE)
		{
			// pack up all the ammo, this weapon is its own ammo type
			pWeaponBox->PackAmmo(MAKE_STRING(pWeapon->GetData().Ammo1), m_rgAmmo[iAmmoIndex]);
			m_rgAmmo[iAmmoIndex] = 0;
		}
		else
		{
			// pack half of the ammo
			int ammoDrop = m_rgAmmo[iAmmoIndex] / 2;
			pWeaponBox->PackAmmo(MAKE_STRING(pWeapon->GetData().Ammo1), ammoDrop);
			m_rgAmmo[iAmmoIndex] -= ammoDrop;
		}
	}
}

void CBasePlayer::DropActiveWeapon()
{
	if ( !CanDropWeapon( true ) ) return;
	if ( IsInPanic() ) return;
	if ( !m_pActiveItem ) return;
	m_flLastWeaponDrop = gpGlobals->time + 0.5f;
	DropWeapon( (CBasePlayerWeapon *)m_pActiveItem, true );
}

int CBasePlayer::AmmoIndexToDrop( int ammoindex )
{
	int idx = ammoindex;
	if ( ammoindex == -1 )
		idx = m_iAmmoTypeToDrop;
	switch ( idx )
	{
		case 0: return ZPAmmoTypes::AMMO_PISTOL;
		case 1: return ZPAmmoTypes::AMMO_MAGNUM;
		case 2: return ZPAmmoTypes::AMMO_SHOTGUN;
		case 3: return ZPAmmoTypes::AMMO_RIFLE;
		case 4: return ZPAmmoTypes::AMMO_LONGRIFLE;
		case 5: return ZPAmmoTypes::AMMO_GRENADE;
		case 6: return ZPAmmoTypes::AMMO_SATCHEL;
	}
	return 0;
}

int CBasePlayer::AmmoIndexToDropArray( int ammoindex )
{
	switch ( ammoindex )
	{
		// 9mm
	    case ZPAmmoTypes::AMMO_PISTOL: return 0;
		// 357
	    case ZPAmmoTypes::AMMO_MAGNUM: return 1;
		// 556ar
	    case ZPAmmoTypes::AMMO_SHOTGUN: return 2;
		// Buckshot
		case ZPAmmoTypes::AMMO_RIFLE: return 3;
		// Long Rifle
		case ZPAmmoTypes::AMMO_LONGRIFLE: return 4;
		// Grenade
		case ZPAmmoTypes::AMMO_GRENADE: return 5;
		// Satchel
		case ZPAmmoTypes::AMMO_SATCHEL: return 6;
	}
	return 0;
}

int CBasePlayer::DefaultAmmoToDrop( int ammoindex )
{
	AmmoData ammo = GetAmmoByAmmoID( ammoindex );
	return ammo.AmmoBoxGive;
}

const char *CBasePlayer::szAmmoToDropClassnames(int ammoindex)
{
	switch ( ammoindex )
	{
		// 9mm
	    case ZPAmmoTypes::AMMO_PISTOL: return "ammo_9mmclip";
		// 357
	    case ZPAmmoTypes::AMMO_MAGNUM: return "ammo_357";
		// Buckshot
		case ZPAmmoTypes::AMMO_SHOTGUN: return "ammo_buckshot";
		// 556ar
	    case ZPAmmoTypes::AMMO_RIFLE: return "ammo_556AR";
		// Long Rifle
	    case ZPAmmoTypes::AMMO_LONGRIFLE: return "ammo_22lrbox";
		// Barricade
	    case ZPAmmoTypes::AMMO_BARRICADE: return "ammo_barricade";
	}
	return "ammo_9mmclip";
}

void CBasePlayer::DropSelectedAmmo()
{
	if ( ZP::GetCurrentRoundState() < ZP::RoundState::RoundState_RoundHasBegun ) return;
	if ( ( m_flLastAmmoDrop - gpGlobals->time > 0 ) ) return;
	m_flLastAmmoDrop = gpGlobals->time + 0.25f;
	int iAmmoType = AmmoIndexToDrop();
	int amount = m_rgAmmo[ iAmmoType ];
	int ammoDrop = DefaultAmmoToDrop( iAmmoType );
	if ( amount < ammoDrop )
		ammoDrop = amount;
	if ( ammoDrop == 0 ) return;
	if ( !DropAmmo( iAmmoType, ammoDrop ) ) return;
	m_rgAmmo[ iAmmoType ] -= ammoDrop;
}

CBasePlayer::ThrowableDropState CBasePlayer::IsThrowableAndActive( CBasePlayerWeapon *pWeapon, bool bOnDrop )
{
	if ( pWeapon->IsThrowable() )
	{
		if ( pWeapon->m_flStartThrow )
		{
			Vector angThrow = pev->v_angle + pev->punchangle;
			if ( angThrow.x < 0 ) angThrow.x = -10 + angThrow.x * ((90 - 10) / 90.0);
			else angThrow.x = -10 + angThrow.x * ((90 + 10) / 90.0);

			static float flMultiplier = 6.5f;
			float flVel = (90 - angThrow.x) * flMultiplier;
			if ( flVel > 1000 ) flVel = 1000;

			UTIL_MakeVectors( angThrow );

			Vector vecSrc = pev->origin + pev->view_ofs + gpGlobals->v_forward * 16;
			Vector vecThrow = gpGlobals->v_forward * flVel + pev->velocity;

			// alway explode 5 seconds after the pin was pulled
			float time = pWeapon->m_flStartThrow - gpGlobals->time + 5.0;
			if ( time < 0 ) time = 1.0;

			if ( pWeapon->GetWeaponID() == WEAPON_MOLOTOV )
				CGrenade::ShootContact( pev, vecSrc, vecThrow, "models/w_molotov_thrown.mdl", CGrenade::CONTACT_TYPE::TYPE_MOLOTOV );
			else
				CGrenade::ShootTimed( pev, vecSrc, vecThrow, time, "models/w_tnt_thrown.mdl" );

			// Decrement
			pWeapon->m_iClip--;
			if ( pWeapon->m_iClip == 0 ) return ThrowableDropState::DELETE_ITEM_AND_ACTIVE;
			return ThrowableDropState::IS_ACTIVE;
		}
		else
			return ThrowableDropState::NOT_ACTIVE_THROWABLE;
	}
	return ThrowableDropState::NOT_ACTIVE;
}

int CBasePlayer::GetBestKillAssist(int iAttacker)
{
	int iDx = 0;
	float flDamage = 0.0f;
	for ( size_t i = 0; i < m_AssistedDamage.size(); i++ )
	{
		KillAssist assist = m_AssistedDamage[i];
		if ( assist.EntIndex == iAttacker ) continue;
		if ( assist.DamageDealt > flDamage )
		{
			iDx = assist.EntIndex;
			flDamage = assist.DamageDealt;
		}
	}
	return iDx;
}

void CBasePlayer::AddToAssistDamage(CBasePlayer *pPlayer, float flDamage)
{
	// Don't add ourselves.
	if ( pPlayer->entindex() == entindex() ) return;

	int iIdx = ENTINDEX(pPlayer->edict());
	for ( size_t i = 0; i < m_AssistedDamage.size(); i++ )
	{
		KillAssist &assist = m_AssistedDamage[i];
		if ( assist.EntIndex == iIdx )
		{
			assist.DamageDealt += flDamage;
			return;
		}
	}
	KillAssist killer;
	killer.EntIndex = iIdx;
	killer.DamageDealt = flDamage;
	m_AssistedDamage.push_back( killer );
}

bool CBasePlayer::DropAmmo( int ammoindex, int amount, Vector Dir, bool pukevel )
{
	// AMMO_GRENADE and higher, we don't allow any kind of ammo to be dropped.
	if ( ammoindex >= ZPAmmoTypes::AMMO_GRENADE ) return false;

	CBasePlayerAmmo *pAmmoItem = (CBasePlayerAmmo *)CBaseEntity::Create((char *)szAmmoToDropClassnames(ammoindex), Dir, pev->angles, nullptr);
	if ( !pAmmoItem ) return false;

	//pAmmoItem->DisallowPickupFor( 2.5f );
	pAmmoItem->m_iDroppedOverride = pAmmoItem->m_iAmountLeft = amount;
	pAmmoItem->pev->angles.x = 0;
	pAmmoItem->pev->angles.z = 0;
	pAmmoItem->pev->impulse = 1;
	pAmmoItem->SetSpawnedTroughRandomEntity( true );
	pAmmoItem->m_bHasDropped = true;
	pAmmoItem->m_flNextBounceSound = 0.0f; // Allow impact sound on player drop

	// for gpGlobals->v_forward
	UTIL_MakeVectors( pev->v_angle );
	if ( pukevel )
		pAmmoItem->pev->velocity = gpGlobals->v_forward * 325;
	else
		pAmmoItem->pev->velocity = gpGlobals->v_forward * 300;
	pAmmoItem->CheckIfStuckInWorld();

	// Play drop sound
	EMIT_SOUND(ENT(pAmmoItem->pev), CHAN_VOICE, "player/pl_drop_ammo.wav", 1, ATTN_NORM);

	return true;
}

bool CBasePlayer::DropAmmo( int ammoindex, int amount )
{
	UTIL_MakeVectors(pev->angles);
	return DropAmmo( ammoindex, amount, pev->origin + gpGlobals->v_forward, false );
}

void CBasePlayer::ChangeAmmoTypeToDrop()
{
	if ( ZP::GetCurrentRoundState() < ZP::RoundState::RoundState_RoundHasBegun ) return;
	int iAmmo = 0;
	int iStartLocation = m_iAmmoTypeToDrop;
	int iAmmoType = m_iAmmoTypeToDrop;
	bool bHasLooped = false;
	// TODO: Make this less shit.
	// It works just fine, but... blegh...
	do
	{
		// Increase each time
		// If we looped, turn it off and don't increase.
		// Because we want to include index 0 again.
		// double 0 index check, yes i know this sucks and its bad.
		// but I'm too tired right now...
		if ( !bHasLooped )
			iAmmoType++;
		else
			bHasLooped = false;

		// We just looped back, stop.
		if ( iStartLocation == iAmmoType )
		{
			iAmmo = 1;
			break;
		}
		// We need to loop back
		if ( iAmmoType > 4 )
		{
			iAmmoType = 0;
			bHasLooped = true;
		}
		// We simply add 1, as our Ammo array starts at 1.
		// As 0 is "none", to mimic Source Engine.
		iAmmo = m_rgAmmo[ iAmmoType + 1 ];
	} while ( iAmmo <= 0 );
	m_iAmmoTypeToDrop = iAmmoType;
}

void CBasePlayer::DropUnuseableAmmo()
{
	if ( ZP::GetCurrentRoundState() < ZP::RoundState::RoundState_RoundHasBegun ) return;
	if ( ( m_flLastUnusedDrop - gpGlobals->time > 0 ) ) return;
	m_flLastUnusedDrop = gpGlobals->time + 1.0f;

	bool bAmmoThatShouldBeDropped[ZPAmmoTypes::AMMO_MAX];
	for ( int i = 0; i < ZPAmmoTypes::AMMO_MAX; i++ )
		bAmmoThatShouldBeDropped[i] = true;

	// If not in panic, we check our weapons and see what ammo types we have.
	// But if we are in panic, we drop everything. We don't care.
	if ( !IsInPanic() )
	{
		bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
		int iMaxSlots = bIsInHardcore ? MAX_WEAPON_SLOTS_HARDCORE : MAX_WEAPON_SLOTS;
		if ( bIsInHardcore && HasBackpack() )
			iMaxSlots += BACKPACK_EXTRA_SLOTS;
		for ( int i = 0; i < iMaxSlots; i++ )
		{
			CBasePlayerItem *pWeapon = GetWeaponFromSlot( i );
			if ( !pWeapon ) continue;
			ZPAmmoTypes iAmmoIndex = GetAmmoByName( pWeapon->GetData().Ammo1 ).AmmoType;
			if ( iAmmoIndex > ZPAmmoTypes::AMMO_NONE && iAmmoIndex < ZPAmmoTypes::AMMO_MAX )
				bAmmoThatShouldBeDropped[ iAmmoIndex ] = false;
		}
	}

	Vector vUp( 0, 0, -5 );
	for ( int i = 0; i < ZPAmmoTypes::AMMO_MAX; i++ )
	{
		if ( !bAmmoThatShouldBeDropped[i] ) continue;
		if ( m_rgAmmo[ i ] == 0 ) continue;
		UTIL_MakeVectors( pev->angles );
		DropAmmo( i, m_rgAmmo[ i ], pev->origin + gpGlobals->v_forward * 10 + vUp, true );
		vUp.z += 5;
		m_rgAmmo[ i ] = 0;
	}
}

void CBasePlayer::DoPanic()
{
	if ( !IsAlive() ) return;
	if ( ZP::GetCurrentRoundState() < ZP::RoundState::RoundState_RoundHasBegun ) return;
	if ( IsInPanic() ) return;
	if ( !CanSelectNewWeapon( false ) )
	{
		UTIL_SayText( CHAT_FILTER_SERVERMSG, "#ZP_Deny_Panic_WeaponSwitch", this );
		return;
	}
	if ( !CanDropWeapon( true ) ) return;
	if ( !CanPanicSinceLastTime() )
	{
		UTIL_SayText( CHAT_FILTER_SERVERMSG, "#ZP_Deny_Panic", this );
		return;
	}

	CWeaponBase *pActive = dynamic_cast<CWeaponBase *>( m_pActiveItem );
	if ( pActive && !pActive->CanHolster() ) return;

	/*
	if ( RoundJustBegun() )
	{
		UTIL_SayText( "#ZP_Deny_Panic_OnRoundStart", this );
		return;
	}
	*/

	ClientAPIData_t apiData = ZPGameRules()->GetClientAPI( this );

	// If "Switch to melee on panic" is enabled, then we quickly switch weapons. We skip the holstering and all that.
	if ( apiData.PanicToMelee )
		SwitchToMelee();

	m_flLastPanic = gpGlobals->time + 30;
	m_flPanicTime = gpGlobals->time + 3.5;

	// Play it right away
	DoVocalize( PlayerVocalizeType::VOCALIZE_PANIC, true );
	MESSAGE_BEGIN(MSG_ONE, gmsgPanic, NULL, pev);
	MESSAGE_END();
	m_flLastVocalize = gpGlobals->time + 8.0f; // override the time

	// Drop everything in a backpack.
	DropEverything( true );

	// Give our achievements
	GiveAchievement( EAchievements::PANIC_ATTACK );
	GiveAchievement( EAchievements::PANIC_100 );
}

void CBasePlayer::SwitchToMelee()
{
	// If we have an active weapon, make sure we kill it's thinking.
	CWeaponBase *pActive = dynamic_cast<CWeaponBase *>(m_pActiveItem);
	if ( pActive )
	{
		if ( pActive->IsHolstering() )
			pActive->StopHolstering();
	}

	// Let's find a melee weapon!
	bool bIsInHardcore = ( ZP::GetCurrentGameMode()->GetGameModeType() == ZP::GameModeType_e::GAMEMODE_HARDCORE );
	int iMaxSlots = bIsInHardcore ? MAX_WEAPON_SLOTS_HARDCORE : MAX_WEAPON_SLOTS;
	if ( bIsInHardcore && HasBackpack() )
		iMaxSlots += BACKPACK_EXTRA_SLOTS;

	for ( int i = 0; i < iMaxSlots; i++ )
	{
		CBasePlayerWeapon *pWeapon = GetWeaponFromSlot( i );
		if ( !pWeapon ) continue;
		if ( !pWeapon->IsMeleeWeapon() ) continue;
		SelectNewActiveWeapon( pWeapon );
		break;
	}
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasPlayerItem(CBasePlayerItem *pCheckItem)
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs(pItem->pev, STRING(pCheckItem->pev->classname)))
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasNamedPlayerItem(const char *pszItemName)
{
	CBasePlayerItem *pItem;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pItem = m_rgpPlayerItems[i];

		while (pItem)
		{
			if (!strcmp(pszItemName, STRING(pItem->pev->classname)))
			{
				return TRUE;
			}
			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}

//=========================================================
// HasPlayerItemFromID
// Just compare IDs, rather than classnames
//=========================================================
BOOL CBasePlayer::HasPlayerItemFromID(int nID)
{
	CBasePlayerItem *pItem;
	int i;

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		pItem = m_rgpPlayerItems[i];

		while (pItem)
		{
			if (pItem->GetWeaponID() == nID)
			{
				return TRUE;
			}

			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}


//=========================================================
// Player Corpse
//=========================================================
class CPlayerCorpse : public CBaseMonster
{
public:
	virtual int ObjectCaps(void) { return FCAP_DONT_SAVE; }
	void Spawn(void);
	bool InitFromPlayer(CBasePlayer *pPlayer);
	void EXPORT AnimateCorpseThink(void);
	int Classify(void) { return CLASS_HUMAN_MILITARY; }
};

LINK_ENTITY_TO_CLASS(player_corpse, CPlayerCorpse);

void CPlayerCorpse::Spawn(void)
{
	PRECACHE_MODEL("models/player/survivor1/survivor1.mdl");
	SET_MODEL(ENT(pev), "models/player/survivor1/survivor1.mdl");

	pev->effects = 0;
	pev->yaw_speed = 8;
	pev->sequence = 0;
	pev->body = 1;
	m_bloodColor = BLOOD_COLOR_RED;

	// Corpses have less health
	pev->health = 8;

	MonsterInitDead();
}

bool CPlayerCorpse::InitFromPlayer(CBasePlayer *pPlayer)
{
	if (!pPlayer || pPlayer->pev->modelindex == 0 || FBitSet(pPlayer->pev->effects, EF_NODRAW))
		return false;

	const char *pszModel = STRING(pPlayer->pev->model);
	if (!pszModel || !pszModel[0])
		return false;

	SET_MODEL(ENT(pev), pszModel);

	pev->origin = pPlayer->pev->origin;
	pev->angles = pPlayer->pev->angles;
	pev->sequence = pPlayer->pev->sequence;
	pev->frame = pPlayer->pev->frame;
	pev->animtime = pPlayer->pev->animtime;
	pev->framerate = pPlayer->pev->framerate;
	pev->body = pPlayer->pev->body;
	pev->skin = pPlayer->pev->skin;
	pev->colormap = pPlayer->pev->colormap;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_NOT;
	pev->velocity = pPlayer->pev->velocity;
	pev->flags = 0;
	pev->deadflag = DEAD_DEAD;
	pev->takedamage = DAMAGE_NO;
	pev->health = 8;
	pev->effects = pPlayer->pev->effects & ~(EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_NODRAW | EF_NOINTERP);
	m_bloodColor = BLOOD_COLOR_RED;

	InitBoneControllers();
	ResetSequenceInfo();

	for (int i = 0; i < ARRAYSIZE(pev->controller); ++i)
		pev->controller[i] = pPlayer->pev->controller[i];

	for (int i = 0; i < ARRAYSIZE(pev->blending); ++i)
		pev->blending[i] = pPlayer->pev->blending[i];

	pev->frame = pPlayer->pev->frame;
	pev->animtime = pPlayer->pev->animtime;
	pev->framerate = pPlayer->pev->framerate;
	m_fSequenceFinished = FALSE;
	pev->playerclass = PLAYER_CORPSE_ANIM_MARKER;

	UTIL_SetOrigin(pev, pPlayer->pev->origin);
	UTIL_SetSize(pev, pPlayer->pev->mins, pPlayer->pev->maxs);

	SetThink(&CPlayerCorpse::AnimateCorpseThink);
	pev->nextthink = gpGlobals->time + 0.1f;

	return true;
}

void CPlayerCorpse::AnimateCorpseThink(void)
{
	if (!m_fSequenceFinished && pev->framerate > 0)
	{
		StudioFrameAdvance();

		if (m_fSequenceFinished)
		{
			pev->frame = 255;
			StopAnimation();
			pev->effects |= EF_NOINTERP;
		}
	}

	if (pev->movetype != MOVETYPE_NONE && FBitSet(pev->flags, FL_ONGROUND))
	{
		pev->movetype = MOVETYPE_NONE;
		pev->velocity = g_vecZero;
		UTIL_SetOrigin(pev, pev->origin);
	}

	if (m_fSequenceFinished && pev->movetype == MOVETYPE_NONE)
	{
		SetThink(NULL);
		return;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

static bool SpawnAnimatedPlayerCorpse(CBasePlayer *pPlayer)
{
	if (!pPlayer || mp_hidecorpses.GetBool())
		return false;

	CPlayerCorpse *pCorpse = GetClassPtr((CPlayerCorpse *)NULL);
	if (!pCorpse)
		return false;

	if (!pCorpse->InitFromPlayer(pPlayer))
	{
		pCorpse->SUB_Remove();
		return false;
	}

	return true;
}

class CStripWeapons : public CPointEntity
{
public:
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);

private:
};

LINK_ENTITY_TO_CLASS(player_weaponstrip, CStripWeapons);

void CStripWeapons ::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	CBasePlayer *pPlayer = NULL;

	if (pActivator && pActivator->IsPlayer())
	{
		pPlayer = (CBasePlayer *)pActivator;
	}
	else if (!g_pGameRules->IsDeathmatch())
	{
		pPlayer = (CBasePlayer *)CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex(1));
	}

	if (pPlayer)
		pPlayer->RemoveAllItems(FALSE);
}

class CRevertSaved : public CPointEntity
{
public:
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void EXPORT MessageThink(void);
	void EXPORT LoadThink(void);
	void KeyValue(KeyValueData *pkvd);

	virtual int Save(CSave &save);
	virtual int Restore(CRestore &restore);
	static TYPEDESCRIPTION m_SaveData[];

	inline float Duration(void) { return pev->dmg_take; }
	inline float HoldTime(void) { return pev->dmg_save; }
	inline float MessageTime(void) { return m_messageTime; }
	inline float LoadTime(void) { return m_loadTime; }

	inline void SetDuration(float duration) { pev->dmg_take = duration; }
	inline void SetHoldTime(float hold) { pev->dmg_save = hold; }
	inline void SetMessageTime(float time) { m_messageTime = time; }
	inline void SetLoadTime(float time) { m_loadTime = time; }

private:
	float m_messageTime;
	float m_loadTime;
};

LINK_ENTITY_TO_CLASS(player_loadsaved, CRevertSaved);

TYPEDESCRIPTION CRevertSaved::m_SaveData[] = {
	DEFINE_FIELD(CRevertSaved, m_messageTime, FIELD_FLOAT), // These are not actual times, but durations, so save as floats
	DEFINE_FIELD(CRevertSaved, m_loadTime, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CRevertSaved, CPointEntity);

void CRevertSaved ::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration(atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime(atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagetime"))
	{
		SetMessageTime(atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "loadtime"))
	{
		SetLoadTime(atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue(pkvd);
}

void CRevertSaved ::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	UTIL_ScreenFadeAll(pev->rendercolor, Duration(), HoldTime(), pev->renderamt, FFADE_OUT);
	pev->nextthink = gpGlobals->time + MessageTime();
	SetThink(&CRevertSaved::MessageThink);
}

void CRevertSaved ::MessageThink(void)
{
	UTIL_ShowMessageAll(STRING(pev->message));
	float nextThink = LoadTime() - MessageTime();
	if (nextThink > 0)
	{
		pev->nextthink = gpGlobals->time + nextThink;
		SetThink(&CRevertSaved::LoadThink);
	}
	else
		LoadThink();
}

void CRevertSaved ::LoadThink(void)
{
	if (!gpGlobals->deathmatch)
	{
		SERVER_COMMAND("reload\n");
	}
}

//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
	void Spawn(void);
	void Think(void);
};

void CInfoIntermission::Spawn(void)
{
	UTIL_SetOrigin(pev, pev->origin);
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	pev->nextthink = gpGlobals->time + 2; // let targets spawn!
}

void CInfoIntermission::Think(void)
{
	edict_t *pTarget;

	// find my target
	pTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pev->target));

	if (!FNullEnt(pTarget))
	{
		pev->v_angle = UTIL_VecToAngles((pTarget->v.origin - pev->origin).Normalized());
		pev->v_angle.x = -pev->v_angle.x;
	}
}

LINK_ENTITY_TO_CLASS(info_intermission, CInfoIntermission);

//=========================================================
// Player vocalize
//=========================================================

static VocalizeData default_vocalize;

void PrecachePlayerVocalizeSounds()
{
	// Set the default data
	default_vocalize.Character = SURVIVOR1;
	default_vocalize.Type = VOCALIZE_NONE;
	default_vocalize.VoiceLine = "";

	// Clear previous data, if we have any.
	m_VocalizeData.clear();

	// Read the manifest file
	KeyValuesAD kvData( "VocalizeManifest" );
	if ( kvData->LoadFromFile( g_pFullFileSystem, "sound/vo/manifest.txt" ) )
	{
		for ( int i = 0; i < ARRAYSIZE( g_CharacterTypes ); i++ )
		{
			KeyValues *pCharacterData = kvData->FindKey( g_CharacterTypes[i].Name );
			if ( !pCharacterData ) continue;
			for ( KeyValues *sub = pCharacterData->GetFirstSubKey(); sub != NULL ; sub = sub->GetNextKey() )
			{
				int length = Q_strlen( sub->GetString() ) + 1;
				char *strValue = (char *)malloc( length );

				Q_strcpy( strValue, sub->GetString() );

				// Precache the sound
				PRECACHE_SOUND( strValue );

				VocalizeData data;
				data.Character = g_CharacterTypes[i].Type;
				data.VoiceLine = strValue;

				if ( !Q_stricmp( sub->GetName(), "agree" ) ) data.Type = VOCALIZE_AGREE;
				else if ( !Q_stricmp( sub->GetName(), "decline" ) ) data.Type = VOCALIZE_DECLINE;
				else if ( !Q_stricmp( sub->GetName(), "cover_me" ) ) data.Type = VOCALIZE_COVER;
				else if ( !Q_stricmp( sub->GetName(), "need_ammo" ) ) data.Type = VOCALIZE_NEED_AMMO;
				else if ( !Q_stricmp( sub->GetName(), "need_weapon" ) ) data.Type = VOCALIZE_NEED_WEAPON;
				else if ( !Q_stricmp( sub->GetName(), "hold_here" ) ) data.Type = VOCALIZE_HOLD_HERE;
				else if ( !Q_stricmp( sub->GetName(), "open_fire" ) ) data.Type = VOCALIZE_OPEN_FIRE;
				else if ( !Q_stricmp( sub->GetName(), "taunt" ) ) data.Type = VOCALIZE_TAUNT;
				else if ( !Q_stricmp( sub->GetName(), "panic" ) ) data.Type = VOCALIZE_PANIC;
				else if ( !Q_stricmp( sub->GetName(), "on_start" ) ) data.Type = VOCALIZE_AUTO_ONSTART;
				else if ( !Q_stricmp( sub->GetName(), "kill" ) ) data.Type = VOCALIZE_AUTO_KILL;
				else if ( !Q_stricmp( sub->GetName(), "camp" ) ) data.Type = VOCALIZE_AUTO_CAMP;
				else if ( !Q_stricmp( sub->GetName(), "hurt" ) ) data.Type = VOCALIZE_AUTO_HURT;
				else if ( !Q_stricmp( sub->GetName(), "pain" ) ) data.Type = VOCALIZE_AUTO_PAIN;
				else if ( !Q_stricmp( sub->GetName(), "pain_drown" ) ) data.Type = VOCALIZE_AUTO_PAIN_DROWN;
				else if ( !Q_stricmp( sub->GetName(), "death" ) ) data.Type = VOCALIZE_AUTO_DEATH;
				else if ( !Q_stricmp( sub->GetName(), "death_fall" ) ) data.Type = VOCALIZE_AUTO_DEATH_FALL;

				m_VocalizeData.push_back( data );
			}
		}
	}
}

VocalizeData GetVocalizeData( PlayerCharacter nCharacter, PlayerVocalizeType nType )
{
	std::random_device rd;
	std::mt19937 g( rd() );
	std::shuffle( m_VocalizeData.begin(), m_VocalizeData.end(), g );
	for ( size_t i = 0; i < m_VocalizeData.size(); i++ )
	{
		VocalizeData data = m_VocalizeData[ i ];
		if ( data.Character == nCharacter && data.Type == nType )
			return data;
	}
	return default_vocalize;
}
