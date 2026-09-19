// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include <vgui/ISurface.h>
#include "steam_achievements.h"
#include "gameui/gameui_viewport.h"
#include "vgui/client_viewport.h"
#include "client_vgui.h"

// ================================================================= \\

extern void DoSteamStatReset();
extern void CallSteamStatReset( float flDelay );

// Let's construct our MapStatCheck_t array here.

#define _MAP_STAT(map, mapid, statid) { mapid, #map, statid }
static MapStatCheck_t g_MapStatChecks[] = {
	// Survival maps
	_MAP_STAT(zp_santeria, MAPSTAT_SURVIVAL, MAP_ZP_SANTERIA),
	_MAP_STAT(zp_clubzombo, MAPSTAT_SURVIVAL, MAP_ZP_CLUBZOMBO),
	_MAP_STAT(zp_thelabs, MAPSTAT_SURVIVAL, MAP_ZP_THELABS),
	_MAP_STAT(zp_eastside, MAPSTAT_SURVIVAL, MAP_ZP_EASTSIDE),
	_MAP_STAT(zp_industry, MAPSTAT_SURVIVAL, MAP_ZP_INDUSTRY),
	_MAP_STAT(zp_contingency, MAPSTAT_SURVIVAL, MAP_ZP_CONTINGENCY),
	_MAP_STAT(zp_haunted, MAPSTAT_SURVIVAL, MAP_ZP_HAUNTED),
	_MAP_STAT(zp_hotel, MAPSTAT_SURVIVAL, MAP_ZP_HOTEL),
	_MAP_STAT(zp_ruins, MAPSTAT_SURVIVAL, MAP_ZP_RUINS),
	//_MAP_STAT(zp_town, MAPSTAT_SURVIVAL, MAP_ZP_TOWN),
	//_MAP_STAT(zp_mansion, MAPSTAT_SURVIVAL, MAP_ZP_MANSION),

	// Objective maps
	_MAP_STAT(zpo_contingency, MAPSTAT_OBJECTIVE, MAP_ZPO_CONTINGENCY),
	_MAP_STAT(zpo_clubzombo, MAPSTAT_OBJECTIVE, MAP_ZPO_CLUBZOMBO),
	_MAP_STAT(zpo_eastside, MAPSTAT_OBJECTIVE, MAP_ZPO_EASTSIDE),
	_MAP_STAT(zpo_hotel, MAPSTAT_OBJECTIVE, MAP_ZPO_HOTEL),

	// Hardcore maps
	_MAP_STAT(zph_thelabs, MAPSTAT_HARDCORE, MAP_ZPH_THELABS),
	_MAP_STAT(zph_eastside, MAPSTAT_HARDCORE, MAP_ZPH_EASTSIDE),
	_MAP_STAT(zph_industry, MAPSTAT_HARDCORE, MAP_ZPH_INDUSTRY),
	_MAP_STAT(zph_haunted, MAPSTAT_HARDCORE, MAP_ZPH_HAUNTED),
	_MAP_STAT(zph_santeria, MAPSTAT_HARDCORE, MAP_ZPH_SANTERIA),
	_MAP_STAT(zph_contingency, MAPSTAT_HARDCORE, MAP_ZPH_CONTINGENCY),
	_MAP_STAT(zph_hotel, MAPSTAT_HARDCORE, MAP_ZPH_HOTEL),
	_MAP_STAT(zph_ruins, MAPSTAT_HARDCORE, MAP_ZPH_RUINS),
};

// ================================================================= \\

