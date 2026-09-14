// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonUIExtensions.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "CommonLocalPlayer.h"
#include "Engine/GameInstance.h"
#include "GameUIManagerSubsystem.h"
#include "GameUIPolicy.h"
#include "LogCommonGame.h"
#include "PrimaryGameLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonUIExtensions)

int32 UCommonUIExtensions::InputSuspensions = 0;

ECommonInputType UCommonUIExtensions::GetOwningPlayerInputType(const UUserWidget* WidgetContextObject)
{
	if (WidgetContextObject)
	{
		if (const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(WidgetContextObject->GetOwningLocalPlayer()))
		{
			return InputSubsystem->GetCurrentInputType();
		}
	}

	return ECommonInputType::Count;
}

bool UCommonUIExtensions::IsOwningPlayerUsingTouch(const UUserWidget* WidgetContextObject)
{
	if (WidgetContextObject)
	{
		if (const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(WidgetContextObject->GetOwningLocalPlayer()))
		{
			return InputSubsystem->GetCurrentInputType() == ECommonInputType::Touch;
		}
	}
	return false;
}

bool UCommonUIExtensions::IsOwningPlayerUsingGamepad(const UUserWidget* WidgetContextObject)
{
	if (WidgetContextObject)
	{
		if (const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(WidgetContextObject->GetOwningLocalPlayer()))
		{
			return InputSubsystem->GetCurrentInputType() == ECommonInputType::Gamepad;
		}
	}
	return false;
}

UCommonActivatableWidget* UCommonUIExtensions::PushContentToLayer_ForPlayer(const ULocalPlayer* LocalPlayer, FGameplayTag LayerName, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!ensure(LocalPlayer) || !ensure(WidgetClass != nullptr))
	{
		return nullptr;
	}

	if (UGameUIManagerSubsystem* UIManager = LocalPlayer->GetGameInstance()->GetSubsystem<UGameUIManagerSubsystem>())
	{
		if (UGameUIPolicy* Policy = UIManager->GetCurrentUIPolicy())
		{
			if (UPrimaryGameLayout* RootLayout = Policy->GetRootLayout(CastChecked<UCommonLocalPlayer>(LocalPlayer)))
			{
				return RootLayout->PushWidgetToLayerStack(LayerName, WidgetClass);
			}
		}
	}

	return nullptr;
}

void UCommonUIExtensions::PushStreamedContentToLayer_ForPlayer(const ULocalPlayer* LocalPlayer, FGameplayTag LayerName, TSoftClassPtr<UCommonActivatableWidget> WidgetClass)
{
	if (!ensure(LocalPlayer) || !ensure(!WidgetClass.IsNull()))
	{
		return;
	}

	if (UGameUIManagerSubsystem* UIManager = LocalPlayer->GetGameInstance()->GetSubsystem<UGameUIManagerSubsystem>())
	{
		if (UGameUIPolicy* Policy = UIManager->GetCurrentUIPolicy())
		{
			if (UPrimaryGameLayout* RootLayout = Policy->GetRootLayout(CastChecked<UCommonLocalPlayer>(LocalPlayer)))
			{
				const bool bSuspendInputUntilComplete = true;
				RootLayout->PushWidgetToLayerStackAsync(LayerName, bSuspendInputUntilComplete, WidgetClass);
			}
		}
	}
}

void UCommonUIExtensions::PopContentFromLayer(UCommonActivatableWidget* ActivatableWidget)
{
	if (!ActivatableWidget)
	{
		// Ignore request to pop an already deleted widget
		return;
	}

	if (const ULocalPlayer* LocalPlayer = ActivatableWidget->GetOwningLocalPlayer())
	{
		if (const UGameUIManagerSubsystem* UIManager = LocalPlayer->GetGameInstance()->GetSubsystem<UGameUIManagerSubsystem>())
		{
			if (const UGameUIPolicy* Policy = UIManager->GetCurrentUIPolicy())
			{
				if (UPrimaryGameLayout* RootLayout = Policy->GetRootLayout(CastChecked<UCommonLocalPlayer>(LocalPlayer)))
				{
					RootLayout->FindAndRemoveWidgetFromLayer(ActivatableWidget);
				}
			}
		}
	}
}

ULocalPlayer* UCommonUIExtensions::GetLocalPlayerFromController(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		return Cast<ULocalPlayer>(PlayerController->Player);
	}

	return nullptr;
}

