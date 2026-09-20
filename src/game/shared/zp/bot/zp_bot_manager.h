// ===========================================================================
// Copyright (C) 2026 by Zombie Panic! Team

#ifndef ZP_BOT_MANAGER_H
#define ZP_BOT_MANAGER_H

#include <bot/bot_manager.h>

// This class manages all active bots, propagating events to them and updating them.
class CZPBotManager : public CBotManager
{
	typedef CBotManager BaseClass;

public:
	CZPBotManager();

	// Use this singleton to access the bot manager
	static CZPBotManager *GetInstance();


	Place GetNavPlace() const { return m_navPlace; }
	void SetNavPlace( Place place ) { m_navPlace = place; }

	// Overrides from the base class
	void ClientDisconnect( CBasePlayer *pPlayer ) override;
	BOOL ClientCommand( CBasePlayer *pPlayer, const char *pcmd ) override;

	void ServerActivate( void ) override;
	void ServerDeactivate( void ) override;
	void ServerCommand( const char *pcmd ) override;
	void AddServerCommand( const char *cmd ) override;
	void AddServerCommands( void ) override;

	unsigned int GetPlayerPriority( CBasePlayer *player ) const override;	///< return priority of player (0 = max pri)

private:
	Place m_navPlace;	///< the current nav place
};

#endif // ZP_BOT_MANAGER_H