#define _STAT_ID(id, max) { id, #id, 0, max }
StatData_t g_SteamStats[] = {
	// INVALID STAT, MUST BE INDEX 0
	_STAT_ID(INVALID_STAT, 0),

	_STAT_ID(ZP_KILLS_CROWBAR, 10),
	_STAT_ID(ZP_KILLS_DBARREL, 80),
	_STAT_ID(ZP_KILLS_LEADPIPE, 20),
	_STAT_ID(ZP_KILLS_PISTOL, 40),
	_STAT_ID(ZP_KILLS_PPK, 40),
	_STAT_ID(ZP_KILLS_PPK_HS, 10),
	_STAT_ID(ZP_KILLS_REVOLVER, 25),
	_STAT_ID(ZP_KILLS_RIFLE, 30),
	_STAT_ID(ZP_KILLS_SHOTGUN, 35),
	_STAT_ID(ZP_KILLS_SATCHEL, 5),
	_STAT_ID(ZP_KILLS_TNT, 10),
	_STAT_ID(ZP_KILLS_MOLOTOV, 5),
	_STAT_ID(ZP_KILLS_ZOMBIE, 20),
	_STAT_ID(ZP_KILLS_MP5, 25),
	_STAT_ID(ZP_KILLS_FIREAXE, 20),
	_STAT_ID(ZP_FLEEESH, 50),
	_STAT_ID(ZP_ITS_A_MASSACRE, 25),
	_STAT_ID(ZP_PANIC_100, 100),
	_STAT_ID(ZP_PUMPUPSHOTGUN, 777),
	_STAT_ID(ZP_CHILDOFGRAVE, 666),
	_STAT_ID(ZP_MMMWAP, 1000),
	_STAT_ID(ZP_ESCAPE_ARTIST, 250),
	_STAT_ID(ZP_REGEN_10K, 10000),
	_STAT_ID(ZP_ILIVEAGAIN, 1000),
	_STAT_ID(ZP_HC_SNACKTIME, 200),
	_STAT_ID(ZP_GENOCIDESTEP3, 6666),
	_STAT_ID(ZP_KILLYOSELF, 100),
	_STAT_ID(ZP_EFFED_FACE, 50),

	_STAT_ID(MAP_ZP_SANTERIA, 1),
	_STAT_ID(MAP_ZP_CLUBZOMBO, 1),
	_STAT_ID(MAP_ZP_THELABS, 1),
	_STAT_ID(MAP_ZP_EASTSIDE, 1),
	_STAT_ID(MAP_ZP_INDUSTRY, 1),
	_STAT_ID(MAP_ZP_CONTINGENCY, 1),
	_STAT_ID(MAP_ZP_HAUNTED, 1),
	_STAT_ID(MAP_ZP_HOTEL, 1),
	_STAT_ID(MAP_ZP_RUINS, 1),
	_STAT_ID(MAP_ZP_TOWN, 1),
	_STAT_ID(MAP_ZP_MANSION, 1),

	_STAT_ID(MAP_ZPO_CONTINGENCY, 1),
	_STAT_ID(MAP_ZPO_CLUBZOMBO, 1),
	_STAT_ID(MAP_ZPO_EASTSIDE, 1),
	_STAT_ID(MAP_ZPO_HOTEL, 1),

	_STAT_ID(MAP_ZPH_THELABS, 1),
	_STAT_ID(MAP_ZPH_EASTSIDE, 1),
	_STAT_ID(MAP_ZPH_INDUSTRY, 1),
	_STAT_ID(MAP_ZPH_HAUNTED, 1),
	_STAT_ID(MAP_ZPH_SANTERIA, 1),
	_STAT_ID(MAP_ZPH_CONTINGENCY, 1),

	_STAT_ID(ZP_RESET_ALL, 1),
};

// ================================================================= \\

StatData_t GrabStat( EStats nID )
{
	for ( int i = 0; i < ARRAYSIZE(g_SteamStats); i++ )
	{
		StatData_t stat = g_SteamStats[i];
		if ( stat.ID == nID ) return stat;
	}
	return g_SteamStats[0];
}

void SetStat( EStats nID, int32 value )
{
	for ( int i = 0; i < ARRAYSIZE(g_SteamStats); i++ )
	{
		StatData_t &stat = g_SteamStats[i];
		if ( stat.ID == nID )
			stat.Value = value;
	}
}

