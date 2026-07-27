// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FUltimateDifficultyScalingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command. */
	void PluginButtonClicked();

private:

	void RegisterMenus();


private:
	TSharedPtr<class FUICommandList> PluginCommands;
};