// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "convar.h"
#include "gamerules.h"

#ifdef SCRIPT_SYSTEM
#include "core.h"
#endif

#include "zp/info_beacon.h"
#include "zp/info_random_base.h"
#include "zp/zp_gamerules.h"
#include "zp_gamemodebase.h"

// for std::vector random_shuffle
#include <iterator>
#include <random>
#include <algorithm>

static IGameModeBase *s_GameModeBase = nullptr;
extern ConVar sv_fafo_only;

namespace
{
bool ShouldRoundLockPlayerInput( CBasePlayer *pPlayer, ZP::RoundState state )
{
	if ( !pPlayer ) return false;
	if ( !pPlayer->IsConnected() ) return false;
	if ( !pPlayer->IsAlive() ) return false;
	if ( pPlayer->IsObserver() ) return false;
	if ( pPlayer->pev->team == ZP::TEAM_OBSERVER ) return false;
	return state != ZP::RoundState::RoundState_RoundHasBegun;
}

void UpdateRoundInputLockForPlayer( CBasePlayer *pPlayer, ZP::RoundState state )
{
	if ( !pPlayer ) return;
	g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "zprl", ShouldRoundLockPlayerInput( pPlayer, state ) ? "1" : "0" );
}

void UpdateRoundInputLockForAllPlayers( ZP::RoundState state )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer ) continue;
		UpdateRoundInputLockForPlayer( pPlayer, state );
	}
}
}

void IGameModeBase::SetRoundState( ZP::RoundState state )
{
	m_iRoundState = state;
	UpdateRoundInputLockForAllPlayers( state );
}

IGameModeBase *ZP::GetCurrentGameMode()
{
	return s_GameModeBase;
}

void ZP::SetCurrentGameMode( IGameModeBase *pGameMode )
{
	if ( s_GameModeBase )
		delete s_GameModeBase;
	s_GameModeBase = pGameMode;
}

ZP::RoundState ZP::GetCurrentRoundState()
{
	IGameModeBase *pGameMode = GetCurrentGameMode();
	if ( !pGameMode ) return ZP::RoundState_Invalid;
	return pGameMode->GetRoundState();
}

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

extern cvar_t roundtime;
extern cvar_t startdelay;
extern int gmsgTeamInfo;

bool CBaseGameMode::IsTestModeActive() const
{
	return sv_testmode.GetBool();
}

void CBaseGameMode::SendRoundState()
{
	const int winner = ( GetRoundState() == ZP::RoundState_RoundIsOver ) ? GetWinState() : IGameModeBase::State_None;

	MESSAGE_BEGIN( MSG_ALL, gmsgRoundState );
	WRITE_SHORT( GetRoundState() );
	WRITE_SHORT( winner );
	MESSAGE_END();
}

void CBaseGameMode::SendRoundState( CBasePlayer *pPlayer )
{
	if ( !pPlayer ) return;
	if ( !pPlayer->IsConnected() ) return;

	const int winner = ( GetRoundState() == ZP::RoundState_RoundIsOver ) ? GetWinState() : IGameModeBase::State_None;

	MESSAGE_BEGIN( MSG_ONE, gmsgRoundState, NULL, pPlayer->edict() );
	WRITE_SHORT( GetRoundState() );
	WRITE_SHORT( winner );
	MESSAGE_END();
}

void CBaseGameMode::OnHUDInit(CBasePlayer *pPlayer)
{
	if ( !m_bHasPlayersConnected )
		m_bHasPlayersConnected = true;
}

