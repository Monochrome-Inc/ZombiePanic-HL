// ============== Copyright (c) 2025 Monochrome Games ============== \\

#ifndef SCRIPT_SYSTEM_SCRIPT_IOSYSTEM
#define SCRIPT_SYSTEM_SCRIPT_IOSYSTEM
#pragma once

#include "script_interface.h"

enum IOFunctionCommands_t
{
	IO_INVALID = -1,
	IO_FIRE_OUTPUT = 0,
	IO_EXEC_AS,
	IO_WAIT,
	IO_BREAK,
	IO_IF,
	IO_ELSEIF,
	IO_ELSE,
	IO_END,
	IO_PRINT_TO_CONSOLE,
	IO_PRINT_TO_CHAT,
	IO_GIVE_ITEM,
	IO_HAS_PARTICIPATED,
	IO_ADD_TO_SPAWN_LIST,
	IO_SPAWN_ITEMS,
	IO_SPAWN_MELEE_ITEMS,
	IO_RANDOM,
	IO_RANDOM_INT,
	IO_SWITCH,
	IO_CASE,
	IO_DEFAULT,

	IO_MAX,

	// Special stuff
	IO_SPECIAL_FUNCTION
};

enum IOFunctions_t
{
	IO_ON_MAP_START = 0,
	IO_ON_ROUND_START,
	IO_ON_MAP_END,
	IO_ON_PLAYER_SPAWNED,
	IO_ON_INPUT_RECEIVED,
	IO_ON_OUTPUT_SENT,

	IO_ON_MAX
};
const char *IO_GetAvailableFunctions( IOFunctions_t nFunc );
const char *IO_GetAvailableFunctionCommands( IOFunctionCommands_t nCommand );

struct ISpawnListData
{
	std::string ListName; // The list name (Ammo/Weapons/Item)
	std::string ItemName; // The item name
	int Limit; // The limit of this item in the list
};

enum IORequirementStatements
{
	IF_INVALID = -1,
	IF_EQUAL = 0,
	IF_NOT_EQUAL,
	IF_GREATER,
	IF_GREATER_EQUAL,
	IF_LESS,
	IF_LESS_EQUAL,
};

struct IOFunctionCommand
{
	IOFunctionCommands_t Type; // The type of this command
	std::string Message; // The message of our command, used by PRINT_TO values and IO_FIRE_OUTPUT after our first argument value
	float Delay; // How long until we fire this? used by IO_WAIT
	std::string EntFire; // The entity we want to call
	std::string Input; // The input we want to call
	std::string Require; // The value we require to execute this
	IORequirementStatements RequireStatement; // The requirement we need to use
	bool IsElseIf; // Is this an ELSEIF command?
	bool IsElse; // Is this an ELSE command?
	ISpawnListData SpawnItem; // The spawn list data, used by IO_ADD_TO_SPAWN_LIST

	// Switch/Case support
	std::string SwitchValue; // The value to switch on (for IO_SWITCH)
	std::string CaseValue;   // The case value to match (for IO_CASE)
	std::string VarName;     // Variable name to store result (for IO_RANDOM)
	
	// Switch/Case ID tracking
	int SwitchID = -1;  // Which switch block this belongs to
	int CaseID = -1;    // Which case within the switch (0 = first case, -1 = not a case)
	bool IsDefault = false; // Is this a default case?
};

struct IOFunctionData
{
	std::string FunctionName; // The function name
	std::vector<std::string> Parameters; // The parameters it takes
	std::vector<IOFunctionCommand> Commands; // The commands it will execute
};

enum IOFunctionCallIfBlockStatements
{
	IFBLOCK_NONE = 0,
	IFBLOCK_IF,
	IFBLOCK_ELSEIF,
	IFBLOCK_ELSE,
};

enum IOFunctionCallSwitchBlockStatements
{
	SWITCHBLOCK_NONE = 0,
	SWITCHBLOCK_SWITCH,
	SWITCHBLOCK_CASE,
	SWITCHBLOCK_DEFAULT,
};