FName UCommonUIExtensions::SuspendInputForPlayer(APlayerController* PlayerController, FName SuspendReason)
{
	// IRONICS P0 2026-09-13: a suspend that resolves a LocalPlayer and a resume that does NOT (stale owning
	// player across a map load) leaves the CommonInput filter blocking mouse+keyboard+gamepad for the rest of
	// the session. Log both ends so a leak is visible in the shipping log.
	if (!PlayerController)
	{
		UE_LOG(LogCommonGame, Log, TEXT("SuspendInputForPlayer(%s): NO PlayerController -> nothing suspended (token None)."), *SuspendReason.ToString());
	}
	return SuspendInputForPlayer(PlayerController ? PlayerController->GetLocalPlayer() : nullptr, SuspendReason);
}

FName UCommonUIExtensions::SuspendInputForPlayer(ULocalPlayer* LocalPlayer, FName SuspendReason)
{
	if (UCommonInputSubsystem* CommonInputSubsystem = UCommonInputSubsystem::Get(LocalPlayer))
	{
		InputSuspensions++;
		FName SuspendToken = SuspendReason;
		SuspendToken.SetNumber(InputSuspensions);

		CommonInputSubsystem->SetInputTypeFilter(ECommonInputType::MouseAndKeyboard, SuspendToken, true);
		CommonInputSubsystem->SetInputTypeFilter(ECommonInputType::Gamepad, SuspendToken, true);
		CommonInputSubsystem->SetInputTypeFilter(ECommonInputType::Touch, SuspendToken, true);

		UE_LOG(LogCommonGame, Log, TEXT("SuspendInputForPlayer: token %s on %s -> MouseAndKeyboard filter now %s"),
			*SuspendToken.ToString(), *GetNameSafe(LocalPlayer),
			CommonInputSubsystem->GetInputTypeFilter(ECommonInputType::MouseAndKeyboard) ? TEXT("BLOCKED") : TEXT("open"));
		return SuspendToken;
	}

	UE_LOG(LogCommonGame, Log, TEXT("SuspendInputForPlayer(%s): no CommonInputSubsystem for %s -> nothing suspended (token None)."),
		*SuspendReason.ToString(), *GetNameSafe(LocalPlayer));
	return NAME_None;
}

void UCommonUIExtensions::ResumeInputForPlayer(APlayerController* PlayerController, FName SuspendToken)
{
	if (!PlayerController && SuspendToken != NAME_None)
	{
		// The leak shape: a real token, nobody to resume it on. Warning so it stands out in any log.
		UE_LOG(LogCommonGame, Warning, TEXT("ResumeInputForPlayer: token %s but NO PlayerController -> the CommonInput filter is NOT cleared (input stays blocked)."), *SuspendToken.ToString());
	}
	ResumeInputForPlayer(PlayerController ? PlayerController->GetLocalPlayer() : nullptr, SuspendToken);
}

void UCommonUIExtensions::ResumeInputForPlayer(ULocalPlayer* LocalPlayer, FName SuspendToken)
{
	if (SuspendToken == NAME_None)
	{
		return;
	}

	if (UCommonInputSubsystem* CommonInputSubsystem = UCommonInputSubsystem::Get(LocalPlayer))
	{
		CommonInputSubsystem->SetInputTypeFilter(ECommonInputType::MouseAndKeyboard, SuspendToken, false);
		CommonInputSubsystem->SetInputTypeFilter(ECommonInputType::Gamepad, SuspendToken, false);
		CommonInputSubsystem->SetInputTypeFilter(ECommonInputType::Touch, SuspendToken, false);

		const bool bStillBlocked = CommonInputSubsystem->GetInputTypeFilter(ECommonInputType::MouseAndKeyboard);
		UE_LOG(LogCommonGame, Log, TEXT("ResumeInputForPlayer: token %s on %s -> MouseAndKeyboard filter now %s%s"),
			*SuspendToken.ToString(), *GetNameSafe(LocalPlayer), bStillBlocked ? TEXT("BLOCKED") : TEXT("open"),
			bStillBlocked ? TEXT(" (another token still holds it)") : TEXT(""));
	}
	else
	{
		UE_LOG(LogCommonGame, Warning, TEXT("ResumeInputForPlayer: token %s but no CommonInputSubsystem for %s -> filter NOT cleared."),
			*SuspendToken.ToString(), *GetNameSafe(LocalPlayer));
	}
}

