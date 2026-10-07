// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include "core.h"
#include "iosys.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <cstdlib>
#include <tier2/tier2.h>
#include "FileSystem.h"
#include <KeyValues.h>
#include <convar.h>

// For giving stuff for our players.
#include "player.h"

#include "zp/info_random_base.h"

static ConVar sv_ss_debug( "sv_ss_debug", "0", FCVAR_CHEATS );

// A simple static list of our I/O IOFunctions_t
static const char *g_IOFunctions[IO_ON_MAX] = {
	"OnMapStart",
	"OnRoundStart",
	"OnMapEnd",
	"OnPlayerSpawned",
	"OnInputReceived",
	"OnOutputSent"
};

// Same as above, a static list of our I/O IOFunctionCommands_t
static const char *g_IOCommands[IO_MAX] = {
	"FireOutput",
	"ASCall",
	"Wait",
	"Break",
	"if",
	"elseif",
	"else",
	"end",
	"PrintToConsole",
	"PrintToChat",
	"GiveItem",
	"Participated",
	"AddToSpawnList",
	"SpawnItems",
	"SpawnMeleeWeapons",
	"Random",
	"RandomInt",
	"switch",
	"case",
	"default",
};

static std::vector<ISpawnListData> s_SpawnListData; // Used by IO_SPAWN_ITEMS and IO_ADD_TO_SPAWN_LIST

static std::string MessageCleanup( const std::string &str )
{
	const char *ws = "\"";
	size_t start = str.find_first_not_of(ws);
	if (start == std::string::npos)
		return "";
	size_t end = str.find_last_not_of(ws);
	std::string output = str.substr(start, end - start + 1);
	// Now, we check if we have a remaining " at the end, if we do, get rid of it.
	size_t last = output.find_last_of(ws);
	if (last == std::string::npos)
		return output;
	return output.substr(start, last - 1);
}

static std::string Trim( const std::string &str )
{
	const char *ws = " \t\n\r";
	size_t start = str.find_first_not_of(ws);
	if (start == std::string::npos)
		return "";
	size_t end = str.find_last_not_of(ws);
	return str.substr(start, end - start + 1);
}

static std::vector<std::string> Split( const std::string &s, char delimiter )
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter))
		tokens.push_back(Trim(token));
	return tokens;
}

static std::vector<std::string> SplitQoutes( const std::string &s, char delimiter )
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while ( tokenStream >> std::quoted(token) )
		tokens.push_back( Trim(token) );
	return tokens;
}

static std::string GetArgValues(const std::string &message, const std::vector<std::string> &params)
{
	std::string result = message;
	for (size_t i = 0; i < params.size(); ++i)
	{
		std::string replacement = params[i];
		std::string pattern = "%s" + std::to_string(i) + "%";
		size_t pos = 0;
		while ((pos = result.find(pattern, pos)) != std::string::npos)
		{
			result.replace(pos, pattern.length(), replacement);
			pos += replacement.length();
		}
	}
	return result;
}

static std::string ReplaceScriptArgs(const std::string &message, const std::vector<std::string> &params)
{
	std::string result = message;
	for (size_t i = 0; i < params.size(); ++i)
	{
		std::string pattern = "{" + params[i] + "}";
		std::string replacement = "%s" + std::to_string(i) + "%";
		size_t pos = 0;
		while ((pos = result.find(pattern, pos)) != std::string::npos)
		{
			result.replace(pos, pattern.length(), replacement);
			pos += replacement.length();
		}
	}
	return result;
}

IOSystem::IOSystem()
{
	m_Functions.clear();
	// Make sure this is always a nullptr on creation.
	m_szScript = nullptr;
	// Make sure we add I/O system into our manager.
	ScriptSystem::AddToScriptManager( this );
}

// Does nothing.
static void DefaultGenericCallback( KeyValues * ) {}

void IOSystem::OnInit()
{
	// Default callbacks
	for ( size_t i = 0; i < IO_ON_MAX; i++ )
		ScriptSystem::RegisterScriptCallback( AvailableScripts_t::InputOutput, DefaultGenericCallback, g_IOFunctions[i] );
}

void IOSystem::OnThink()
{
	if ( !m_szScript ) return;
	m_szScript->OnThink();
}

ScriptCallBackEnum IOSystem::OnCalled(pOnScriptCallbackReturn pfnCallback, KeyValues *pData, const std::string &szFunctionName)
{
	if ( !bAvailableToCall ) return ScriptCall_Warning;
	ScriptCallBackEnum nRet = ScriptCall_Error;
	bool bIsScriptCall = FStrEq( pData->GetString( "arg0" ), "SCall" );
	for (size_t i = 0; i < m_Functions.size(); i++)
	{
		IScriptFunctions ScriptFunction = m_Functions[i];
		if ( ScriptFunction.Function == szFunctionName )
		{
			// Only call once.
			if ( m_szScript && !bIsScriptCall )
			{
				nRet = m_szScript->OnCalled( szFunctionName, pData );
				bIsScriptCall = true;
			}
			else
			{
				// We found our function, return OK
				nRet = ScriptCall_OK;
			}
			if ( ScriptFunction.Callback ) (*ScriptFunction.Callback)(pData);
			if ( ScriptFunction.Entity )
			{
				if ( bIsScriptCall )
				{
					if ( sv_ss_debug.GetBool() )
					{
						Msg( "Action: %s\n", szFunctionName.c_str() );
						Msg( "Argument 1: %s\n", pData->GetString( "arg1" ) );
						Msg( "Argument 2: %s\n", pData->GetString( "arg2" ) );
					}

					// Make sure we call the right entity, if not, ignore.
					CBaseEntity *pFind = UTIL_FindEntityByTargetname( nullptr, pData->GetString( "arg1" ) );
					while ( pFind )
					{
						KeyValues *pKvNew = new KeyValues( "Items" );
						pKvNew->SetString( "Action", szFunctionName.c_str() );
						pKvNew->SetString( "arg0", pData->GetString( "arg2" ) );
						pFind->ScriptCallback( pKvNew );
						pKvNew->deleteThis();
						pFind = UTIL_FindEntityByTargetname( pFind, pData->GetString( "arg1" ) );
					}
					break;
				}
				else
				{
					CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( ScriptFunction.Entity );
					if ( pEntity && pEntity->entindex() == atoi( pData->GetString( "arg0" ) ) )
					{
						if ( sv_ss_debug.GetBool() )
						{
							Msg( "Entity: %s\n", pData->GetString( "arg0" ) );
							Msg( "Action: %s\n", szFunctionName.c_str() );
							Msg( "Argument 1: %s\n", pData->GetString( "arg1" ) );
							Msg( "Argument 2: %s\n", pData->GetString( "arg2" ) );
						}

						KeyValues *pKvNew = new KeyValues( "Items" );
						pKvNew->SetString( "Action", szFunctionName.c_str() );
						pKvNew->SetString( "arg0", pData->GetString( "arg1" ) );
						pEntity->ScriptCallback( pKvNew );
						pKvNew->deleteThis();
						break;
					}
				}
			}
		}
	}
	if ( pfnCallback )
		(*pfnCallback)(pData, GetScriptType());
	return nRet;
}

