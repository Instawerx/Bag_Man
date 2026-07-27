// Copyright 2025, BlueprintsLab, All rights reserved

#include "UltimateDifficultyScalingCommands.h"

#define LOCTEXT_NAMESPACE "FUltimateDifficultyScalingModule"

void FUltimateDifficultyScalingCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "Ultimate Difficulty Scaling", "Open the Ultimate Difficulty Scaling tool window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
