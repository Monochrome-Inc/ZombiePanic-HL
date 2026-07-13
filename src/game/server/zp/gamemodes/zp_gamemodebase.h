// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SERVER_GAMEMODE_BASE
#define SERVER_GAMEMODE_BASE
#pragma once

#include "cdll_dll.h"
#include "zp/zp_shared.h"

extern int gmsgZombieLives;
extern int gmsgRoundState;
extern int gmsgRoundTime;

class IGameModeBase
{
public:
	enum WinState_e
	{
		State_None,
		State_Draw,
		State_ZombieWin,
		State_SurvivorWin
	};

	virtual ZP::GameModeType_e GetGameModeType() = 0;
	virtual bool IsTestModeActive() const = 0;
	virtual void OnHUDInit(CBasePlayer *pPlayer) = 0;
	virtual void GetZombieLifeData( int &current, int &max ) { current = max = -1; };
	virtual void OnZombieLifeUpdated( bool bIncreased ) { };
	virtual void OnGameModeThink() = 0;
	virtual void RestartRound() = 0;
	virtual void OnPlayerDied(CBasePlayer *pPlayer, entvars_t *pKiller, entvars_t *pInflictor) = 0;
	virtual void OnPlayerSpawned(CBasePlayer *pPlayer) = 0;
	virtual void OnPlayerDisconnected(CBasePlayer *pPlayer) = 0;
	virtual WinState_e GetWinState() = 0;
	virtual void SetWinState(WinState_e state) { }
	virtual void GiveWeapons(CBasePlayer *pPlayer) = 0;

	// Roundstate
	void SetRoundState( ZP::RoundState state );
	ZP::RoundState GetRoundState() const { return m_iRoundState; }
	virtual void OnRoundStateThink( ZP::RoundState state ) {};

	// Was the player already choosen?
	virtual bool WasAlreadyChoosenPreviously( CBasePlayer *pPlayer ) = 0;
	virtual void AddToChoosenList( CBasePlayer *pPlayer ) {}
	virtual void ShouldClearChoosenZombies() = 0;
	virtual void ClearChoosenZombies() = 0;

	// Did the player leave mid round?
	// If true, we will respawn as a zombie if they rejoin back.
	virtual bool HasLeftMidRound( CBasePlayer *pPlayer ) { return false; }

	virtual bool HasTimeRanOut() { return false; }

protected:
	float m_flRoundTime = -1;

private:
	ZP::RoundState m_iRoundState = ZP::RoundState::RoundState_WaitingForPlayers;
};

class CBaseGameMode : public IGameModeBase
{
public:
	virtual bool IsTestModeActive() const;
	virtual void OnHUDInit(CBasePlayer *pPlayer);
	virtual void OnGameModeThink();
	virtual void GiveWeaponsOnRoundStart();
	virtual void RestartRound();
	virtual void CheckZombieAmount();
	virtual void GiveWeapons(CBasePlayer *pPlayer);
	virtual void OnPlayerDied( CBasePlayer *pPlayer, entvars_t *pKiller, entvars_t *pInflictor ) {}
	virtual void OnPlayerSpawned( CBasePlayer *pPlayer );
	virtual void OnPlayerDisconnected( CBasePlayer *pPlayer );
	void SendRoundState();
	void SendRoundState( CBasePlayer *pPlayer );
	void UpdateClientTimer();
	void UpdateClientTimer( CBasePlayer *pPlayer );

	bool HasLeftMidRound( CBasePlayer *pPlayer ) override;
	bool HasTimeRanOut() override { return m_bTimeRanOut; }

protected:
	float m_flRoundBeginsIn;
	float m_flLastZombieCheck;
	bool m_bTimeRanOut = false;
	bool m_bAllSurvivorsDead = false;
	bool m_bHasPlayersConnected = false;

	bool WasAlreadyChoosenPreviously( CBasePlayer *pPlayer );
	void AddToChoosenList( CBasePlayer *pPlayer );
	void ShouldClearChoosenZombies();
	void ClearChoosenZombies();

private:
	std::vector<int> m_LeftMidRoundList;
	struct LastChoosenZombie
	{
		// This is an std::string instead of uint64,
		// since the AuthString in GoldSrc can be different
		std::string AuthID = "";
		int EntIndex = -1;
	};
	std::vector<LastChoosenZombie> m_LastChoosenZombies;
	void RemoveFromList( int entindex );
};

namespace ZP
{
	IGameModeBase *GetCurrentGameMode();
	void SetCurrentGameMode( IGameModeBase *pGameMode );
	ZP::RoundState GetCurrentRoundState();
}

#endif
