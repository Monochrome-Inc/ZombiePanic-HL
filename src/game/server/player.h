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
#ifndef PLAYER_H
#define PLAYER_H

#include "pm_materials.h"
#include "pm_shared.h"
#include "zp/zp_shared.h"

#define PLAYER_FATAL_FALL_SPEED      1024 // approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED   580 // approx 20 feet
#define DAMAGE_FOR_FALL_SPEED        (float)100 / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED) // damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED      200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.
#define PLAYER_MAX_FATIGUE			200

//
// Player PHYSICS FLAGS bits
//
#define PFLAG_ONLADDER   (1 << 0)
#define PFLAG_ONSWING    (1 << 0)
#define PFLAG_ONTRAIN    (1 << 1)
#define PFLAG_ONBARNACLE (1 << 2)
#define PFLAG_DUCKING    (1 << 3) // In the process of ducking, but totally squatted yet
#define PFLAG_USING      (1 << 4) // Using a continuous entity
#define PFLAG_OBSERVER   (1 << 5) // player is locked in stationary cam mode. Spectators can move, observers can't.

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST 4 // max of 4 suit sentences queued up at any time

#define SUIT_GROUP    TRUE
#define SUIT_SENTENCE FALSE

#define SUIT_REPEAT_OK     0
#define SUIT_NEXT_IN_30SEC 30
#define SUIT_NEXT_IN_1MIN  60
#define SUIT_NEXT_IN_5MIN  300
#define SUIT_NEXT_IN_10MIN 600
#define SUIT_NEXT_IN_30MIN 1800
#define SUIT_NEXT_IN_1HOUR 3600

#define CSUITNOREPEAT 32

#define SOUND_FLASHLIGHT_ON  "items/flashlight1.wav"
#define SOUND_FLASHLIGHT_OFF "items/flashlight1.wav"

typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_RELOAD,
	PLAYER_RELOAD_EMPTY,
	PLAYER_RELOAD_START,
	PLAYER_RELOAD_END,
	PLAYER_PUMP,
	PLAYER_ATTACK1,
	PLAYER_ATTACK2_PRE,
	PLAYER_ATTACK2_HOLD,
	PLAYER_ATTACK2_POST,
	PLAYER_DRAW,
	PLAYER_HOLSTER,
	PLAYER_BARRICADE,
	PLAYER_BARRICADE_LONG,
	PLAYER_BARRICADE_NAILGUN,
} PLAYER_ANIM;

#define MAX_ID_RANGE     2048
#define SBAR_STRING_SIZE 128

enum sbar_data
{
	SBAR_ID_TARGETNAME = 1,
	SBAR_ID_TARGETHEALTH,
	SBAR_ID_TARGETARMOR,
	SBAR_END,
};

typedef enum
{
	ANY = -1,
	SURVIVOR1 = 0,		// Eugene
	SURVIVOR2,			// Marcus
	SURVIVOR3,			// David
	MAX_SURVIVORS = SURVIVOR2
} PlayerCharacter;

struct PlayerCharacterType
{
	PlayerCharacter Type;
	const char *Name;
	const char *Survivor;
	const char *Zombie;
};

void PrecachePlayerVocalizeSounds();

struct VocalizeData
{
	PlayerVocalizeType Type;
	PlayerCharacter Character;
	std::string VoiceLine;
};
VocalizeData GetVocalizeData( PlayerCharacter nCharacter, PlayerVocalizeType nType );

#define CHAT_FLOOD          3
#define CHAT_INTERVAL       0.5f
#define CHAT_PENALTY        2.0f
#define FULLUPDATE_INTERVAL 5.0f

class CBasePlayerWeapon;

class CBasePlayer : public CBaseMonster
{
public:

	// Was this player's head blown to bits?
	// Did they get gibbed from a shotgun blast?
	// Fall to their death?
	int m_iDeathFlags;

	// Recently killed an enemy with an explosive
	bool m_bJustKilledWithExplosive;
	float m_flResetExplosiveKillNotice;

	// Observer camera
	EHANDLE m_hObserverTarget;
	float m_flNextObserverInput;
	int m_iObservedWeaponId;
	int m_iObserverMode;

	int random_seed; // See that is shared between client & server for shared weapons code