void CBaseGameMode::OnGameModeThink()
{
	// Nobody has connected, don't do any thinking.
	if ( !m_bHasPlayersConnected ) return;

	// Check who is alive (survivors only)
	bool bHasSomeoneAlive = false;
	int iSurvivorsFound = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsConnected() )
		{
			int iTeam = plr->pev->team;
			if ( iTeam == ZP::TEAM_SURVIVIOR && plr->IsAlive() && !plr->IsObserver() )
			{
				if ( GetRoundState() == ZP::RoundState_WaitingForPlayers )
					iSurvivorsFound++;
				else if ( !bHasSomeoneAlive )
					bHasSomeoneAlive = true;
			}
		}
	}

	switch ( GetRoundState() )
	{
		case ZP::RoundState_WaitingForPlayers:
		{
			if ( iSurvivorsFound >= 2 || IsTestModeActive() )
			{
			    m_flRoundBeginsIn = gpGlobals->time + startdelay.value;
				SetRoundState( ZP::RoundState_RoundIsStarting );
				SendRoundState();
			}
		}
		break;

		case ZP::RoundState_RoundIsStarting:
		{
			float flRoundBeginsIn = m_flRoundBeginsIn - gpGlobals->time;
			if ( flRoundBeginsIn <= 0 )
			{
			    m_flRoundBeginsIn = gpGlobals->time + 0.1f;
				SetRoundState( ZP::RoundState_PickVolunteers );
			}
		}
	    break;

		case ZP::RoundState_PickVolunteers:
		{
			float flRoundBeginsIn = m_flRoundBeginsIn - gpGlobals->time;
			if ( flRoundBeginsIn <= 0 )
			{
			    m_flRoundBeginsIn = 0;
				SetRoundState( ZP::RoundState_RoundHasBegunPost );
			}
		}
	    break;

		case ZP::RoundState_RoundHasBegunPost:
	    {
		    m_flRoundTime = roundtime.value > 0 ? gpGlobals->time + roundtime.value * 60 : -1;
		    m_flLastZombieCheck = gpGlobals->time + 4.5f;
			SetRoundState( ZP::RoundState_RoundHasBegun );
		    SendRoundState();
		    GiveWeaponsOnRoundStart();
		    ZP::OnGameModeRoundStart();
			// Tell the clients that we have a new round timer!
			UpdateClientTimer();
#ifdef SCRIPT_SYSTEM
			ScriptSystem::CallScript(
				AvailableScripts_t::InputOutput,
				nullptr,
				"OnRoundStart",
				1,
				IO_FUNCTION
			);
#endif
		}
	    break;
	}

	// Function to fire and used by gamemodes
	OnRoundStateThink( GetRoundState() );

	// If the round has not yet begun, we do not care about the timer
	if ( GetRoundState() != ZP::RoundState_RoundHasBegun ) return;

	// Sorry pal, we don't care if everyone is dead, or the timeleft inf testmode.
	if ( IsTestModeActive() ) return;

	CheckZombieAmount();

	// Game time is over, go to intermission
	if ( m_flRoundTime != -1 && gpGlobals->time >= m_flRoundTime )
		m_bTimeRanOut = true;

	// If we did not find anyone, then they may be all be dead (or disconnected)
	if ( !bHasSomeoneAlive )
		m_bAllSurvivorsDead = true;
}

void CBaseGameMode::GiveWeaponsOnRoundStart()
{
	std::vector<int> m_Survivors;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsAlive() )
		{
			int iTeam = plr->pev->team;
			if ( iTeam == ZP::TEAM_SURVIVIOR )
			{
				GiveWeapons( plr );
				m_Survivors.push_back( i );
			}
			plr->m_iHideHUD = 0;
			plr->m_flCanSuicide = gpGlobals->time + 20.0f;
			plr->m_flSuicideTimer = -1; // Just in case if the player manages to frame perfect a "kill" command.
			// Zombies can't choose weapons, they only got their arms.
			if ( iTeam == ZP::TEAM_ZOMBIE )
				plr->m_iHideHUD |= HIDEHUD_WEAPONS;
		}
	}

	// Pick a player, if possible, and make them play "on start" voice line
	if ( m_Survivors.size() == 0 ) return;
	std::random_device rd;
	std::mt19937 g( rd() );
	std::shuffle( m_Survivors.begin(), m_Survivors.end(), g );
	CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( m_Survivors[ RandomInt( 0, m_Survivors.size() - 1 ) ] );
	if ( plr && plr->IsAlive() )
		plr->DoVocalize( PlayerVocalizeType::VOCALIZE_AUTO_ONSTART, true );
}