void IOSystem::OnLevelInit( bool bPostLoad )
{
	if ( !bPostLoad )
	{
		bAvailableToCall = true;
		OnLoadMapScriptFile();
		ScriptSystem::CallScriptDelay( AvailableScripts_t::InputOutput, nullptr, g_IOFunctions[ IO_ON_MAP_START ], 5.0f, 0 );
	}
}

void IOSystem::OnLevelShutdown()
{
	ScriptSystem::CallScript( AvailableScripts_t::InputOutput, nullptr, g_IOFunctions[ IO_ON_MAP_END ], 0 );
	if ( m_szScript )
		delete m_szScript;
	m_szScript = nullptr;
	bAvailableToCall = false;
	// Clear our functions on shutdown
	m_Functions.clear();
}

void IOSystem::OnRoundRestart()
{
	if ( m_szScript )
		m_szScript->OnRoundRestart();
}

void IOSystem::OnRegisterFunction(pOnScriptCallback pCallback, const std::string &szFunctionName)
{
	if ( FunctionAlreadyExist( szFunctionName ) ) return;
	IScriptFunctions ScriptFunction;
	ScriptFunction.Callback = pCallback;
	ScriptFunction.Entity = nullptr;
	ScriptFunction.Function = szFunctionName;
	m_Functions.push_back( ScriptFunction );
}

void IOSystem::OnRegisterFunction(CBaseEntity *pEntity, const std::string &szFunctionName)
{
	if ( FunctionAlreadyExist( pEntity, szFunctionName ) ) return;
	IScriptFunctions ScriptFunction;
	ScriptFunction.Callback = nullptr;
	ScriptFunction.Entity = pEntity->edict();
	ScriptFunction.Function = szFunctionName;
	m_Functions.push_back( ScriptFunction );
}

void IOSystem::OnLoadMapScriptFile()
{
	// If we have a script already, get rid of it.
	if ( m_szScript )
		delete m_szScript;
	m_szScript = nullptr;

	// Load our map script file.
	m_szScript = new IOScriptFile( "scripts/maps/" + std::string( STRING( gpGlobals->mapname ) ) + ".io" );
}