// Resets all stats to 0 if ZP_RESET_ALL is 1 (or 0 in debug mode)
void STAT_ResetAllStats()
{
	// We will restet ZP_RESET_ALL locally as well,
	// but we never read it so who cares.
	for ( int i = 0; i < ARRAYSIZE(g_SteamStats); i++ )
	{
		StatData_t &stat = g_SteamStats[i];
		stat.Value = 0;
		GetSteamAPI()->SteamUserStats()->SetStat( stat.Name, stat.Value );
	}

#if defined( _DEBUG )
	StatData_t statReset = GrabStat( ZP_RESET_ALL );
	GetSteamAPI()->SteamUserStats()->SetStat( statReset.Name, 1 );
#endif
}

// Only used by RELEASE build.
void STAT_StoreResetAllValue()
{
	StatData_t statReset = GrabStat( ZP_RESET_ALL );
	GetSteamAPI()->SteamUserStats()->SetStat( statReset.Name, 0 );
	GetSteamAPI()->SteamUserStats()->StoreStats();
}

void STAT_OnUserStatsReceived( UserStatsReceived_t *pCallback )
{
	if ( k_EResultOK != pCallback->m_eResult ) return;
	// Go through our stats
	for ( int i = 0; i < ARRAYSIZE(g_SteamStats); i++ )
	{
		StatData_t stat = g_SteamStats[i];
		int32 iData = 0;
		GetSteamAPI()->SteamUserStats()->GetUserStat( pCallback->m_steamIDUser, stat.Name, &iData );
		SetStat( stat.ID, iData );
	}

	// Go through our achievements, and apply their state.
	for ( int i = 0; i < ACHV_MAX; i++ )
	{
		DialogAchievementData ach = GetAchievementByID( i );
		bool bIsAchieved = ach.IsAchieved();
		GetSteamAPI()->SteamUserStats()->GetAchievement( ach.GetAchievementName(), &bIsAchieved );
		SetAchievementCompletedByID( ach, bIsAchieved );
	}

#if 0
	// Don't reset stats in debug mode, only in release.
#if !defined( _DEBUG )
	// Check if our stat ZP_RESET_ALL is 1. If it is, reset all stats.
	StatData_t statReset = GrabStat( ZP_RESET_ALL );
	if ( statReset.Value == 1 )
	{
		DoSteamStatReset();
		CallSteamStatReset( 5.0f ); // Delay it a bit to make sure Steam processes it.
	}
#else
	// Ditto, but inverted for debug mode.
	// Check if our stat ZP_RESET_ALL is 0. If it is, reset all stats.
	StatData_t statReset = GrabStat( ZP_RESET_ALL );
	if ( statReset.Value == 0 )
	{
		DoSteamStatReset();
		CallSteamStatReset( 5.0f ); // Delay it a bit to make sure Steam processes it.
	}
#endif
#endif
}

#if defined( _DEBUG )
CON_COMMAND( zp_reset_all_stats, "Reset all Steam stats and achievements." )
{
	DoSteamStatReset();
	CallSteamStatReset( 5.0f ); // Delay it a bit to make sure Steam processes it.
	Msg( "All stats has been reset!\n" );
}
#endif

// ================================================================= \\

bool CLIENT_UTIL_IsSpecialAchievement( int iAchievement )
{
	if ( iAchievement == EAchievements::JACKOFTRADES ) return true;
	if ( iAchievement == EAchievements::PARTNERINCRIME ) return true;
	if ( iAchievement == EAchievements::MARATHON ) return true;
	if ( iAchievement == EAchievements::PLAY_ALL_SURVIVAL ) return true;
	if ( iAchievement == EAchievements::PLAY_ALL_OBJECTIVE ) return true;
	return false;
}

bool CLIENT_UTIL_HasEarnedAchievement( int iAchievement )
{
	bool bEarned = false;
	GetSteamAPI()->SteamUserStats()->GetAchievement( GetAchievementByID( iAchievement ).GetAchievementName(), &bEarned );
	return bEarned;
}

