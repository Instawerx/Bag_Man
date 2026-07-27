// Copyright 2025, BlueprintsLab, All rights reserved

#include "UltimateDifficultyScalingStyle.h"
#include "UltimateDifficultyScalingEditor.h" // <-- FIX: Changed to the new header name
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FUltimateDifficultyScalingStyle::StyleInstance = nullptr;

void FUltimateDifficultyScalingStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FUltimateDifficultyScalingStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FUltimateDifficultyScalingStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("UltimateDifficultyScalingStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FUltimateDifficultyScalingStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("UltimateDifficultyScalingStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("UltimateDifficultyScaling")->GetBaseDir() / TEXT("Resources"));

	Style->Set("UltimateDifficultyScaling.PluginAction", new IMAGE_BRUSH(TEXT("UDS logo"), Icon20x20));

	return Style;
}

void FUltimateDifficultyScalingStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FUltimateDifficultyScalingStyle::Get()
{
	return *StyleInstance;
}