void CBaseGameMode::RestartRound()
{
#ifdef SCRIPT_SYSTEM
	ScriptSystem::OnRoundRestart();
#endif
	SetRoundState( ZP::RoundState::RoundState_WaitingForPlayers );
	SetWinState( WinState_e::State_None );
	m_bAllSurvivorsDead = false;
	m_bTimeRanOut = false;
	// New round, clear it.
	m_LeftMidRoundList.clear();
	SendRoundState();
	m_flRoundTime = -1;
	UpdateClientTimer();
}

void CBaseGameMode::CheckZombieAmount()
{
	if ( m_flLastZombieCheck - gpGlobals->time > 0 ) return;
	m_flLastZombieCheck = gpGlobals->time + 3.0f;
	int iZombies = 0;
	std::vector<int> m_Volunteers;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr )
		{
			int iTeam = plr->pev->team;
			if ( iTeam == ZP::TEAM_ZOMBIE )
				iZombies++;
			else if ( iTeam == ZP::TEAM_SURVIVIOR )
				m_Volunteers.push_back( plr->entindex() );
		}
	}
	// Check if we have any zombies
	if ( iZombies > 0 ) return;
	// We got no zombies? Pick a random survivor to become a zombie on the spot.
	int iVolunteerIndex = 0;
	int iVolunteers = m_Volunteers.size() - 1;
	if ( iVolunteers > 0 )
		iVolunteerIndex = RANDOM_LONG( 0, iVolunteers );
	int iPlayerIndex = m_Volunteers[iVolunteerIndex];

	if ( iPlayerIndex == 0 ) return;
	m_Volunteers.erase( m_Volunteers.begin() + iVolunteerIndex );
	CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( iPlayerIndex );
	if ( plr )
	{
		// Drop everything in a backpack.
		plr->m_flLastPanic = gpGlobals->time + 30;
		plr->m_flPanicTime = gpGlobals->time + 3.5;
		plr->DropEverything( false );
		plr->RemoveAllItems( FALSE );

		// Change team
		plr->pev->team = ZP::TEAM_ZOMBIE;
		if ( ZPGameRules() )
			ZPGameRules()->SendPlayerTeamInfo( plr );
		else
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( iPlayerIndex );
			WRITE_STRING( plr->pev->iuser1 ? "" : plr->TeamID() );
			MESSAGE_END();
			plr->SendScoreInfo();
		}
		plr->SpawnInPlace( true );
		plr->Spawn();
		plr->GiveAchievement( ONE_OF_US );
		plr->m_iWeaponKillCount = 0;
	}
}