int32 CLIENT_UTIL_GetStatAchievement( int iAchievement )
{
	int32 value;
	GetSteamAPI()->SteamUserStats()->GetStat( GetAchievementByID( iAchievement ).GetData().Name, &value );
	return value;
}

static bool PlayingWithAFriend()
{
	for (int i = 1; i <= MAX_PLAYERS; i++)
	{
		CPlayerInfo *pi = GetPlayerInfo(i)->Update();
		if ( !pi ) continue;
		if ( !pi->IsConnected() ) continue;
		CSteamID steamfriend( pi->GetValidSteamID64() );
		if ( !steamfriend.IsValid() ) continue;
		if ( GetSteamAPI()->SteamFriends()->GetFriendRelationship( steamfriend ) == k_EFriendRelationshipFriend )
		{
			// Check our local player with this player, and make sure we are on the same team!
			CPlayerInfo *localplayer = GetPlayerInfo( gEngfuncs.GetLocalPlayer()->index );
			if ( !localplayer ) continue;
			if ( !localplayer->IsConnected() ) continue;
			if ( localplayer->GetTeamNumber() != pi->GetTeamNumber() ) continue;
			return true;
		}
	}
	return false;
}

StatData_t GrabMapStat( MapStatCheck nCheck )
{
	// Get the map name
	char mapname[64];
	strcpy( mapname, gEngfuncs.pfnGetLevelName() );

	// Remove the "maps/" part if it exists.
	if ( V_stristr( mapname, "maps/" ) == mapname )
	{
		// Move the string 5 characters forward.
		memmove( mapname, mapname + 5, strlen(mapname) - 4 );
	}

	// Remove the extension if it exists.
	char *dot = V_stristr( mapname, ".bsp" );
	if ( dot ) *dot = '\0';

	// Make sure our mapname is lowercase.
	V_strlower( mapname );

	// mapname should now return something like "zp_santeria"
	// instead of "maps/ZP_santeria.bsp" (official maps does not use uppercase, but it's just an example.)

	// Now, check our array for a match.
	for ( size_t i = 0; i < ARRAYSIZE(g_MapStatChecks); i++ )
	{
		if ( g_MapStatChecks[i].Check != nCheck ) continue;
		if ( !V_stricmp( g_MapStatChecks[i].MapName, mapname ) )
		{
			return GrabStat( g_MapStatChecks[i].StatID );
		}
	}

	// If we reach here, return invalid.
	return GrabStat( INVALID_STAT );
}

// Similar to the IndicateAchievementProgress below,
// but this is used for map progress and does not update
// the actual achievement itself (or the stat).
static void NotifyMapProgress( int iAchievement )
{
	if ( !GetSteamAPI() ) return;
	if ( !GetSteamAPI()->SteamUserStats() ) return;
	CHudAchievementNotification::Get()->ShowAchievement( iAchievement );
}

// Increase the map stat by one.
// This is used for achievements like "Play all survival maps", "Play all objective maps" etc.
// It's also used by marathon achievement, which is why this is a simple static function.
static bool SetMapStat( MapStatCheck nCheck )
{
	StatData_t stat = GrabMapStat( nCheck );
	if ( stat.ID == INVALID_STAT ) return false;
	if ( stat.Value == 1 ) return false; // Already completed.
	stat.Value++;
	SetStat( stat.ID, stat.Value );
	GetSteamAPI()->SteamUserStats()->SetStat( stat.Name, stat.Value );
	return true;
}

static bool CheckIfAllMapsAreCompleted( MapStatCheck nCheck )
{
	// Check if all maps of this type are completed.
	bool bAllCompleted = true;

	// Go trough our array, and check if all maps of this type are completed.
	// They need to have a value of 1 or higher.
	for ( size_t i = 0; i < ARRAYSIZE( g_MapStatChecks ); i++ )
	{
		if ( g_MapStatChecks[i].Check != nCheck ) continue;
		StatData_t stat = GrabStat( g_MapStatChecks[i].StatID );
		if ( stat.ID == INVALID_STAT || stat.Value < 1 )
		{
			bAllCompleted = false;
			break;
		}
	}

	return bAllCompleted;
}