IOScriptFile::IOScriptFile( const std::string &szFile )
{
	// Let's parse our file here.
	// This is how we will parse our I/O file.
	/*
	// An example of a function callback (with no paramaters), an empty () works as well:
	Function OnMapStart
	{
		// Our map started!
		// Fire's an output to an entity.
		// Arguments:
		// arg0 = Entity targetname
		// arg1 = Input name
		// arg2 = Value
		// arg3 = Delay
		// Example:
		FireOutput "my_target" "MyInput" "MyValue" 5.0
	}

	Function OnMapEnd()
	{
		// Our map ended!
		// Fire's an output to an entity.
		// Arguments:
		// arg0 = Entity targetname
		// arg1 = Input name
		// arg2 = Value
		// arg3 = Delay
		// Example:
		FireOutput "my_target" "MyInput" "MyValue" 0.0
	}

	// An example of a function callback (with paramaters):
	Function OnPlayerSpawned( myInput )
	{
		// Our player spawned!
		// Fire's an output to an entity.
		// Arguments:
		// arg0 = Entity targetname
		// arg1 = Input name
		// arg2 = Value
		// arg3 = Delay
		// Example:
		FireOutput "my_target" "MyInput" myInput 0.0
	}

	// An example of input being recieved:
	Function OnInputReceived( EntityID, InputName, InputValue )
	{
		// We recieved an input!
		// Arguments:
		// arg0 = Entity ID (entindex number)
		// arg1 = Input name
		// arg2 = Value
		// Example:
		PrintToConsole "We recieved an input from entity: {EntityID} with input name: {InputName} and value: {InputValue}"
	}

	// An example of output being sent:
	Function OnOutputSent( EntityID, OutputName, OutputValue, OutputDelay )
	{
		// We sent an output!
		// Arguments:
		// arg0 = Entity ID (entindex number)
		// arg1 = Output name
		// arg2 = Value
		// arg3 = Delay
		// Example:
		PrintToConsole "We sent an output to entity: {EntityID} with output name: {OutputName} and value: {OutputValue} after a delay of: {OutputDelay}"
	}
	*/

	// Let's parse our file.
	m_szFileName = szFile;
	m_Functions.clear();
	m_Commands.clear();

	// Make sure we get the full path to file!
	char szFullPath[1024];
	g_pFullFileSystem->GetLocalPath( szFile.c_str(), szFullPath, sizeof( szFullPath ) );

	// Open file
	std::ifstream file( szFullPath );
	if ( !file.is_open() )
		return;

	std::string line;
	IOFunctionData currentFunction;
	bool inFunction = false;
	bool inBlock = false;
	bool inElse = false;
	bool inElseIf = false;
	std::string inIf;
	bool inSwitch = false;
	int currentSwitchID = -1;
	int functionNextSwitchID = 0;
	int functionNextCaseID = 0;
	IORequirementStatements currentRequirement = IORequirementStatements::IF_EQUAL;

	while ( std::getline(file, line) )
	{
		line = Trim(line);
		if (line.empty() || line[0] == '/' || line[0] == '*')
			continue; // Skip comments and empty lines

		// Function header
		if (line.find("Function ") == 0)
		{
			if (inFunction && !currentFunction.FunctionName.empty())
			{
				m_Functions.push_back(currentFunction);
				currentFunction = IOFunctionData();
			}
			inFunction = true;
			inBlock = false;
			inIf.clear();
			inElse = false;
			inElseIf = false;
			inSwitch = false;
			currentSwitchID = -1;
			currentRequirement = IORequirementStatements::IF_EQUAL;

			// Reset switch/case ID counters for each function
			functionNextSwitchID = 0;
			functionNextCaseID = 0;

			// Parse function name and parameters
			size_t nameStart = strlen("Function ");
			size_t parenOpen = line.find('(', nameStart);
			size_t parenClose = line.find(')', nameStart);
			size_t braceOpen = line.find('{', nameStart);

			if (parenOpen != std::string::npos && parenClose != std::string::npos)
			{
				currentFunction.FunctionName = Trim(line.substr(nameStart, parenOpen - nameStart));
				std::string params = line.substr(parenOpen + 1, parenClose - parenOpen - 1);
				currentFunction.Parameters = Split(params, ',');
			}
			else
			{
				// No parameters
				size_t end = (braceOpen != std::string::npos) ? braceOpen : line.length();
				currentFunction.FunctionName = Trim(line.substr(nameStart, end - nameStart));
				currentFunction.Parameters.clear();
			}
			// If '{' is on the same line, enter block
			if (braceOpen != std::string::npos)
				inBlock = true;
			continue;
		}

		// Entering function block
		if (line == "{")
		{
			inBlock = true;
			continue;
		}
		// Exiting function block
		if (line == "}")
		{
			if (inFunction && !currentFunction.FunctionName.empty())
			{
				m_Functions.push_back(currentFunction);
				currentFunction = IOFunctionData();
			}
			inFunction = false;
			inBlock = false;
			continue;
		}

		// Inside function block: parse commands
		if (inFunction && inBlock)
		{
			// Remove inline comments
			size_t comment = line.find("//");
			if (comment != std::string::npos)
				line = Trim(line.substr(0, comment));
			if (line.empty())
				continue;

			// Command name is first word, rest are arguments
			std::istringstream iss(line);
			std::string cmdName;
			iss >> cmdName;
			IOFunctionCommand cmd;
			cmd.Require = inIf;
			cmd.RequireStatement = currentRequirement;
			cmd.IsElseIf = inElseIf;
			cmd.IsElse = inElse;
			cmd.SpawnItem.ListName.clear();
			cmd.SpawnItem.ItemName.clear();
			cmd.SpawnItem.Limit = 0;
			cmd.SwitchID = functionNextSwitchID - 1; // Belongs to the most recent switch
			cmd.CaseID = functionNextCaseID;

			// Identify command type (IOFunctionCommands_t)
			int commandType = -1;
			for (int i = 0; i < IO_MAX; ++i)
			{
				if (cmdName == g_IOCommands[i])
				{
					commandType = i;
					break;
				}
			}
			//IO_SPECIAL_FUNCTION
			cmd.Type = (IOFunctionCommands_t)commandType;

			// Parse arguments according to command type
			std::string restOfLine;
			std::getline(iss, restOfLine);
			restOfLine = Trim(restOfLine);

			switch (cmd.Type)
			{
				case IO_FIRE_OUTPUT:
				{
					// Example: FireOutput "my_target" "MyInput" "MyValue" 5.0
					// EntFire = arg0, Input = arg1, Message = arg2, Delay = arg3
				    auto args = SplitQoutes(restOfLine, ' ');
					if (args.size() > 0)
						cmd.EntFire = MessageCleanup( args[0] );
					if (args.size() > 1)
					    cmd.Input = MessageCleanup( args[1] );
					if (args.size() > 2)
						cmd.Message = MessageCleanup( args[2] );
					if (args.size() > 3)
						cmd.Delay = std::stof( args[3] );
					break;
				}
				case IO_WAIT:
				{
					// Example: Wait 2.5
					if (!restOfLine.empty())
						cmd.Delay = std::stof(restOfLine);
					break;
				}
			    case IO_BREAK:
				{
				    // Example: Break {param} if "MyValue"
				    // Message = arg0, Input = arg1
				    auto args = Split(restOfLine, ' ');
				    if ( args.size() < 2 )
					{
					    cmd.Message.clear();
					    cmd.Input.clear();
					    break;
					}
				    // Replace {param} with %sN%
					cmd.Message = ReplaceScriptArgs( MessageCleanup( args[0] ), currentFunction.Parameters );
				    cmd.Input = ReplaceScriptArgs( MessageCleanup( args[2] ), currentFunction.Parameters );
					break;
				}
			    case IO_IF:
			    case IO_ELSEIF:
				{
					// Example: if EntityName == "apple"
					// or if EntityName is "apple"
				    // or if PlayerCount >= 5
				    // or if PlayerCount > 5
				    // or if PlayerCount <= 5
					// This also applies to elseif and else
					auto args = Split(restOfLine, ' ');
					if ( args.size() < 2 )
						inIf = "_invalid_if_statement";
					else
					{
						if ( args[1] == "==" || args[1] == "is" )
						{
							inIf = MessageCleanup( args[2] );
						    currentRequirement = IORequirementStatements::IF_EQUAL;
						}
					    else if (args[1] == "!=" || args[1] == "not" || args[1] == "isnot")
						{
							inIf = MessageCleanup( args[2] );
						    currentRequirement = IORequirementStatements::IF_NOT_EQUAL;
						}
					    else if (args[1] == ">=")
						{
							inIf = MessageCleanup( args[2] );
						    currentRequirement = IORequirementStatements::IF_GREATER_EQUAL;
						}
					    else if (args[1] == "<=")
						{
							inIf = MessageCleanup( args[2] );
						    currentRequirement = IORequirementStatements::IF_LESS_EQUAL;
						}
						else if (args[1] == ">")
						{
							inIf = MessageCleanup( args[2] );
							currentRequirement = IORequirementStatements::IF_GREATER;
						}
						else if (args[1] == "<")
						{
							inIf = MessageCleanup( args[2] );
							currentRequirement = IORequirementStatements::IF_LESS;
					    }
					    // Make sure this is set correctly.
					    if ( cmd.Type == IO_ELSEIF )
							inElseIf = true;
						else
						    inElseIf = false;
					}
				    break;
				}
				case IO_ELSE:
				{
				    // Else has no condition.
				    // IsElse is already set above.
				    currentRequirement = IORequirementStatements::IF_INVALID;
				    inElseIf = false;
				    inElse = true;
				}
			    break;
			    case IO_END:
				{
					if ( !inIf.empty() )
						inIf.clear();
					// Also handle switch block end
					if ( inSwitch )
					{
						inSwitch = false;
						currentSwitchID = -1;
					}
				    break;
				}
			    case IO_EXEC_AS:
				{
					// Do nothing for now.
				    break;
				}
			    case IO_GIVE_ITEM:
				{
				    // Example: GiveItem 0 "item_name"
				    // arg0 = Player ID (0 for all players)
				    const std::string msg = ReplaceScriptArgs( restOfLine, currentFunction.Parameters );
				    auto args = Split( msg, ' ' );
					if ( args.size() < 2 ) break;
				    // We will store player ID in EntFire.
				    cmd.EntFire = args[0];
				    // Replace {param} with %sN%
					cmd.Message = ReplaceScriptArgs( MessageCleanup( args[1] ), currentFunction.Parameters );
				    break;
				}
				case IO_HAS_PARTICIPATED:
				{
					// Example: Participated 0 "ID_Name"
					// arg0 = Player ID (0 for all players)
					// arg1 = ID Name
					const std::string msg = ReplaceScriptArgs( restOfLine, currentFunction.Parameters );
				    auto args = Split(msg, ' ');
					if ( args.size() < 2 ) break;
					// We will store player ID in EntFire.
					cmd.EntFire = args[0];
				    // Replace {param} with %sN%
					cmd.Message = ReplaceScriptArgs( MessageCleanup( args[1] ), currentFunction.Parameters );
					break;
			    }
				case IO_ADD_TO_SPAWN_LIST:
				{
				    // Example: AddToSpawnList Ammo "item_name" 5
				    // arg0 = List name (Ammo/Weapons/Items)
					// arg1 = Item name
					// arg2 = Limit
					const std::string msg = ReplaceScriptArgs( restOfLine, currentFunction.Parameters );
				    auto args = Split(msg, ' ');
					if ( args.size() < 3 ) break;
					cmd.SpawnItem.ListName = MessageCleanup( args[0] );
					cmd.SpawnItem.ItemName = MessageCleanup( args[1] );
					cmd.SpawnItem.Limit = std::stoi( args[2] );
					break;
			    }
				case IO_PRINT_TO_CONSOLE:
				case IO_PRINT_TO_CHAT:
				{
					// Example: PrintToConsole "Message here"
					// Example: PrintToChat "Message here"
					// Replace {param} with %sN%
					cmd.Message = ReplaceScriptArgs( restOfLine, currentFunction.Parameters );
					break;
				}
				case IO_RANDOM:
				{
					// Example: Random 0.0 1.0 myVar
					// arg0 = min, arg1 = max, arg2 = variable name to store result
					auto args = Split(restOfLine, ' ');
					if (args.size() >= 3)
					{
						cmd.Message = MessageCleanup(args[0]); // min
						cmd.Input = MessageCleanup(args[1]);   // max
						cmd.VarName = MessageCleanup(args[2]); // variable name
					}
					break;
				}
				case IO_RANDOM_INT:
				{
					// Example: RandomInt 1 5 myVar
					// arg0 = min (int), arg1 = max (int), arg2 = variable name to store result
					auto args = Split(restOfLine, ' ');
					if (args.size() >= 3)
					{
						cmd.Message = MessageCleanup(args[0]); // min
						cmd.Input = MessageCleanup(args[1]);   // max
						cmd.VarName = MessageCleanup(args[2]); // variable name
					}
					break;
				}
				case IO_SWITCH:
				{
					// Example: switch myVar
					// arg0 = variable name or value to switch on
					cmd.SwitchValue = restOfLine;
				    cmd.SwitchID = functionNextSwitchID;
					cmd.CaseID = -1;
					cmd.IsDefault = false;
					functionNextSwitchID++;
					functionNextCaseID = 0; // Reset case counter for new switch
					
					// Enter switch block
					inSwitch = true;
					currentSwitchID = functionNextSwitchID - 1;
					break;
				}
				case IO_CASE:
				{
					// Example: case 0.5
					// arg0 = value to match
					cmd.CaseValue = restOfLine;
					cmd.SwitchID = functionNextSwitchID - 1; // Belongs to the most recent switch
					cmd.CaseID = functionNextCaseID;
					cmd.IsDefault = false;
					functionNextCaseID++;
					break;
				}
				case IO_DEFAULT:
				{
					// Default case - no value needed
					cmd.SwitchID = functionNextSwitchID - 1; // Belongs to the most recent switch
					cmd.CaseID = functionNextCaseID;
					cmd.IsDefault = true;
					functionNextCaseID++;
					break;
				}
				default:
					break;
			}

			// For commands inside a switch block, assign the current switch ID
			if (inSwitch && cmd.Type != IO_SWITCH && cmd.Type != IO_CASE && cmd.Type != IO_DEFAULT && cmd.Type != IO_END)
			{
				cmd.SwitchID = currentSwitchID;
			}

			if ( sv_ss_debug.GetBool() )
			{
				Msg( "Added Command: %s [%i]\n", g_IOCommands[cmd.Type], cmd.Type );
				if ( sv_ss_debug.GetInt() == 2 && cmd.SwitchID > -1 )
				{
					Msg( "Is in a switch statement.\n" );
					Msg( "SwitchValue: %s\n", cmd.SwitchValue.c_str() );
					Msg( "SwitchID: %i\n", cmd.SwitchID );
					Msg( "CaseID: %i\n", cmd.CaseID );
				}
			}

			currentFunction.Commands.push_back(cmd);
		}
	}
	// Push last function if file doesn't end with '}'
	if (inFunction && !currentFunction.FunctionName.empty())
		m_Functions.push_back(currentFunction);
	file.close();
}