	int m_iPlayerSound; // the index of the sound list slot reserved for this player
	int m_iTargetVolume; // ideal sound volume.
	int m_iWeaponVolume; // how loud the player's weapon is right now.
	int m_iExtraSoundTypes; // additional classification for this weapon's sound
	int m_iWeaponFlash; // brightness of the weapon flash
	float m_flStopExtraSoundTime;

	float m_flFlashLightTime; // Time until next battery draw/Recharge
	int m_iFlashBattery; // Flashlight Battery Draw

	int m_afButtonLast;
	int m_afButtonPressed;
	int m_afButtonReleased;

	edict_t *m_pentSndLast; // last sound entity to modify player room type
	float m_flSndRoomtype; // last roomtype set by sound entity
	float m_flSndRange; // dist from player to sound entity

	float m_flFallVelocity;

	int m_rgItems[MAX_ITEMS];
	int m_fKnownItem; // True when a new item needs to be added
	int m_fNewAmmo; // True when a new item has been added

	unsigned int m_afPhysicsFlags; // physics flags - set when 'normal' physics should be revisited or overriden
	float m_fNextSuicideTime; // the time after which the player can next use the suicide command

	// these are time-sensitive things that we keep track of
	float m_flTimeStepSound; // when the last stepping sound was made
	float m_flTimeWeaponIdle; // when to play another weapon idle animation.
	float m_flSwimTime; // how long player has been underwater
	float m_flDuckTime; // how long we've been ducking
	float m_flWallJumpTime; // how long until next walljump

	float m_flSuitUpdate; // when to play next suit update
	int m_rgSuitPlayList[CSUITPLAYLIST]; // next sentencenum to play for suit update
	int m_iSuitPlayNext; // next sentence slot for queue storage;
	int m_rgiSuitNoRepeat[CSUITNOREPEAT]; // suit sentence no repeat list
	float m_rgflSuitNoRepeatTime[CSUITNOREPEAT]; // how long to wait before allowing repeat
	int m_lastDamageAmount; // Last damage taken
	float m_tbdPrev; // Time-based damage timer
#if 0
	float m_flgeigerRange; // range to nearest radiation source
	float m_flgeigerDelay; // delay per update of range msg to client
	int m_igeigerRangePrev;
#endif
	int m_iStepLeft; // alternate left/right foot stepping sound
	char m_szTextureName[CBTEXTURENAMEMAX]; // current texture name we're standing on
	char m_chTextureType; // current texture type

	int m_idrowndmg; // track drowning damage taken
	int m_idrownrestored; // track drowning damage restored

	int m_bitsHUDDamage; // Damage bits for the current fame. These get sent to
	    // the hude via the DAMAGE message
	BOOL m_fInitHUD; // True when deferred HUD restart msg needs to be sent
	BOOL m_fGameHUDInitialized;
	int m_iTrain; // Train control position
	BOOL m_fWeapon; // Set this to FALSE to force a reset of the current weapon HUD info

	EHANDLE m_pTank; // the tank which the player is currently controlling,  NULL if no tank
	float m_fDeadTime; // the time at which the player died  (used in PlayerDeathThink())

	BOOL m_fNoPlayerSound; // a debugging feature. Player makes no sound if this is true.
	BOOL m_fLongJump; // does this player have the longjump module?

	float m_tSneaking;
	int m_iUpdateTime; // stores the number of frame ticks before sending HUD update messages
	int m_iClientHealth; // the health currently known by the client.  If this changes, send a new
	int m_iClientBattery; // the Battery currently known by the client.  If this changes, send a new
	int m_iHideHUD; // the players hud weapon info is to be hidden
	int m_iClientHideHUD;
	int m_iFOV; // field of view
	int m_iClientFOV; // client's known FOV
	// usable player items
	CBasePlayerItem *m_rgpPlayerItems[MAX_ITEM_TYPES];
	CBasePlayerItem *m_pActiveItem;
	CBasePlayerItem *m_pClientActiveItem; // client version of the active item
	CBasePlayerItem *m_pLastItem;
	// shared ammo slots
	int m_rgAmmo[ZPAmmoTypes::AMMO_MAX];
	int m_rgAmmoLast[ZPAmmoTypes::AMMO_MAX];

	Vector m_vecAutoAim;
	BOOL m_fOnTarget;
	int m_iDeaths;
	float m_flRespawnTimer; // used in PlayerDeathThink() to make sure players can always respawn
	float m_flDeathAnimationStartTime; // used in PlayerDeathThink() to make sure players can always respawn
	bool m_bNoLives;