static bool CheckIfMarathonIsComplete()
{
	// Check if all maps of this type are completed.
	bool bAllCompleted = true;

	// Same as CheckIfAllMapsAreCompleted, but check for all map types.
	for ( size_t i = 0; i < ARRAYSIZE( g_MapStatChecks ); i++ )
	{
		StatData_t stat = GrabStat( g_MapStatChecks[i].StatID );
		if ( stat.ID == INVALID_STAT || stat.Value < 1 )
		{
			bAllCompleted = false;
			break;
		}
	}

	return bAllCompleted;
}

bool IsAtCertainPoint( int32 iValue, int32 iMaxValuem, float flProgress )
{
	float flValue = float(iValue) / iMaxValuem;
	return ( flValue == flProgress ) ? true : false;
}

void CLIENT_UTIL_GiveAchievement( int iAchievement )
{
	if ( !GetSteamAPI() ) return;
	if ( !GetSteamAPI()->SteamUserStats() ) return;

	bool bEarned = false;
	GetSteamAPI()->SteamUserStats()->GetAchievement( GetAchievementByID( iAchievement ).GetAchievementName(), &bEarned );

	// Already have it? Skip.
	if ( bEarned ) return;

	// If jack of trades, check
	if ( CLIENT_UTIL_IsSpecialAchievement( iAchievement ) )
	{
		switch ( iAchievement )
		{
			case EAchievements::JACKOFTRADES:
			{
				// Check each of these, and make sure we got them all first!
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_CROWBAR ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_MP5 ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_PPK ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_PISTOL ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_REVOLVER ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_RIFLE ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_SATCHEL ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_SHOTGUN ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_DBARREL ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_TNT ) ) return;
			    if ( !CLIENT_UTIL_GetStatAchievement( EAchievements::KILLS_MOLOTOV ) ) return;
			}
			break;

		    case EAchievements::PARTNERINCRIME:
			{
				// Make sure we are playing with a friend.
				// And make sure we are on the same team!
			    if ( !PlayingWithAFriend() ) return;
			}
			break;

		    case EAchievements::MARATHON:
		    {
			    if ( SetMapStat( MAPSTAT_SURVIVAL )
					|| SetMapStat( MAPSTAT_OBJECTIVE )
					|| SetMapStat( MAPSTAT_HARDCORE ) )
				    NotifyMapProgress( iAchievement );
			    if ( !CheckIfMarathonIsComplete() )
					return;
			}
			break;

		    case EAchievements::PLAY_ALL_SURVIVAL:
		    {
			    if ( SetMapStat( MAPSTAT_SURVIVAL ) )
				    NotifyMapProgress( iAchievement );
				if ( !CheckIfAllMapsAreCompleted( MAPSTAT_SURVIVAL ) )
					return;
			}
			break;

		    case EAchievements::PLAY_ALL_OBJECTIVE:
		    {
			    if ( SetMapStat( MAPSTAT_OBJECTIVE ) )
				    NotifyMapProgress( iAchievement );
				if ( !CheckIfAllMapsAreCompleted( MAPSTAT_OBJECTIVE ) )
					return;
			}
			break;

			// TODO: Add PLAY_ALL_HARDCORE
			// and maybe other special achievements.
		}
	}

	// Do we have a stat value?
	if ( GetAchievementByID( iAchievement ).HasStatID() )
	{
		int32 value;
		GetSteamAPI()->SteamUserStats()->GetStat( GetAchievementByID( iAchievement ).GetData().Name, &value );

		// Increase it by one.
		value++;
		GetSteamAPI()->SteamUserStats()->SetStat( GetAchievementByID( iAchievement ).GetData().Name, value );

		// Update it.
		SetStat( GetAchievementByID( iAchievement ).GetData().ID, value );

		// Check if we have enough.
		int32 maxvalue = GetAchievementByID( iAchievement ).GetData().MaxValue;
		if ( value < maxvalue )
		{
			bool bShouldDraw = false;
			// 25%, 50% or 90% of the way?
			if ( IsAtCertainPoint( value, maxvalue, 0.25 ) ) bShouldDraw = true;
			else if ( !bShouldDraw && IsAtCertainPoint( value, maxvalue, 0.5 ) ) bShouldDraw = true;
			else if ( !bShouldDraw && IsAtCertainPoint( value, maxvalue, 0.9 ) ) bShouldDraw = true;
			if ( bShouldDraw )
				CHudAchievementNotification::Get()->ShowAchievement( iAchievement );
			return;
		}
	}

	CHudAchievementNotification::Get()->ShowAchievement( iAchievement );

	// For our dialog
	SetAchievementCompletedByID( GetAchievementByID( iAchievement ), true );

	// Give the achievement.
	GetSteamAPI()->SteamUserStats()->SetAchievement( GetAchievementByID( iAchievement ).GetAchievementName() );

	// Tell the server, that we earned it!
	gEngfuncs.pfnServerCmd( vgui2::VarArgs( "achearn %i", iAchievement ) );
}

