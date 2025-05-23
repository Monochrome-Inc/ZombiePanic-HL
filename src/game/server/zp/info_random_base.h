// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SERVER_RANDOM_ENTITY_BASE
#define SERVER_RANDOM_ENTITY_BASE
#pragma once

#include "cbase.h"

class CRandomItemBase : public CPointEntity
{
	SET_BASECLASS( CPointEntity );
public:
	enum ItemType
	{
		TypeNone = 0,
		TypeItem,
		TypeAmmo,
		TypeWeapon
	};

	void SpawnItem( void );
	virtual const char *GetRandomClassname() const = 0;
	virtual ItemType GetType() const = 0;
};

namespace ZP
{
	void SpawnWeaponsFromRandomEntities();
}

#endif