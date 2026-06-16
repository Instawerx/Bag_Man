// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorComponent.h"

#include "Components/ChildActorComponent.h"
#include "Cosmetics/AFLCharacterPartActor.h"
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Materials/MaterialInstanceConstant.h"   // the equipped facemask MIC (replication-safe content asset)
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSkinColorComponent)

// ---- Skin race diagnostics (cvar-gated, OFF by default) -----------------------------------------
DEFINE_LOG_CATEGORY(LogAFLSkinDiag);

static TAutoConsoleVariable<int32> CVarAFLSkinDiag(
	TEXT("afl.SkinDiag"),
	0,
	TEXT("1=log skin color apply/replication for race diagnosis (PATH 1/2 + controller). 0=silent (default)."),
	ECVF_Default);

namespace AFLSkinDiag
{
	bool IsOn()
	{
		return CVarAFLSkinDiag.GetValueOnGameThread() > 0;
	}

	FString Prefix(const UObject* WorldContext)
	{
		// Resolve the world's net mode to a SHORT tag so two-client logs are orderable by eye.
		// SRV = listen-host / authority world; CLI = remote client; STD = standalone; SVR-D = dedicated.
		const TCHAR* Role = TEXT("???");
		if (const UObject* Ctx = WorldContext)
		{
			if (const UWorld* World = Ctx->GetWorld())
			{
				switch (World->GetNetMode())
				{
				case NM_ListenServer:    Role = TEXT("SRV");   break;
				case NM_Client:          Role = TEXT("CLI");   break;
				case NM_Standalone:      Role = TEXT("STD");   break;
				case NM_DedicatedServer: Role = TEXT("SVR-D"); break;
				default:                 Role = TEXT("???");   break;
				}
			}
		}
		return FString::Printf(TEXT("[SkinDiag][%s][f=%llu] "), Role, (uint64)GFrameCounter);
	}
}
// -------------------------------------------------------------------------------------------------

UAFLSkinColorComponent::UAFLSkinColorComponent()
{
	// No replicated base to inherit from (standalone UActorComponent) -> WE must enable replication so
	// SkinColor replicates. Do NOT omit -- without it, SkinColor never reaches clients and convergence
	// is silently zero (the "compiles but doesn't replicate" trap).
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLSkinColorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLSkinColorComponent, SkinColor);
	DOREPLIFETIME(UAFLSkinColorComponent, Facemask);
}

void UAFLSkinColorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Reconcile: if a color + parts are BOTH already present when we begin play (late join, or color set
	// before our BeginPlay), apply now. Idempotent + null-guarded -> safe no-op otherwise.
	ReapplyColorToAllParts();

	// Same reconcile for the facemask (a part arriving after the replicated facemask value picks it up here).
	ReapplyFacemaskToAllParts();
}

void UAFLSkinColorComponent::SetSkinColor(UAFLSkinColorAsset* NewColor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SkinColor = NewColor;

		// Listen-host: the authority is also a client, but OnRep does NOT fire on the authority --
		// apply locally now so the host's own view updates immediately.
		ReapplyColorToAllParts();
	}
}

void UAFLSkinColorComponent::OnRep_SkinColor()
{
	// PATH 2 of 2 (covers COLOR-ARRIVES-SECOND): the color value replicated in. Push to already-spawned
	// parts that read null/stale at their BeginPlay. LOAD-BEARING -- without this, color-after-part desyncs.
	if (AFLSkinDiag::IsOn())
	{
		// On a LATE client this firing proves Race C: SkinColor arrived in the join-in-progress bunch.
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_SkinColor fired: color=%s"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			SkinColor ? *SkinColor->GetName() : TEXT("null"));
	}
	ReapplyColorToAllParts();
}

void UAFLSkinColorComponent::ReapplyColorToAllParts()
{
	if (SkinColor == nullptr)
	{
		return; // guard: nothing to apply yet
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Owner->GetComponents<UChildActorComponent>(ChildActorComps);

	const bool bDiag = AFLSkinDiag::IsOn();
	int32 NumPartsFound = 0;
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		// EDIT 1 FILTER (by-construction): only OUR body parts. Non-body child-actors (e.g. a weapon)
		// are not AAFLCharacterPartActor -> Cast returns null -> skipped. Skin never bleeds onto them.
		if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
		{
			++NumPartsFound;
			if (bDiag)
			{
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s/%s : Reapply -> part"),
					*AFLSkinDiag::Prefix(this), *Owner->GetName(), *Part->GetName());
			}
			Part->ApplySkinColor(SkinColor); // idempotent (part's owned-MID create-once)
		}
	}

	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : Reapply: found %d parts (color=%s)"),
			*AFLSkinDiag::Prefix(this), *Owner->GetName(), NumPartsFound,
			SkinColor ? *SkinColor->GetName() : TEXT("null"));
	}
}

// ---- FACEMASK (replicated PARALLEL to SkinColor; same two-path race-safe spine) ------------------

void UAFLSkinColorComponent::SetFacemask(UMaterialInstanceConstant* NewMaterial)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Facemask = NewMaterial;
		// Listen-host: OnRep does NOT fire on authority -> apply locally now (mirrors SetSkinColor).
		ReapplyFacemaskToAllParts();
	}
}

void UAFLSkinColorComponent::OnRep_Facemask()
{
	// PATH 2 (facemask-arrives-second): the facemask value replicated in -> swap slot-1 on already-spawned parts.
	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : OnRep_Facemask fired: mask=%s"),
			*AFLSkinDiag::Prefix(this),
			GetOwner() ? *GetOwner()->GetName() : TEXT("<no-owner>"),
			Facemask ? *Facemask->GetName() : TEXT("null"));
	}
	ReapplyFacemaskToAllParts();
}

void UAFLSkinColorComponent::ReapplyFacemaskToAllParts()
{
	// NOTE: NO early-return on null Facemask -- unlike SkinColor, a NULL facemask is a MEANINGFUL state (un-equip
	// -> restore the part's authored slot-1). ApplyFacemask(nullptr, ...) does the restore.
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Owner->GetComponents<UChildActorComponent>(ChildActorComps);

	const bool bDiag = AFLSkinDiag::IsOn();
	int32 NumPartsFound = 0;
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		// Same by-construction filter as ReapplyColorToAllParts: only OUR body parts (weapons cast to null).
		if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
		{
			++NumPartsFound;
			// Pass the CURRENT SkinColor so the part re-layers the finish params on top of the swapped material
			// (composition order: material swap, then param re-push). SkinColor may be null (finish not set yet);
			// ApplyFacemask handles that (the mask MIC shows raw until the color lands + its OnRep re-applies).
			Part->ApplyFacemask(Facemask, SkinColor);
		}
	}

	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ReapplyFacemask: found %d parts (mask=%s color=%s)"),
			*AFLSkinDiag::Prefix(this), *Owner->GetName(), NumPartsFound,
			Facemask ? *Facemask->GetName() : TEXT("null"),
			SkinColor ? *SkinColor->GetName() : TEXT("null"));
	}
}
