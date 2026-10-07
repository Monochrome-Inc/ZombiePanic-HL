// zp_example.io - Example script demonstrating Random and Switch/Case system
// 
// Syntax:
//   Random <min> <max> <variableName>     - Generates random float between min and max, stores in variable
//   RandomInt <min> <max> <variableName>  - Generates random integer between min and max (inclusive), stores in variable
//   switch <variableName>                 - Start a switch block on a variable
//   case <value>                          - Case to match (numeric or string)
//   default                               - Default case (when no case matches)
//   break                                 - Exit the switch block (use inside a case)
//   end                                   - End of if/switch block
//
// Variables can be used in commands with {variableName} syntax

Function OnRoundStart()
{
	// Example 1: Using Random with if statements (original approach)
	Random 0.0 1.0 randVal1
	if randVal1 <= 0.3
		FireOutput "barricade1" "PlaceBarricade" "" 0.0
	end

	Random 0.0 1.0 randVal2
	if randVal2 <= 0.2
		FireOutput "barricade2" "PlaceBarricade" "" 0.0
	end

	Random 0.0 1.0 randVal3
	if randVal3 <= 0.4
		FireOutput "barricade3" "PlaceBarricade" "" 0.0
	end

	Random 0.0 1.0 randVal4
	if randVal4 <= 0.6
		FireOutput "barricade4" "PlaceBarricade" "" 0.0
	end

	// Example 2: Using Switch/Case with break for clean random selection
	// Generate a random integer 1-4 for different barricade configurations
	RandomInt 1 4 barricadeConfig
	
	switch barricadeConfig
		case 1
			PrintToChat "Config 1: Light barricades"
			FireOutput "barricade1" "PlaceBarricade" "" 0.0
			FireOutput "barricade2" "PlaceBarricade" "" 0.0
			break
		case 2
			PrintToChat "Config 2: Medium barricades"
			FireOutput "barricade1" "PlaceBarricade" "" 0.0
			FireOutput "barricade2" "PlaceBarricade" "" 0.0
			FireOutput "barricade3" "PlaceBarricade" "" 0.0
			break
		case 3
			PrintToChat "Config 3: Heavy barricades"
			FireOutput "barricade1" "PlaceBarricade" "" 0.0
			FireOutput "barricade2" "PlaceBarricade" "" 0.0
			FireOutput "barricade3" "PlaceBarricade" "" 0.0
			FireOutput "barricade4" "PlaceBarricade" "" 0.0
			break
		case 4
			PrintToChat "Config 4: Custom setup"
			FireOutput "barricade1" "PlaceBarricade" "" 0.0
			FireOutput "barricade3" "PlaceBarricade" "" 0.0
			break
		default
			PrintToChat "Default config"
			FireOutput "barricade1" "PlaceBarricade" "" 0.0
	end

	// Example 3: Switch on difficulty with break
	RandomInt 1 3 difficultyLevel
	switch difficultyLevel
		case 1
			PrintToConsole "Difficulty: Easy"
			FireOutput "bot_manager" "SetDifficulty" "easy" 0.0
			break
		case 2
			PrintToConsole "Difficulty: Normal"
			FireOutput "bot_manager" "SetDifficulty" "normal" 0.0
			break
		case 3
			PrintToConsole "Difficulty: Hard"
			FireOutput "bot_manager" "SetDifficulty" "hard" 0.0
			break
		default
			PrintToConsole "Difficulty: Unknown, using Normal"
			FireOutput "bot_manager" "SetDifficulty" "normal" 0.0
	end

	// Example 4: Conditional break (original behavior still works)
	// Break out of function if condition met
	RandomInt 1 10 specialEvent
	if specialEvent == 10
		PrintToChat "RARE EVENT TRIGGERED!"
		FireOutput "rare_event" "Trigger" "" 0.0
		break  // Exit function early
	end
}

Function OnOutputSent( EntityID, EntityName, OutputName, OutputValue, OutputDelay )
{
	if EntityName is "kill_radio"
		FireOutput "radio" "TurnOff" "" 0.0
	end
}

// Example: Using variables in FireOutput
Function OnMapStart()
{
	RandomInt 10 100 randomDelay
	PrintToConsole "Random delay for event: {randomDelay}"
	FireOutput "event_relay" "Trigger" "" {randomDelay}
}