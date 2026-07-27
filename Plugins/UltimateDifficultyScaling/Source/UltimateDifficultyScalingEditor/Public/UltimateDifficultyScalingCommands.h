// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "Framework/Commands/Commands.h"
#include "UltimateDifficultyScalingStyle.h"

class FUltimateDifficultyScalingCommands : public TCommands<FUltimateDifficultyScalingCommands>
{
public:

	FUltimateDifficultyScalingCommands()
		: TCommands<FUltimateDifficultyScalingCommands>(TEXT("UltimateDifficultyScaling"), NSLOCTEXT("Contexts", "UltimateDifficultyScaling", "UltimateDifficultyScaling Plugin"), NAME_None, FUltimateDifficultyScalingStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
