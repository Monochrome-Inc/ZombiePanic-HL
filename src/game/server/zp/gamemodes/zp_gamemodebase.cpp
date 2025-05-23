// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#include "zp/info_random_base.h"
#include "zp_gamemodebase.h"

static IGameModeBase *s_GameModeBase = nullptr;

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

extern cvar_t timeleft;
extern cvar_t testmode;
extern cvar_t startdelay;

bool CBaseGameMode::IsTestModeActive() const
{
	return (testmode.value >= 1) ? true : false;
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
			if ( iTeam == ZP::TEAM_SURVIVIOR && plr->IsAlive() )
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
				MESSAGE_BEGIN(MSG_ALL, gmsgRoundState);
				WRITE_SHORT(GetRoundState());
				MESSAGE_END();
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
			SetRoundState( ZP::RoundState_RoundHasBegun );
		    MESSAGE_BEGIN(MSG_ALL, gmsgRoundState);
		    WRITE_SHORT(GetRoundState());
		    MESSAGE_END();
		    GiveWeaponsOnRoundStart();
		    ZP::SpawnWeaponsFromRandomEntities();
		}
	    break;
	}

	// If the round has not yet begun, we do not care about the timer
	if ( GetRoundState() != ZP::RoundState_RoundHasBegun ) return;

	static int last_time;

	int time_remaining = 0;

	float flTimeLimit = CVAR_GET_FLOAT( "mp_timelimit" ) * 60;
	time_remaining = (int)(flTimeLimit ? (flTimeLimit - gpGlobals->time) : 0);
	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
		m_bTimeRanOut = true;

	// Updates once per second
	if (timeleft.value != last_time)
		g_engfuncs.pfnCvar_DirectSet(&timeleft, UTIL_VarArgs("%i", time_remaining));
	last_time = time_remaining;

	// Sorry pal, we don't care if everyone is dead.
	if ( IsTestModeActive() ) return;

	// If we did not find anyone, then they may be all be dead (or disconnected)
	if ( !bHasSomeoneAlive )
		m_bAllSurvivorsDead = true;
}

void CBaseGameMode::GiveWeaponsOnRoundStart()
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsAlive() )
		{
			int iTeam = plr->pev->team;
			if ( iTeam == ZP::TEAM_SURVIVIOR )
				GiveWeapons( plr );
			plr->m_iHideHUD = 0;
			// Zombies can't choose weapons, they only got their arms.
			if ( iTeam == ZP::TEAM_ZOMBIE )
				plr->m_iHideHUD |= HIDEHUD_WEAPONS;
		}
	}
}

void CBaseGameMode::GiveWeapons(CBasePlayer *pPlayer)
{
	CBaseEntity *pWeaponEntity = NULL;
	BOOL addDefault = TRUE;

	while (pWeaponEntity = UTIL_FindEntityByClassname(pWeaponEntity, "game_player_equip"))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if (addDefault)
	{
		pPlayer->GiveNamedItem( "weapon_crowbar" );
		pPlayer->GiveNamedItem( "weapon_9mmhandgun" );
		pPlayer->GiveAmmo( 68, "9mm", _9MM_MAX_CARRY ); // 4 full reloads
	}
}