	int m_lastx, m_lasty; // These are the previous update's crosshair angles, DON"T SAVE/RESTORE

	bool m_bSpawnInPlace = false;
	int m_nCustomSprayFrames; // Custom clan logo frames for this player
	float m_flNextDecalTime; // next time this player can spray a decal

	void SpawnInPlace( bool state ) { m_bSpawnInPlace = state; }
	virtual void Spawn(void);
	void Pain(bool bDrown);

	void SetPlayerHullSize();

	//	virtual void Think( void );
	virtual void Jump(void);
	virtual void Duck(void);
	virtual void PreThink(void);
	virtual void PostThink(void);
	virtual float GetAmmoWeight( const char *szAmmo );
	virtual float GetTotalWeight(void);
	virtual void UpdatePlayerMaxSpeed();
	virtual Vector GetGunPosition(void);
	virtual int TakeHealth(float flHealth, int bitsDamageType);
	virtual void TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	virtual void Killed(entvars_t *pevAttacker, int iGib);
	virtual Vector BodyTarget(const Vector &posSrc) { return Center() + pev->view_ofs * RANDOM_FLOAT(0.5, 1.1); }; // position to shoot at
	virtual void StartSneaking(void) { m_tSneaking = gpGlobals->time - 1; }
	virtual void StopSneaking(void) { m_tSneaking = gpGlobals->time + 30; }
	virtual BOOL IsSneaking(void) { return m_tSneaking <= gpGlobals->time; }
	virtual BOOL IsAlive(void) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual BOOL ShouldFadeOnDeath(void) { return FALSE; }
	virtual BOOL IsPlayer(void) { return TRUE; } // Spectators should return FALSE for this, they aren't "players" as far as game logic is concerned

	virtual BOOL IsNetClient(void) { return TRUE; } // Bots should return FALSE for this, they can't receive NET messages
	// Spectators should return TRUE for this
	virtual const char *TeamID(void);

	virtual int Save(CSave &save);
	virtual int Restore(CRestore &restore);
	void RenewItems(void);
	void PackDeadPlayerItems(void);
	void RemoveAllItems(BOOL removeSuit);

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void UpdateClientData(void);

	void SetPrefsFromUserinfo(char *infobuffer);

	static TYPEDESCRIPTION m_playerSaveData[];