void IOScriptFile::OnThink()
{
	for ( size_t i = 0; i < m_Commands.size(); i++ )
		RunCommands( i );
}

void IOScriptFile::OnRoundRestart()
{
	m_Commands.clear();
}

IOScriptFile::~IOScriptFile()
{
	m_Functions.clear();
}

ScriptCallBackEnum IOScriptFile::OnCalled(const std::string &szFunction, KeyValues *pData)
{
	// Fire an output variable
	if ( !pData ) return ScriptCall_Error;
	if ( FStrEq( pData->GetString( "arg0" ), "Function" ) )
		return CallData( szFunction, pData->GetString( "arg1" ) );

	CBaseEntity *pEnt = nullptr;
	edict_t *pEdict = INDEXENT( atoi( pData->GetString( "arg0" ) ) );
	if ( pEdict && !pEdict->free )
		pEnt = CBaseEntity::Instance( pEdict );
	if ( pData->GetBool( "IsInput" ) )
		return OnInput( pEnt, szFunction, pData->GetString( "arg1" ) );
	else
		return OnOutput( pEnt, szFunction, pData->GetString( "arg1" ), atof( pData->GetString( "arg2" ) ) );
}

ScriptCallBackEnum IOScriptFile::OnOutput( CBaseEntity *pEnt, const std::string &szAction, const std::string &szValue, const float &szDelay )
{
	if ( !pEnt ) return ScriptCall_Error;
	return CallData( IO_ON_OUTPUT_SENT, UTIL_VarArgs( "%i, %s, %s, %s, %f", pEnt->entindex(), STRING( pEnt->pev->targetname ), szAction.c_str(), szValue.c_str(), szDelay ) );
}