// ================================================================= \\

// Let's create our custom HUD element here, specifically for steam achievement progress
// It's mainly used by the map notifications however, but can be used for many things too.

DEFINE_HUD_ELEM( CHudAchievementNotification );

#define NOTIFICATION_HEIGHT 33
#define NOTIFICATION_WIDTH 200
static ConVar cl_achievement_popupspeed( "cl_achievement_popupspeed", "4" );

CHudAchievementNotification::CHudAchievementNotification()
    : BaseClass( NULL, "HudAchievementNotification" )
{
	SetParent( g_pViewport );
	SetProportional( true );

	vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile( VGUI2_ROOT_DIR "resource/ChatScheme.res", "ChatScheme" );
	SetScheme( scheme );
	SetPaintBackgroundEnabled( true );

	m_pIcon = new vgui2::ImagePanel( this, "IconImage" );
	m_pIcon->SetShouldScaleImage( true );
	m_pLabel = new vgui2::Label( this, "LabelText", "" );
	m_pLabelProgress = new vgui2::Label( this, "LabelProgress", "" );

	// Make sure our size is set only once.
	SetSize( GetScaledValue( NOTIFICATION_WIDTH ), GetScaledValue( NOTIFICATION_HEIGHT ) );

	ClearData();
}

void CHudAchievementNotification::ApplySchemeSettings( vgui2::IScheme *pScheme )
{
	// Use the chat scheme
	LoadControlSettings( VGUI2_ROOT_DIR "resource/Chat.res" );
	BaseClass::ApplySchemeSettings( pScheme );

	// Setup the background painting style
	SetPaintBackgroundType( 2 );
	SetPaintBorderEnabled( true );
	SetPaintBackgroundEnabled( true );

	// Fix our size for larger resolutions.
	SetSize( GetScaledValue( NOTIFICATION_WIDTH ), GetScaledValue( NOTIFICATION_HEIGHT ) );

	// Set the background color
	Color cColor = pScheme->GetColor("ChatBgColor", GetBgColor());
	SetBgColor( Color( cColor.r(), cColor.g(), cColor.b(), 64 ) );

	// Set our text color
	m_pLabel->SetFgColor( pScheme->GetColor( "ChatTextColor", NoTeamColor::Orange ) );
	m_pLabel->SetPaintBackgroundEnabled( false ); // No background for our label
}

void CHudAchievementNotification::Init()
{
	ClearData();
	PrecacheImages();
	// Make sure its at the bottom.
	SetDefaultPos();
}