void CBaseGameMode::GiveWeapons( CBasePlayer *pPlayer )
{
	CBaseEntity *pWeaponEntity = NULL;
	BOOL addDefault = TRUE;

	while (pWeaponEntity = UTIL_FindEntityByClassname(pWeaponEntity, pPlayer->m_bPunishLateJoiner ? "game_player_equip_punish" : "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( addDefault )
	{
		if ( !pPlayer->m_bPunishLateJoiner )
		{
			int randWep = RANDOM_LONG(0, 1);
			switch (randWep)
			{
				case 0: pPlayer->GiveNamedItem( "weapon_crowbar" ); break;
				case 1: pPlayer->GiveNamedItem( "weapon_leadpipe" ); break;
			}
			if ( sv_fafo_only.GetBool() )
				pPlayer->GiveNamedItem( "weapon_fafo" );
			else
			{
				pPlayer->GiveNamedItem( "weapon_sig" );
				pPlayer->GiveAmmo( 14, ZPAmmoTypes::AMMO_PISTOL );
			}
		}
		else
			pPlayer->GiveNamedItem( "weapon_walkstick" );
		pPlayer->m_bPunishLateJoiner = false;
	}
}

void CBaseGameMode::OnPlayerSpawned( CBasePlayer *pPlayer )
{
	// Tell the clients that we have a new round timer!
	UpdateClientTimer( pPlayer );

#ifdef SCRIPT_SYSTEM
	ScriptSystem::CallScript(
		AvailableScripts_t::InputOutput,
		nullptr,
		"OnPlayerSpawned",
		2,
	    IO_FUNCTION,
	    std::to_string( pPlayer->entindex() )
	);
#endif

	// Player spawned, check if we need to show any info_beacon entities
	// We need to do this here, because a new player may have joined mid-round
	if ( GetRoundState() != ZP::RoundState_RoundHasBegun ) return;
	CInfoBeacon *pBeacon = (CInfoBeacon *)UTIL_FindEntityByClassname( nullptr, "info_beacon" );
	while ( pBeacon )
	{
		// Make sure we only start those with SP_START_ENABLED flag set
		if ( pBeacon->IsActive() )
			pBeacon->UpdateMessageStateForEntity( pPlayer );
		pBeacon = (CInfoBeacon *)UTIL_FindEntityByClassname( pBeacon, "info_beacon" );
	}
}

void CBaseGameMode::OnPlayerDisconnected(CBasePlayer *pPlayer)
{
	// We don't want the list to freak out, just delete this index.
	RemoveFromList( pPlayer->entindex() );
	// Player left mid round, add them to the list!!
	if ( GetRoundState() == ZP::RoundState_RoundHasBegun )
		m_LeftMidRoundList.push_back( pPlayer->entindex() );
}

void CBaseGameMode::UpdateClientTimer()
{
	float flTimeLimit = CVAR_GET_FLOAT( "mp_timelimit" ) * 60;
	int time_remaining = (int)(flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);
	MESSAGE_BEGIN( MSG_ALL, gmsgRoundTime );
	WRITE_FLOAT( m_flRoundTime );
	WRITE_SHORT( time_remaining );
	MESSAGE_END();
}

void CBaseGameMode::UpdateClientTimer( CBasePlayer *pPlayer )
{
	if ( !pPlayer ) return;
	if ( !pPlayer->IsConnected() ) return;

	float flTimeLimit = CVAR_GET_FLOAT( "mp_timelimit" ) * 60;
	int time_remaining = (int)(flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);
	MESSAGE_BEGIN( MSG_ONE, gmsgRoundTime, NULL, pPlayer->edict() );
	WRITE_FLOAT( m_flRoundTime );
	WRITE_SHORT( time_remaining );
	MESSAGE_END();
}

bool CBaseGameMode::HasLeftMidRound( CBasePlayer *pPlayer )
{
	for ( size_t i = 0; i < m_LeftMidRoundList.size(); i++ )
	{
		if ( m_LeftMidRoundList[i] == pPlayer->entindex() )
			return true;
	}
	return false;
}

bool CBaseGameMode::WasAlreadyChoosenPreviously( CBasePlayer *pPlayer )
{
	const char *authId = GETPLAYERAUTHID( pPlayer->edict() );
	if ( authId )
	{
		for ( size_t i = 0; i < m_LastChoosenZombies.size(); i++ )
		{
			LastChoosenZombie choosen = m_LastChoosenZombies[i];
			if ( FStrEq( choosen.AuthID.c_str(), authId ) )
				return true;
		}
	}
	return false;
}

void CBaseGameMode::AddToChoosenList( CBasePlayer *pPlayer )
{
	const char *authId = GETPLAYERAUTHID( pPlayer->edict() );
	if ( !authId ) return;
	LastChoosenZombie choosen;
	choosen.AuthID = authId;
	m_LastChoosenZombies.push_back( choosen );
}

void CBaseGameMode::ShouldClearChoosenZombies()
{
	// How many players do we have? If everyone was choosen, purge the list.
	int iAmountChoosen = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsConnected() && ( !plr->IsObserver() || plr->pev->team == ZP::TEAM_OBSERVER ) )
			iAmountChoosen++;
	}

	// There are no left
	if ( m_LastChoosenZombies.size() >= iAmountChoosen )
		m_LastChoosenZombies.clear();
}

void CBaseGameMode::ClearChoosenZombies()
{
	m_LastChoosenZombies.clear();
}

void CBaseGameMode::RemoveFromList( int entindex )
{
	int idx = -1;
	for ( size_t i = 0; i < m_LastChoosenZombies.size(); i++ )
	{
		LastChoosenZombie choosen = m_LastChoosenZombies[i];
		if ( choosen.EntIndex == entindex )
		{
			idx = i;
			break;
		}
	}
	if ( idx == -1 ) return;
	m_LastChoosenZombies.erase( m_LastChoosenZombies.begin() + idx );
}
