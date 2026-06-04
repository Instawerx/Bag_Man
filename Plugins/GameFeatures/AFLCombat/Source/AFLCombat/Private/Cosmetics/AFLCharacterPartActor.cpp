// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLCharacterPartActor.h"

#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacterPartActor)

void AAFLCharacterPartActor::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	// Byte-identical to ALyraTaggedActor: feeds the reparented robots' Cosmetic.* tags to Lyra's AnimBP
	// (animation-style + body-proportion selection) via IGameplayTagAssetInterface.
	TagContainer.AppendTags(StaticGameplayTags);
}

void AAFLCharacterPartActor::BeginPlay()
{
	Super::BeginPlay();

	// PATH 1 of 2 (covers PART-ARRIVES-SECOND): resolve the owning pawn's color component and self-apply
	// the CURRENT color. The part is a child-actor attached to the pawn (spawned by the pawn's CharacterParts
	// component), so the pawn + its UAFLSkinColorComponent are guaranteed present before this BeginPlay.
	// If SkinColor's VALUE hasn't replicated yet (color-arrives-second), GetSkinColor() is null -> we no-op
	// here, and the component's OnRep-push (PATH 2) applies when the color arrives. BOTH paths are required.
	AActor* PawnActor = GetParentActor();
	const UAFLSkinColorComponent* ColorComp = PawnActor ? PawnActor->FindComponentByClass<UAFLSkinColorComponent>() : nullptr;
	const UAFLSkinColorAsset* ResolvedColor = ColorComp ? ColorComp->GetSkinColor() : nullptr;

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s/%s : BeginPlay resolve: pawn=%s colorComp=%s color=%s"),
			*AFLSkinDiag::Prefix(this),
			PawnActor ? *PawnActor->GetName() : TEXT("<none>"), *GetName(),
			PawnActor ? TEXT("y") : TEXT("n"),
			ColorComp ? TEXT("y") : TEXT("n"),
			ResolvedColor ? *ResolvedColor->GetName() : TEXT("null"));
	}

	if (ColorComp)
	{
		ApplySkinColor(ResolvedColor); // null -> ApplySkinColor early-returns (guard)
	}
}

void AAFLCharacterPartActor::ApplySkinColor(const UAFLSkinColorAsset* ColorAsset)
{
	const bool bDiag = AFLSkinDiag::IsOn();
	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ApplySkinColor(%s)"),
			*AFLSkinDiag::Prefix(this), *GetName(),
			ColorAsset ? *ColorAsset->GetName() : TEXT("null"));
	}

	if (ColorAsset == nullptr)
	{
		return; // GUARD: no color -> never touch materials -> never create a MID
	}

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
		{
			continue;
		}

		FAFLSkinMIDSlots& Slots = OwnedMIDs.FindOrAdd(Mesh); // our cache for this mesh
		const int32 NumMaterials = Mesh->GetNumMaterials();
		for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
		{
			// FIX 1 (a) -- OWN-YOUR-MID. Use the MID WE created+cached for this slot. We do NOT reuse a
			// foreign MID found in the slot (e.g. one the hit-flash / HitPosition0 path created) -- writing
			// skin params onto someone else's MID, or theirs stomping ours, is the collision we avoid.
			UMaterialInstanceDynamic* MID = Slots.SlotMIDs.FindRef(SlotIndex);

			// RACE B CRITICAL: log the material on the slot BEFORE we touch it. If this is ever the engine
			// default (not the baked MI_<team>_Body / MI_*_Limbs), that is a true unstyled frame. Normally it
			// is the base team MI (baked into the BP SCS, present pre-BeginPlay) -> never default grey.
			if (bDiag)
			{
				const UMaterialInterface* Pre = Mesh->GetMaterial(SlotIndex);
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : slot[%d] preexisting mat=%s"),
					*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex,
					Pre ? *Pre->GetName() : TEXT("null"));
			}

			// (Re)create if: we never made one for this slot, OR the slot no longer holds OUR MID (a foreign
			// system replaced it -> we re-establish ours). Otherwise reuse -> create-once, no duplicate, no leak.
			if (!IsValid(MID) || Mesh->GetMaterial(SlotIndex) != MID)
			{
				MID = Mesh->CreateAndSetMaterialInstanceDynamic(SlotIndex); // ENGINE_API
				Slots.SlotMIDs.Add(SlotIndex, MID);                         // cache OURS
				if (bDiag)
				{
					UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : slot[%d] MID CREATED"),
						*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex);
				}
			}
			else if (bDiag)
			{
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : slot[%d] MID reused"),
					*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex);
			}
			if (!MID)
			{
				continue;
			}

			for (const TPair<FName, float>& KV : ColorAsset->GetScalars())
			{
				MID->SetScalarParameterValue(KV.Key, KV.Value);
			}
			for (const TPair<FName, FLinearColor>& KV : ColorAsset->GetColors())
			{
				MID->SetVectorParameterValue(KV.Key, FVector(KV.Value));
			}
			for (const TPair<FName, TObjectPtr<UTexture>>& KV : ColorAsset->GetTextures())
			{
				MID->SetTextureParameterValue(KV.Key, KV.Value);
			}
		}
	}
}
