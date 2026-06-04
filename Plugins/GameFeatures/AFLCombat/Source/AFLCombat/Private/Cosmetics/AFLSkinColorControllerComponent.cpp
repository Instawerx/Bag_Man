// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorControllerComponent.h"

#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSkinColorControllerComponent)

void UAFLSkinColorControllerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (AController* OwningController = GetController<AController>())
		{
			// AController::OnPossessedPawnChanged is a public engine delegate (AddDynamic linkable -- NOT
			// reflection). Re-push the persistent color on each possession so the skin survives respawn.
			OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

			// Cover the already-possessed case (BeginPlay after possession): push to the current pawn.
			if (APawn* ControlledPawn = GetPawn<APawn>())
			{
				PushToPawn(ControlledPawn);
			}
		}
	}
}

void UAFLSkinColorControllerComponent::SetPersistentSkinColor(UAFLSkinColorAsset* NewColor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PersistentSkinColor = NewColor;
		if (APawn* ControlledPawn = GetPawn<APawn>())
		{
			PushToPawn(ControlledPawn);
		}
	}
}

void UAFLSkinColorControllerComponent::OnPossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	// Authority-only (bound under HasAuthority() in BeginPlay). Re-push so the selection survives
	// respawn / re-possession.
	if (NewPawn)
	{
		PushToPawn(NewPawn);
	}
}

void UAFLSkinColorControllerComponent::PushToPawn(APawn* Pawn) const
{
	if (Pawn)
	{
		if (AFLSkinDiag::IsOn())
		{
			UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : PushToPawn persistent=%s"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
				PersistentSkinColor ? *PersistentSkinColor->GetName() : TEXT("null"));
		}

		if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
		{
			// Authority -> sets the replicated SkinColor -> all clients re-apply via OnRep (PATH 2) +
			// the new pawn's parts self-color on their BeginPlay (PATH 1).
			PawnComp->SetSkinColor(PersistentSkinColor);
		}
	}
}