void CHudAchievementNotification::VidInit()
{
	ClearData();
	PrecacheImages();
	// Make sure its at the bottom.
	SetDefaultPos();
}

void CHudAchievementNotification::SetDefaultPos()
{
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	// Make sure we're at the bottom center of the screen.
	wide = (wide / 2) - ( GetScaledValue( NOTIFICATION_WIDTH ) / 2);
	SetPos( wide, tall + GetScaledValue( NOTIFICATION_HEIGHT ) );
}

void CHudAchievementNotification::Think()
{
	// if we should draw, make sure we move up from the bottom of the screen.
	if ( m_bShowingAchievement )
	{
		// Hide after 6 seconds.
		bool bShouldHide = ( gEngfuncs.GetClientTime() - m_flStartTime ) > 6.0f;
		int wide, tall;
		vgui2::surface()->GetScreenSize( wide, tall );
		int targety = tall - GetScaledValue( NOTIFICATION_HEIGHT );

		int x, y;
		GetPos( x, y );

		if ( bShouldHide )
		{
			// We overstayed our welcome, move down and hide.
			targety = tall + GetScaledValue( NOTIFICATION_HEIGHT );
			if ( y < targety )
			{
				y += cl_achievement_popupspeed.GetInt();
				if ( y > targety ) y = targety;
				SetPos( x, y );
			}
			else
			{
				// We're off the screen, clear data
				ClearData();
			}
			return;
		}

		if ( y > targety )
		{
			y -= cl_achievement_popupspeed.GetInt();
			if ( y < targety ) y = targety;
			SetPos( x, y );
		}
	}
}

void CHudAchievementNotification::Paint()
{
	if ( !m_bShowingAchievement ) return;
	// Paint our background first.
	int wide, tall;
	GetSize( wide, tall );

	// We use GetScaledValue so that it scales with resolution.
	int iIconSize = GetScaledValue( 30 );

	// Paint our icon.
	if ( m_pIcon )
	{
		m_pIcon->SetPos( 10, 5 );
		m_pIcon->SetSize( iIconSize, iIconSize );
	}

	// Now, let's paint our text next to the icon.
	if ( m_pLabel )
	{
		m_pLabel->SetPos( GetScaledValue( 37 ), 10 );
		m_pLabel->SetSize( wide - GetScaledValue( 50 ), GetScaledValue( 8 ) );
	}

	// Let's paint our progress bar, and then the label after that.
	// Since it needs to be drawn ontop of the bar.
	if ( m_pLabelProgress && m_iMaxValue > 0 )
	{
		int barwide = wide - GetScaledValue( 45 );
		int barheight = GetScaledValue( 10 );
		int barx = GetScaledValue( 37 );
		int bary = GetScaledValue( 15 );

		// Progress bar background
		vgui2::surface()->DrawSetColor( 25, 25, 25, 255 );
		vgui2::surface()->DrawFilledRect( barx, bary, barx + barwide, bary + barheight );

		float flPercent = float(m_iValue) / float(m_iMaxValue);
		int fillwide = int(float(barwide) * flPercent);

		// Our progress bar
		vgui2::surface()->DrawSetColor( 160, 10, 10, 255 );
		vgui2::surface()->DrawFilledRect( barx, bary, barx + fillwide, bary + barheight );

		// Outline
		vgui2::surface()->DrawSetColor( 100, 100, 100, 255 );
		vgui2::surface()->DrawOutlinedRect( barx, bary, barx + barwide, bary + barheight );

		// Now draw the label ontop of it.
		m_pLabelProgress->SetPos( barx, bary );
		m_pLabelProgress->SetSize( barwide, barheight );
	}
}

void CHudAchievementNotification::ClearData()
{
	m_flStartTime = 0.0f;
	m_iValue = 0;
	m_iMaxValue = 0;
	m_bShowingAchievement = false;

	SetDefaultPos();

	// Process our queue.
	if ( m_Queue.size() > 0 )
	{
		int iAchievement = m_Queue[0];
		m_Queue.erase( m_Queue.begin() );
		ShowAchievement( iAchievement );
	}
}