ScriptCallBackEnum IOScriptFile::OnInput( CBaseEntity *pEnt, const std::string &szAction, const std::string &szValue )
{
	if ( !pEnt ) return ScriptCall_Error;
	ScriptCallBackEnum nRet = CallData( IO_ON_INPUT_RECEIVED, UTIL_VarArgs( "%i, %s, %s", pEnt->entindex(), szAction.c_str(), szValue.c_str() ) );

	KeyValues *pScriptCall = new KeyValues( "Items" );
	pScriptCall->SetString( "Action", szAction.c_str() );
	pScriptCall->SetString( "arg0", szValue.c_str() );
	pEnt->ScriptCallback( pScriptCall );
	pScriptCall->deleteThis();
	return nRet;
}

ScriptCallBackEnum IOScriptFile::CallData( const std::string &szFunction )
{
	// Let's call our function with no arguments.
	return CallData( szFunction, "" );
}

ScriptCallBackEnum IOScriptFile::CallData( const std::string &szFunction, const std::string &szArgs )
{
	ScriptCallBackEnum nRet = ScriptCall_Warning;
	// We need this if we have more than 1 wait command.
	float flPreviousWait = 0.0f;
	for ( size_t i = 0; i < m_Functions.size(); i++ )
	{
		// Get our function data
		IOFunctionData function = m_Functions[i];
		if ( function.FunctionName != szFunction ) continue;

		// Let's add our command to our vector.
		// We must also apply the Input call
		IOFunctionCall IOCall;
		IOCall.ID = GetCurrentID() + 1;
		IOCall.Arguments = Split(szArgs, ',');

		if ( IOCall.Arguments.size() > 0 )
		{
			// Very simplified for now
			if ( szFunction == "OnOutputSent" )
				IOCall.InputCall = IOCall.Arguments[1];
			else
				IOCall.InputCall = IOCall.Arguments[0];
		}
		else
			IOCall.InputCall = "";

		// Add our commands
		for ( size_t y = 0; y < function.Commands.size(); y++ )
		{
			IOFunctionCommand cmd = function.Commands[y];
			// Only wait needs this
			if ( cmd.Type == IO_WAIT )
			{
				float flDelay = cmd.Delay;
				cmd.Delay = gpGlobals->time + flDelay + flPreviousWait;
				flPreviousWait += flDelay;
			}
			IOCall.Commands.push_back( cmd );
		}

		m_Commands.push_back( IOCall );

		// We found our function, return OK
		nRet = ScriptCall_OK;
	}
	return nRet;
}

ScriptCallBackEnum IOScriptFile::CallData( const IOFunctions_t &nFunction, const std::string &szArgs )
{
	return CallData( g_IOFunctions[ nFunction ], szArgs );
}