struct IOFunctionCall
{
	uint ID; // Our ID for this function call
	std::string InputCall; // Our function
	std::vector<std::string> Arguments; // Our arguments
	std::vector<IOFunctionCommand> Commands; // The commands it will execute
	IOFunctionCallIfBlockStatements InsideIfBlock = IFBLOCK_NONE; // Are we inside an if block?
	IOFunctionCallSwitchBlockStatements InsideSwitchBlock = SWITCHBLOCK_NONE; // Are we inside a switch block?
	std::string SwitchValue; // The value we're switching on
	bool SwitchMatched = false; // Has a case matched?
	bool InCaseBlock = false; // Are we currently executing inside a matched case?
	
	// Switch/Case ID tracking (for multiple/nested switches)
	int CurrentSwitchID = -1;    // The switch ID we're currently in
	int CurrentCaseID = -1;      // The case ID we're currently executing (-1 = not in a case)
	int NextSwitchID = 0;        // Counter for assigning switch IDs during parsing
	int NextCaseID = 0;          // Counter for assigning case IDs during parsing
};

class IOScriptFile
{
public:
	IOScriptFile( const std::string &szFile );
	~IOScriptFile();

	ScriptCallBackEnum OnCalled(const std::string &szFunction, KeyValues *pData);
	void OnThink();
	void OnRoundRestart();

	// Variable management
	void SetVariable( const std::string &szName, const std::string &szValue );
	std::string GetVariable( const std::string &szName ) const;

private:
	ScriptCallBackEnum OnOutput( CBaseEntity *pEnt, const std::string &szAction, const std::string &szValue, const float &szDelay );
	ScriptCallBackEnum OnInput( CBaseEntity *pEnt, const std::string &szAction, const std::string &szValue );

	// Let's call our function with no arguments.
	ScriptCallBackEnum CallData( const std::string &szFunction );

	// Let's call our function with variable arguments.
	ScriptCallBackEnum CallData( const std::string &szFunction, const std::string &szArgs );

	// Let's call specific functions
	ScriptCallBackEnum CallData( const IOFunctions_t &nFunction, const std::string &szArgs );

	// Runs the commands
	void RunCommands( int nID );

	// Replace variables in string (e.g., {varName} -> value)
	std::string ReplaceVariables( const std::string &str ) const;

	std::string m_szFileName;
	// Our available functions
	std::vector<IOFunctionData> m_Functions;
	std::vector<IOFunctionCall> m_Commands;

	// Variable storage for script execution
	std::map<std::string, std::string> m_Variables;

	uint GetCurrentID() const;
};

class IOSystem : public IBaseScriptClass
{
public:
	IOSystem();

	AvailableScripts_t GetScriptType() { return AvailableScripts_t::InputOutput; }
	void OnInit();
	void OnThink();
	ScriptCallBackEnum OnCalled(pOnScriptCallbackReturn pfnCallback, KeyValues *pData, const std::string &szFunctionName);
	void OnLevelInit(bool bPostLoad);
	void OnLevelShutdown();
	void OnRoundRestart();
	void OnRegisterFunction(pOnScriptCallback pCallback, const std::string &szFunctionName);
	void OnRegisterFunction(CBaseEntity *pEntity, const std::string &szFunctionName);
	void OnLoadMapScriptFile();

private:
	inline bool FunctionAlreadyExist(const std::string &szFunctionName)
	{
		for (size_t i = 0; i < m_Functions.size(); i++)
		{
			IScriptFunctions ScriptFunction = m_Functions[i];
			if ( ScriptFunction.Function == szFunctionName )
				return true;
		}
		return false;
	}
	inline bool FunctionAlreadyExist(CBaseEntity *pEnt, const std::string &szFunctionName)
	{
		for (size_t i = 0; i < m_Functions.size(); i++)
		{
			IScriptFunctions ScriptFunction = m_Functions[i];
			if ( !CBaseEntity::Instance( ScriptFunction.Entity ) ) continue;
			if ( ScriptFunction.Function == szFunctionName
				&& FClassnameIs( CBaseEntity::Instance( ScriptFunction.Entity )->pev, STRING( pEnt->pev->classname ) ) )
				return true;
		}
		return false;
	}
	std::vector<IScriptFunctions> m_Functions;
	bool bAvailableToCall = false;
	IOScriptFile *m_szScript = nullptr;
};

#endif