void CHudAchievementNotification::PrecacheImages()
{
	// Precache all achievement images.
	for ( int i = 0; i < ACHV_MAX; i++ )
	{
		DialogAchievementData AchievementData = GetAchievementByID( i );
		char buffer[158];
		Q_snprintf( buffer, sizeof( buffer ), "ui/achievements/%s", AchievementData.GetAchievementName() );
		vgui2::scheme()->GetImage( buffer, false );
	}
}

void CHudAchievementNotification::ShowAchievement( int iAchievement )
{
	if ( m_bShowingAchievement )
	{
		// We already have an achievement showing?
		// Add this to our queue instead.
		// But let's not add duplicates.
		for ( size_t i = 0; i < m_Queue.size(); i++ )
		{
			if ( m_Queue[i] == iAchievement ) return;
		}
		m_Queue.push_back( iAchievement );
		return;
	}

	// Get our achievement data.
	DialogAchievementData AchievementData = GetAchievementByID( iAchievement );

	// Make sure we only daaw this for a few seconds.
	m_flStartTime = gEngfuncs.GetClientTime();

	// Set our icon image.
	if ( m_pIcon )
	{
		char buffer[158];
		Q_snprintf( buffer, sizeof( buffer ), "ui/achievements/%s", AchievementData.GetAchievementName() );
		m_pIcon->SetImage( vgui2::scheme()->GetImage( buffer, false ) );
	}

	// Set our label text.
	if ( m_pLabel )
	{
		char buffer[158];
		Q_snprintf( buffer, sizeof( buffer ), "#ZP_ACH_%s_NAME", AchievementData.GetAchievementName() );
		m_pLabel->SetText( buffer );
	}

	// Let's set our progress text with the format of "current / max"
	if ( m_pLabelProgress )
	{
		m_iValue = 0;
		m_pLabelProgress->SetVisible( true );
		if ( AchievementData.HasStatID() )
		{
			m_iValue = GrabStat( AchievementData.GetData().ID ).Value;
			m_iMaxValue = AchievementData.GetData().MaxValue;
		}
		else if ( AchievementData.HasRequiredSteps() )
		{
			m_iMaxValue = AchievementData.GetRequiredStepCount();
			for ( size_t i = 0; i < m_iMaxValue; i++ )
			{
				// Check if this step is completed.
				if ( GrabStat( AchievementData.GetRequiredStepID( i ).Stat ).Value >= 1 )
					m_iValue++;
			}
		}
		else
		{
			m_pLabelProgress->SetVisible( false );
			m_iMaxValue = 0;
		}
		char buffer[64];
		Q_snprintf( buffer, sizeof(buffer), "%i / %i", m_iValue, m_iMaxValue );
		m_pLabelProgress->SetText( buffer );
		m_pLabelProgress->SetContentAlignment( vgui2::Label::a_center );
	}

	// Play the audio sound.
	if ( m_iValue == m_iMaxValue && m_iMaxValue > 0 )
		gEngfuncs.pfnPlaySoundByName( "ui/ach_earned.wav", 1.0f );
	else
		gEngfuncs.pfnPlaySoundByName( "ui/ach_progress.wav", 1.0f );

	m_bShowingAchievement = true;
}

CON_COMMAND( cl_debug_show_achievement, "Show the achievements dialog." )
{
	static ConVarRef sv_cheats( "sv_cheats" );
	if ( !sv_cheats.GetBool() )
	{
		gEngfuncs.Con_Printf( "Cheats must be enabled to use this command.\n" );
		return;
	}
	const char *pszCommand = gEngfuncs.Cmd_Argv( 1 );
	if ( !pszCommand ) return;
	int iAchievement = Q_atoi( pszCommand );
	CHudAchievementNotification::Get()->ShowAchievement( iAchievement );
}