void IOScriptFile::RunCommands( int nID )
{
	IOFunctionCall &pFunctionCall = m_Commands[ nID ];
	// If we have commands to execute, then let's do that.
	if ( pFunctionCall.Commands.size() > 0 )
	{
		IOFunctionCommand cmd = pFunctionCall.Commands[ 0 ];

		// Apply variable replacement to command strings (both function args and runtime variables)
		auto ApplyVariables = [&](std::string str) -> std::string {
			str = GetArgValues(str, pFunctionCall.Arguments);
			str = ReplaceVariables(str);
			return str;
		};

		// Handle switch block: if we're in a switch block but haven't matched a case yet,
		// skip all commands except CASE, DEFAULT, and END (for the correct switch)
		if ( pFunctionCall.InsideSwitchBlock != SWITCHBLOCK_NONE && 
		     !pFunctionCall.SwitchMatched &&
		     cmd.Type != IO_CASE && 
		     cmd.Type != IO_DEFAULT && 
		     cmd.Type != IO_END )
		{
			// Skip if the command do not belong to the correct SwitchID
			if (cmd.SwitchID == -1 || cmd.SwitchID != pFunctionCall.CurrentSwitchID)
			{
				pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
				return;
			}

			// Now we do the same for the CaseID
			if (cmd.CaseID == -1 || cmd.CaseID != atoi( pFunctionCall.SwitchValue.c_str() ))
			{
				pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
				return;
			}
		}

		// Also skip commands that belong to a different case within the same switch
		if ( pFunctionCall.InsideSwitchBlock != SWITCHBLOCK_NONE && 
			pFunctionCall.SwitchMatched &&
			pFunctionCall.InCaseBlock &&
		    ( cmd.SwitchID == -1 || cmd.SwitchID != pFunctionCall.CurrentSwitchID ) &&
			( cmd.CaseID == -1 || cmd.CaseID != atoi( pFunctionCall.SwitchValue.c_str() ) )
			)
		{
			// This command belongs to a different case, skip it
			pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
			return;
		}

		// Not empty? check what we require
		if ( !cmd.Require.empty() )
		{
			bool bMatch = true;
			// Make sure what we require is the same as our input call
			if ( cmd.Require != pFunctionCall.InputCall )
				bMatch = false;

			// Maybe it's a argument value?
			if ( !bMatch && cmd.RequireStatement != IORequirementStatements::IF_EQUAL )
			{
				// Reset, and try again.
				bMatch = true;

				const std::string &szValue = pFunctionCall.InputCall;
				const std::string &szArgument = cmd.Require;

				// Check our requirement statement (only for numeric values)
				if ( cmd.RequireStatement == IORequirementStatements::IF_GREATER )
				{
					if ( atoi( szValue.c_str() ) <= atoi( szArgument.c_str() ) )
						bMatch = false;
				}
				else if ( cmd.RequireStatement == IORequirementStatements::IF_LESS )
				{
					if ( atoi( szValue.c_str() ) >= atoi( szArgument.c_str() ) )
						bMatch = false;
				}
				else if ( cmd.RequireStatement == IORequirementStatements::IF_GREATER_EQUAL )
				{
					if ( atoi( szValue.c_str() ) < atoi( szArgument.c_str() ) )
						bMatch = false;
				}
				else if ( cmd.RequireStatement == IORequirementStatements::IF_LESS_EQUAL )
				{
					if ( atoi( szValue.c_str() ) > atoi( szArgument.c_str() ) )
						bMatch = false;
				}
				else if ( cmd.RequireStatement == IORequirementStatements::IF_NOT_EQUAL )
				{
					if ( szValue == szArgument )
						bMatch = false;
					else if ( cmd.Require == szArgument )
						bMatch = false;
				}
			}

			// If we don't match, erase this command and return.
			if ( !bMatch )
			{
				pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
				return;
			}

			// We match, but are we on the correct block?
			switch ( pFunctionCall.InsideIfBlock )
			{
				case IOFunctionCallIfBlockStatements::IFBLOCK_IF:
					// We are inside an if block, but is it the correct one?
					if ( cmd.Type != IO_IF )
					{
						// Not the correct one, erase and return.
						pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
						return;
					}
				break;
				case IOFunctionCallIfBlockStatements::IFBLOCK_ELSEIF:
					// We are inside an elseif block, but is it the correct one?
					if ( cmd.Type != IO_ELSEIF )
					{
						// Not the correct one, erase and return.
						pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
						return;
					}
				break;
				case IOFunctionCallIfBlockStatements::IFBLOCK_ELSE:
					// We are inside an else block, but is it the correct one?
					if ( cmd.Type != IO_ELSE )
					{
						// Not the correct one, erase and return.
						pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
						return;
					}
				break;
			}

			// We are inside an if block now, let's check what type we are.
			switch( cmd.Type )
			{
				case IO_IF: pFunctionCall.InsideIfBlock = IOFunctionCallIfBlockStatements::IFBLOCK_IF; break;
				case IO_ELSE: pFunctionCall.InsideIfBlock = IOFunctionCallIfBlockStatements::IFBLOCK_ELSE; break;
				case IO_ELSEIF: pFunctionCall.InsideIfBlock = IOFunctionCallIfBlockStatements::IFBLOCK_ELSEIF; break;
			}
		}

		if ( cmd.Type == IO_BREAK )
		{
			// Check if this is a switch break (no condition, just "break")
			std::string szArgument = ApplyVariables(cmd.Message);
			std::string szInput = ApplyVariables(cmd.Input);
			
			// If we're in a case block and break has no meaningful condition, exit switch
			if ( pFunctionCall.InCaseBlock && cmd.SwitchID == pFunctionCall.CurrentSwitchID && (szArgument.empty() || szArgument == szInput) )
			{
				// Exit the switch block - skip to END
				pFunctionCall.InsideSwitchBlock = SWITCHBLOCK_SWITCH;
				pFunctionCall.SwitchValue.clear();
				pFunctionCall.SwitchMatched = false;
				pFunctionCall.CurrentSwitchID = -1;
				pFunctionCall.CurrentCaseID = -1;
				pFunctionCall.InCaseBlock = false;
			}
			// Original conditional break behavior
			else if ( szArgument == szInput )
			{
				pFunctionCall.Commands.clear();
				return;
			}
		}

		switch ( cmd.Type )
		{
			case IO_FIRE_OUTPUT:
			{
				// Let's fire an output!
				// Make sure it's I/O
				// We don't use the entity index here, since we don't want to call ourselves and make an infinite loop (if it happens)
			    const std::string &szArg0( "SCall" );
			    const std::string szArg1 = ApplyVariables(cmd.EntFire);
			    const std::string szArg2 = ApplyVariables(cmd.Message);
			    ScriptSystem::CallScriptDelay( AvailableScripts_t::InputOutput, nullptr, cmd.Input, cmd.Delay, 3, szArg0, szArg1, szArg2 );
			}
			break;

			case IO_EXEC_AS:
				// Doesn't do anything for now.
			break;

			case IO_GIVE_ITEM:
				// Give item to player(s)
				{
					std::string szPlayer = ApplyVariables(cmd.EntFire);
					int nPlayerID = atoi( szPlayer.c_str() );
					std::string szItem = ApplyVariables(cmd.Message);
					int iszItem = ALLOC_STRING( szItem.c_str() ); // Make a copy of the classname
					if ( nPlayerID == 0 )
					{
						// Give to all players
						for ( int i = 1; i <= gpGlobals->maxClients; i++ )
						{
							CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
							if ( pPlayer )
								pPlayer->GiveNamedItem( STRING(iszItem) );
						}
					}
					else
					{
						CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( nPlayerID );
						if ( pPlayer )
							pPlayer->GiveNamedItem( STRING(iszItem) );
					}
			    }
			break;

			case IO_HAS_PARTICIPATED:
			{
				std::string szPlayer = ApplyVariables(cmd.EntFire);
				int nPlayerID = atoi( szPlayer.c_str() );
				std::string szIDName = ApplyVariables(cmd.Message);
				DialogAchievementData ach = GetAchievementByID( szIDName.c_str() );
				if ( nPlayerID == 0 )
				{
					for ( int i = 1; i <= gpGlobals->maxClients; i++ )
					{
						CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
						if ( pPlayer )
							pPlayer->Participated( ach.GetAchievementID() );
					}
				}
				else
				{
					CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( nPlayerID );
					if ( pPlayer )
						pPlayer->Participated( ach.GetAchievementID() );
				}
		    }
		    break;

			case IO_ADD_TO_SPAWN_LIST:
			{
				// Let's add our spawn item to our list.
				if ( !cmd.SpawnItem.ListName.empty() && !cmd.SpawnItem.ItemName.empty() && cmd.SpawnItem.Limit > 0 )
					s_SpawnListData.push_back( cmd.SpawnItem );
		    }
		    break;

			case IO_SPAWN_ITEMS:
			{
				KeyValuesAD mySpawnItems( "SpawnItems" );
			    // Let's populare our spawn items.
				for ( size_t i = 0; i < s_SpawnListData.size(); i++ )
			    {
					ISpawnListData item = s_SpawnListData[i];
					KeyValuesAD pItem( item.ListName.c_str() );
					pItem->SetString( "Classname", item.ItemName.c_str() );
					pItem->SetInt( "Amount", item.Limit );
					mySpawnItems->AddSubKey( pItem->MakeCopy() );
			    }
			    // Let's spawn our items now.
				ZP::IO_CalculatePlayerAmount( mySpawnItems );
				// Clear our list after use.
				s_SpawnListData.clear();
			}
		    break;

			case IO_SPAWN_MELEE_ITEMS:
			{
			    // Setup default melee spawn list
			    ZP::SetupDefaultMeleeSpawnList();
			}
		    break;

			case IO_RANDOM:
			{
				// Random min max varName - generates random float between min and max, stores in variable
				float flMin = std::stof(cmd.Message);
				float flMax = std::stof(cmd.Input);
				std::string varName = cmd.VarName;
				
				// Generate random float
				float flRandom = flMin + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (flMax - flMin)));
				
				// Store in variable
				SetVariable(varName, std::to_string(flRandom));
			}
			break;

			case IO_RANDOM_INT:
			{
				// RandomInt min max varName - generates random integer between min and max (inclusive), stores in variable
				int iMin = std::stoi(cmd.Message);
				int iMax = std::stoi(cmd.Input);
				std::string varName = cmd.VarName;
				
				// Generate random integer (inclusive)
				int iRandom = iMin + rand() % (iMax - iMin + 1);
				
				// Store in variable
				SetVariable(varName, std::to_string(iRandom));
			}
			break;
			case IO_SWITCH:
			{
				// switch varName - start a switch block
				// Get the value of the variable (or use literal)
				std::string switchVar = cmd.SwitchValue;
				std::string value = GetVariable(switchVar);
				if (value.empty())
					value = switchVar; // Use literal if not a variable
				
				// Record the switch ID for this switch block
				pFunctionCall.CurrentSwitchID = cmd.SwitchID;
				pFunctionCall.CurrentCaseID = -1; // Not in a case yet
				pFunctionCall.InsideSwitchBlock = SWITCHBLOCK_SWITCH;
				pFunctionCall.SwitchValue = value;
				pFunctionCall.SwitchMatched = false;
				pFunctionCall.InCaseBlock = false;
			}
			break;

		case IO_CASE:
		{
			// case value - check if matches switch value
			// Only process if this case belongs to our current switch
			if (pFunctionCall.InsideSwitchBlock != SWITCHBLOCK_NONE && cmd.SwitchID == pFunctionCall.CurrentSwitchID)
			{
				std::string caseValue = cmd.CaseValue;
				// Check if case value matches switch value
				if (!pFunctionCall.SwitchMatched && caseValue == pFunctionCall.SwitchValue)
				{
					pFunctionCall.SwitchMatched = true;
					pFunctionCall.InsideSwitchBlock = SWITCHBLOCK_CASE;
					pFunctionCall.CurrentCaseID = cmd.CaseID;
					pFunctionCall.InCaseBlock = true; // We're now inside a matched case
				}
				else if (pFunctionCall.InCaseBlock && cmd.CaseID == pFunctionCall.CurrentCaseID)
				{
					// We hit the same case again (shouldn't happen normally)
					pFunctionCall.Commands.erase(pFunctionCall.Commands.begin());
					return;
				}
				else if (pFunctionCall.InCaseBlock && cmd.CaseID > pFunctionCall.CurrentCaseID)
				{
					// We were in a case block, but hit another case - 
					// This means the previous case didn't have a break
					// Exit the switch block entirely
					pFunctionCall.InsideSwitchBlock = SWITCHBLOCK_SWITCH;
					pFunctionCall.SwitchValue.clear();
					pFunctionCall.SwitchMatched = false;
					pFunctionCall.CurrentSwitchID = -1;
					pFunctionCall.CurrentCaseID = -1;
					pFunctionCall.InCaseBlock = false;
					pFunctionCall.Commands.erase(pFunctionCall.Commands.begin());
					return;
				}
				else
				{
					// Not matched, skip this case's commands
					pFunctionCall.Commands.erase(pFunctionCall.Commands.begin());
					return;
				}
			}
			else
			{
				// Not in the right switch block, skip
				pFunctionCall.Commands.erase(pFunctionCall.Commands.begin());
				return;
			}
		}
		break;

	case IO_DEFAULT:
	{
		// default - execute if no case matched
		if (pFunctionCall.InsideSwitchBlock != SWITCHBLOCK_NONE && cmd.SwitchID == pFunctionCall.CurrentSwitchID)
		{
			if (!pFunctionCall.SwitchMatched)
			{
				pFunctionCall.SwitchMatched = true;
				pFunctionCall.InsideSwitchBlock = SWITCHBLOCK_DEFAULT;
				pFunctionCall.CurrentCaseID = cmd.CaseID;
				pFunctionCall.InCaseBlock = true; // We're now inside default case
			}
			else
			{
				// Already matched a case, skip default
				pFunctionCall.Commands.erase(pFunctionCall.Commands.begin());
				return;
			}
		}
		else
		{
			// Not in the right switch block, skip
			pFunctionCall.Commands.erase(pFunctionCall.Commands.begin());
			return;
		}
	}
	break;

	case IO_END:
	{
		// We reached the end of the if block, reset it.
		if ( pFunctionCall.InsideIfBlock != IFBLOCK_NONE )
			pFunctionCall.InsideIfBlock = IFBLOCK_NONE;
		// Also handle end of switch block - only if it's our switch
		if ( pFunctionCall.InsideSwitchBlock != SWITCHBLOCK_NONE && cmd.SwitchID == pFunctionCall.CurrentSwitchID )
		{
			pFunctionCall.InsideSwitchBlock = SWITCHBLOCK_NONE;
			pFunctionCall.SwitchValue.clear();
			pFunctionCall.SwitchMatched = false;
			pFunctionCall.CurrentSwitchID = -1;
			pFunctionCall.CurrentCaseID = -1;
			pFunctionCall.InCaseBlock = false;
		}
	}
	break;

			break;

			case IO_WAIT:
			{
				// We still have delay, don't go to the next command until we are done.
			    if ( cmd.Delay > gpGlobals->time )
					return;
			}
			break;

			case IO_PRINT_TO_CHAT:
			{
			    std::string szOutput = ApplyVariables(cmd.Message);
			    szOutput = MessageCleanup( szOutput ) + "\n";
				UTIL_ClientPrintAll( HUD_PRINTTALK, szOutput.c_str() );
			}
			break;

			case IO_PRINT_TO_CONSOLE:
			{
			    std::string szOutput = ApplyVariables(cmd.Message);
			    szOutput = MessageCleanup( szOutput ) + "\n";
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				    if ( pPlayer )
						UTIL_PrintConsole( szOutput.c_str(), pPlayer );
				}
			}
			break;
		}

		// Erase after use
		pFunctionCall.Commands.erase( pFunctionCall.Commands.begin() );
	}
	else
		m_Commands.erase( m_Commands.begin() + nID );
}

uint IOScriptFile::GetCurrentID() const
{
	if ( m_Commands.size() == 0 ) return -1;
	return m_Commands[ m_Commands.size() - 1 ].ID;
}

// Variable management methods
void IOScriptFile::SetVariable( const std::string &szName, const std::string &szValue )
{
	m_Variables[szName] = szValue;
}

std::string IOScriptFile::GetVariable( const std::string &szName ) const
{
	auto it = m_Variables.find(szName);
	if (it != m_Variables.end())
		return it->second;
	return "";
}

std::string IOScriptFile::ReplaceVariables( const std::string &str ) const
{
	std::string result = str;
	for (const auto &pair : m_Variables)
	{
		std::string pattern = "{" + pair.first + "}";
		size_t pos = 0;
		while ((pos = result.find(pattern, pos)) != std::string::npos)
		{
			result.replace(pos, pattern.length(), pair.second);
			pos += pair.second.length();
		}
	}
	return result;
}

const char *IO_GetAvailableFunctions( IOFunctions_t nFunc )
{
	return g_IOFunctions[ nFunc ];
}

const char *IO_GetAvailableFunctionCommands( IOFunctionCommands_t nCommand )
{
	return g_IOCommands[ nCommand ];
}