	// Player is moved across the transition by other means
	virtual int ObjectCaps(void) { return CBaseMonster ::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void Precache(void);
	BOOL IsOnLadder(void);
	BOOL FlashlightIsOn(void);
	void FlashlightTurnOn(void);
	void FlashlightTurnOff(void);

	void UpdatePlayerSound(void);
	void DeathSound(void);

	int Classify(void);
	bool CanActiveInteruptAnimation();
	void EXPORT SetAnimation(PLAYER_ANIM playerAnim);
	void SetWeaponAnimType(const char *szExtention);
	int SetNewActivity(const char *szActivity, bool bUseExt);
	char m_szAnimExtention[32];

	void OnPlayerReload();

	// custom player functions
	virtual void ImpulseCommands(void);
	void CheatImpulseCommands(int iImpulse);

	void StartDeathCam(void);
	void StartObserver(void);
	void StopObserver(void);
	void Observer_FindNextPlayer(bool bReverse, bool bOverview);
	void Observer_FindNextSpot(bool bReverse);
	void Observer_HandleButtons();
	void Observer_SetMode(int iMode);
	void Observer_CheckTarget();
	int IsObserver() { return pev->iuser1; };

	void AddPoints(int score, BOOL bAllowNegativeScore);
	void AddPointsToTeam(int score, BOOL bAllowNegativeScore);
	bool AlreadyOwnWeapon(CBasePlayerItem *pWeapon);
	BOOL AddPlayerItem(CBasePlayerItem *pItem);
	BOOL RemovePlayerItem(CBasePlayerItem *pItem);
	void DropPlayerItem(char *pszItemName);
	void DropActiveWeapon();
	void DropSelectedAmmo();
	bool DropAmmo( int ammoindex, int amount );
	bool DropAmmo( int ammoindex, int amount, Vector Dir, bool pukevel );
	void ChangeAmmoTypeToDrop();
	void DropUnuseableAmmo();
	void DoPanic();
	void SwitchToMelee();
	int AmmoIndexToDrop( int ammoindex = -1 );
	int AmmoIndexToDropArray( int ammoindex );
	int DefaultAmmoToDrop( int ammoindex );
	const char *szAmmoToDropClassnames( int ammoindex );
	BOOL HasPlayerItem(CBasePlayerItem *pCheckItem);
	BOOL HasNamedPlayerItem(const char *pszItemName);
	BOOL HasPlayerItemFromID(int nID);
	BOOL HasWeapons(void); // do I have ANY weapons?
	void SelectPrevItem(int iItem);
	void SelectNextItem(int iItem);
	void SelectLastItem(void);
	void SelectItem(const char *pstr);
	void ItemPreFrame(void);
	void ItemPostFrame(void);
	void EXPORT GiveNamedItem(const char *szName);
	void EnableControl(BOOL fControl);

	int PickupAmmo(int iAmount, AmmoData data);
	bool CanGiveAmmo(int iAmount, AmmoData data);

	int GiveAmmo(int iAmount, ZPAmmoTypes ammotype );
	int GiveAmmo(int iAmount, char *szName);
	void SendAmmoUpdate(CBasePlayer *pPlayer);

	void WaterMove(void);
	void EXPORT PlayerDeathThink(void);
	void PlayerUse(void);

	void CheckSuitUpdate();
	void SetSuitUpdate(char *name, int fgroup, int iNoRepeat);
#if 0
	void UpdateGeigerCounter(void);
#endif
	void CheckTimeBasedDamage(void);

	BOOL FBecomeProne(void);
	void BarnacleVictimBitten(entvars_t *pevBarnacle);
	void BarnacleVictimReleased(void);
	static int EXPORT GetAmmoIndex(const char *psz);
	int AmmoInventory(int iAmmoIndex);
	int Illumination(void);

	void ResetAutoaim(void);
	Vector GetAutoaimVector(float flDelta);
	Vector AutoaimDeflection(Vector &vecSrc, float flDist, float flDelta);

	void ForceClientDllUpdate(void); // Forces all client .dll specific data to be resent to client.

	void DeathMessage(entvars_t *pevKiller);

	void SetCustomDecalFrames(int nFrames);
	int GetCustomDecalFrames(void);

	void TabulateAmmo(void);

	float m_flStartCharge;
	float m_flAmmoStartCharge;
	float m_flPlayAftershock;
	float m_flNextAmmoBurn; // while charging, when to absorb another unit of player's ammo?

	enum ThrowableDropState
	{
		NOT_ACTIVE = 0,
		NOT_ACTIVE_THROWABLE,
		IS_ACTIVE,
		DELETE_ITEM,
		DELETE_ITEM_AND_ACTIVE,
	};
	ThrowableDropState IsThrowableAndActive(CBasePlayerWeapon *pWeapon, bool bOnDrop);

	// Kill assist stuff
	int GetBestKillAssist(int iAttacker);
	void AddToAssistDamage(CBasePlayer *pPlayer, float flDamage);
	struct KillAssist
	{
		int EntIndex;
		float DamageDealt;
	};
	std::vector<KillAssist> m_AssistedDamage;

	//Player ID
	void InitStatusBar(void);
	void UpdateStatusBar(void);
	int m_izSBarState[SBAR_END];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	char m_SbarString0[SBAR_STRING_SIZE];
	char m_SbarString1[SBAR_STRING_SIZE];

	int m_iChatFlood;
	float m_flNextChatTime;
	int m_iAutoWepSwitch;

	//-----------------------------------------------------
	// Zombie Panic! related stuff for weapon & ammo drop crap
	//-----------------------------------------------------
	float m_flCanSuicide;
	float m_flSuicideTimer;
	float m_flLastWeaponDrop;
	float m_flLastAmmoDrop;
	float m_flLastPanic;
	float m_flPanicTime;
	float m_flLastUnusedDrop;
	float m_flLastPlayerIdleAudio;
	bool m_bFallingToMyDeath;
	int m_iAmmoTypeToDrop;
	int m_iClientAmmoType;

	bool IsInPanic();
	bool CanPanicSinceLastTime();
	void WantsToSuicide();
	void JustKilledWithExplosives();

	bool m_bInZombieVision;
	bool m_bJustSpawned;

#ifdef SERVER_DLL
private:
	float m_flBloodLoss;
	float m_flImHurtDelay;
	float m_flLastVocalize;
	float m_flLastFatigue = -1;
	bool m_bFatigueUpdated = false;

	float m_flLastRegen = -1;
	float m_flRegenTime = 0;
	bool m_bRegenUpdated = false;
	// Used by trigger_endround and zp_gamerules to check if a survivor has escaped
	bool m_bHasEscaped = false;
	float m_flRefuseWeaponAudioCalls;
	// Used by hardcore
	bool m_bIsBleeding = false;
	bool m_bGotBandage = false;
	bool m_bGotPainkiller = false;
	float m_flPainPillHealthDelay;
	int m_iPillAmount;
	int m_iPillsTaken;
	int m_iBleedHit[4]; // Track who we hit.
	char m_szAPIRetrieveKey[64]; // Key to retrieve API data.
	bool m_bAPIOK = false; // Was the retrieved API data OK?

	void DoHeadshotChunk( const Vector &vecPos, short modelIndex, int iAmount, int iScale );
public:
	CBaseEntity *FindUseEntity(); // Backported from Source SDK 2013
	CBaseEntity *GetUseEntityFromCrosshair( bool bUseableOnly );
	CBaseEntity *GetUseEntitiesFromSphere( float flDist );

	virtual bool IsBot( void ) { return false; }

	void GiveCurrentAmmo();
	const char *GetPlayerName() const;
	int GetTeamNumber() const;

	const char *GetAPIRetrieveKey() const { return m_szAPIRetrieveKey; }
	void ClearAPIRetrieveKey() { m_szAPIRetrieveKey[0] = '\0'; SetAPIPassed( false ); }
	void SetAPIPassed( const bool &bState ) { m_bAPIOK = bState; }
	bool GetAPIPassed() const { return m_bAPIOK; }

	void IncreaseBleed( int iIndex );
	void DoHeadshotBlood( const Vector &vecPos, int iAmount );
	void DoHeadshotExploded( const Vector &vecPos );

	inline void RefuseWeaponAudioCalls( float flDelay ) { m_flRefuseWeaponAudioCalls = gpGlobals->time + flDelay; }

	// Has this survivor escaped?
	bool HasEscaped() { return m_bHasEscaped; }
	// Sets the escaped state
	void SetHasEscaped(bool state) { m_bHasEscaped = state; }

	// Increases the fatigue.
	void IncreaseFatigue( float flValue )
	{
		pev->fuser4 = clamp( pev->fuser4 + flValue, 0, PLAYER_MAX_FATIGUE );
		m_bFatigueUpdated = true;
	}

	// Updates our fatigue for the player, this is a super
	// simplified version of the ZPS version. Mostly used when
	// jumping, since the jump delay was not really fun.
	void UpdateFatigue();

	// Max health for our client
	int GetMaxHealth();

	// Zombie health regen
	void UpdateHealthRegen();

	// Fires decals at the ground
	void DoBloodLoss();
	void DoBloodLossDecal( float flDelay );

	void DoScreenTint( bool bDamage );

	bool GotBandage( bool bGiveHealth );
	bool GotPainKiller();

	// Give achivement
	void GiveAchievement( EAchievements eAchivement );
	void NotifyOfEarnedAchivement( int eAchivement );
	void GiveAchievementsFromKill( CBasePlayer *pKiller, const char *szWeapon );

	// Punish the late joiner.
	bool m_bPunishLateJoiner = false;

	// Buddha mode
	bool m_bBuddhaMode = false;

	// Apply our correct model!
	void SetTheCorrectPlayerModel();
	PlayerCharacter GetCharacter( const char *szType );
	PlayerCharacterType GetPlayerCharacterType();

	// 0 - Normal kills
	// 1 - headshots
	int m_iKillsThisLife[2] = { 0, 0 };
	int m_iWeaponKillCount = 0;
	PlayerCharacter m_iCharacter = PlayerCharacter::ANY;

	// Achievement trigger
	void InAchievementTrigger( string_t iszAchievementID );
	float m_flLastKillCheck = 0; // When we last checked for a kill (for achievements)

	// Achievement participation tracking.
	// Not all achievements use this, only the ones that need to track.
	// And since it's an array, we can easily add more achievements that need this.
	void Participated( EAchievements eAchivement ) { m_bParticipated[eAchivement] = true; }
	bool HasParticipated( EAchievements eAchivement ) const { return m_bParticipated[eAchivement]; }
	void ResetParticipation() { memset( m_bParticipated, 0, sizeof(m_bParticipated) ); }
	bool m_bParticipated[ACHV_MAX] = { false };

	// Select weapon from slot (1-5)
	// This is used for the weapon selection menu (brought up by pressing the weapon slot keys)
	// This is also used by the client when they press the weapon slot keys
	// This replaces the old HL1 client side weapon selection code
	void SelectWeaponFromSlot( int iSlot );
	void SelectNextSlot();
	void SelectPreviousSlot();
	CBasePlayerWeapon *GetWeaponFromSlot( int iSlot );
	bool HasAvailableWeaponSlots( bool bIsDoubleSlot );
	int GetBestSlotPosition( bool bIsDoubleSlot );

	bool CanSelectNewWeapon( bool bPrintMsg );
	bool CanDropWeapon( bool bPrintMsg );

	void SelectWeapon( CBasePlayerWeapon *pWeapon );

	void SelectNewActiveWeapon( CBasePlayerWeapon *pWeapon );
	void WeaponSlotSet( CBasePlayerWeapon *pWeapon, bool bState );
	bool WeaponSlots[MAX_WEAPON_SLOTS] = { false };

	// Drop all weapons and ammo, except the primary weapon.
	// It's very similar to Zombie Panic Source's panic drop.
	void DropEverything( bool bDontDropPrimary );
	void DropWeapon( CBasePlayerWeapon *pWeapon, bool bAutoSwitch, bool pukevel = false );

	void SetBackpackState( bool bState );
	inline bool HasBackpack() const { return GetWeaponOwn( ZPWeaponID::WEAPON_BACKPACK ); }

	// Get an available weapon from our weapon ID
	CBasePlayerWeapon *GetWeaponFromID( int iID );
	void NotifyOfWeaponPickup( CBasePlayerWeapon *pWeapon );

	// The player will now speak
	void DoVocalize( PlayerVocalizeType nType, bool bForced = false, bool bNoChatLine = false );

	// When we can next switch weapon
	float m_flNextWeaponSwitch;

	// What weapons do we own?
	bool m_bOwnWeaponID[ ZPWeaponID::LAST_WEAPON_ID ];
	inline void ClearWeaponOwn( const bool &bKeepSuit )
	{
		pev->weapons = 0;
		for ( int i = 0; i < ZPWeaponID::LAST_WEAPON_ID; i++ )
			m_bOwnWeaponID[ i ] = false;
		if ( bKeepSuit )
		{
			pev->weapons |= (1 << ZPWeaponID::WEAPON_SUIT);
			m_bOwnWeaponID[ ZPWeaponID::WEAPON_SUIT ] = true;
		}
	}
	inline void SetWeaponOwn( ZPWeaponID nID, const bool &state )
	{
		m_bOwnWeaponID[ nID ] = state;
		SendWeaponOwnUpdate( nID, state );
	}
	inline bool GetWeaponOwn( ZPWeaponID nID ) const { return m_bOwnWeaponID[ nID ]; }
	void SendWeaponOwnUpdate( ZPWeaponID nID, const bool &state );

#endif

	//-----------------------------------------------------
	// BEGIN BUGFIXED HL FIELDS
	//-----------------------------------------------------

	float m_flNextSpectatorCommand;
	float m_flNextFullupdate[2];

	BOOL m_bConnected; // we set it in Spawn() so it will be TRUE only after player was spawned
	BOOL m_bPutInServer; // we set it after PutInServer finished
	BOOL m_bIsBot; // we set it at PutInServer start
	BOOL IsConnected() { return m_bConnected; }
	void Disconnect()
	{
		m_bConnected = FALSE;
		m_bPutInServer = FALSE;
		m_bIsBot = FALSE;
		m_bInWelcomeCam = FALSE;
	}

	Vector m_vecLastViewAngles;

	BOOL m_bInWelcomeCam;
	void StartWelcomeCam(void);
	void StopWelcomeCam(void);

	/**
	 * Sends a message to all clients with score of this player.
	 * Should be called any time kills/deaths are updated.
	 */
	void SendScoreInfo();
};

#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669

extern int gmsgHudText;
extern BOOL gInitHUD;

void PrecachePlayerModels();
bool UTIL_IsValidPlayerModel( const char *szModel, bool bIsZombie );

#endif // PLAYER_H
