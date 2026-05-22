// Copyright C12 AI Gaming. All Rights Reserved.

#include "LagComp/AFLPawnHitboxHistoryComponent.h"

#include "AFLCombat.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LagComp/AFLLagCompensationWorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPawnHitboxHistoryComponent)


UAFLPawnHitboxHistoryComponent::UAFLPawnHitboxHistoryComponent()
{
	PrimaryComponentTick.bCanEverTick          = true;
	PrimaryComponentTick.TickGroup             = TG_PostPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Enabled in BeginPlay if server.

	// Default 8-bone Lyra hit silhouette. BP children may override. Names match
	// SKM_Manny / SKM_Quinn (Lyra's stock skeletons); missing bones are silently
	// skipped at sample time so swapping skeletons narrows the hit profile but
	// does not crash.
	TrackedBones = {
		TEXT("head"),
		TEXT("neck_01"),
		TEXT("spine_03"),
		TEXT("pelvis"),
		TEXT("hand_l"),
		TEXT("hand_r"),
		TEXT("foot_l"),
		TEXT("foot_r"),
	};
}

void UAFLPawnHitboxHistoryComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	bIsServer = Owner && Owner->HasAuthority();

	if (!bIsServer)
	{
		// Clients never sample; lag-comp rewind only runs server-side.
		return;
	}

	// Size the ring from configured rate and window. Cap at 256 so a misconfig
	// (e.g., 120Hz * 3s) does not silently allocate kilobytes per pawn.
	const int32 Slots = FMath::Clamp(
		FMath::CeilToInt(HistorySeconds * static_cast<float>(SampleHz)),
		1,
		256);
	Ring.Reset(Slots);
	Ring.SetNum(Slots);
	WriteHead = 0;
	RingCount = 0;
	TimeSinceLastSample = 0.0f;

	if (UWorld* World = GetWorld())
	{
		if (UAFLLagCompensationWorldSubsystem* Sys = World->GetSubsystem<UAFLLagCompensationWorldSubsystem>())
		{
			Sys->RegisterComponent(this);
			CachedSubsystem = Sys;
		}
	}

	PrimaryComponentTick.SetTickFunctionEnable(true);
}

void UAFLPawnHitboxHistoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAFLLagCompensationWorldSubsystem* Sys = CachedSubsystem.Get())
	{
		Sys->UnregisterComponent(this);
	}
	CachedSubsystem.Reset();
	CachedMesh.Reset();
	Ring.Reset();
	WriteHead = 0;
	RingCount = 0;

	Super::EndPlay(EndPlayReason);
}

void UAFLPawnHitboxHistoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsServer || Ring.Num() == 0)
	{
		return;
	}

	const float SampleInterval = 1.0f / FMath::Max(1, SampleHz);
	TimeSinceLastSample += DeltaTime;
	if (TimeSinceLastSample < SampleInterval)
	{
		return;
	}

	// Drop fractional remainder rather than carrying it. Under a frame stall
	// we want the next snapshot timestamped at the new "now," not a burst of
	// catch-up samples — catch-up would put two snapshots at near-identical
	// ServerTime and break the bracketing lerp.
	TimeSinceLastSample = 0.0f;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	PushSnapshot(World->GetTimeSeconds());
}

USkeletalMeshComponent* UAFLPawnHitboxHistoryComponent::ResolveMesh() const
{
	if (USkeletalMeshComponent* Cached = CachedMesh.Get())
	{
		return Cached;
	}
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	// Component-by-class lookup keeps the component working on any APawn
	// variant — we deliberately do not cast to ACharacter.
	USkeletalMeshComponent* Found = Owner->FindComponentByClass<USkeletalMeshComponent>();
	CachedMesh = Found;
	return Found;
}

void UAFLPawnHitboxHistoryComponent::PushSnapshot(double ServerTime)
{
	USkeletalMeshComponent* Mesh = ResolveMesh();
	if (!Mesh)
	{
		return;
	}

	FAFLHitboxFrameSnapshot& Slot = Ring[WriteHead];
	Slot.ServerTime = ServerTime;
	Slot.Samples.Reset(TrackedBones.Num());

	if (TrackedBones.Num() == 0)
	{
		// Root-bone-only fallback. Cheap and avoids the empty-snapshot case
		// that would make SampleAtTime return nothing useful.
		FAFLHitboxBoneSample Sample;
		Sample.Bone       = Mesh->GetBoneName(0);
		Sample.WorldXForm = Mesh->GetComponentTransform();
		Slot.Samples.Add(Sample);
	}
	else
	{
		for (const FName& BoneName : TrackedBones)
		{
			if (BoneName.IsNone())
			{
				continue;
			}
			const int32 BoneIdx = Mesh->GetBoneIndex(BoneName);
			if (BoneIdx == INDEX_NONE)
			{
				continue;
			}
			FAFLHitboxBoneSample Sample;
			Sample.Bone       = BoneName;
			Sample.WorldXForm = Mesh->GetBoneTransform(BoneIdx);
			Slot.Samples.Add(Sample);
		}
	}

	WriteHead = (WriteHead + 1) % Ring.Num();
	RingCount = FMath::Min(RingCount + 1, Ring.Num());
}

bool UAFLPawnHitboxHistoryComponent::FindBracketingSnapshots(double ServerTime, int32& OutNewerIdx, int32& OutOlderIdx, float& OutAlpha) const
{
	if (RingCount == 0 || Ring.Num() == 0)
	{
		return false;
	}

	const int32 N = Ring.Num();
	auto IndexAtAge = [N, this](int32 Age) -> int32
	{
		// Age 0 = newest. WriteHead points to the next slot to write, so the
		// slot just written sits at (WriteHead - 1 - Age) mod N.
		return ((WriteHead - 1 - Age) % N + N) % N;
	};

	const int32 NewestIdx = IndexAtAge(0);
	if (ServerTime >= Ring[NewestIdx].ServerTime)
	{
		// Future or this-tick: clamp to newest with no interpolation.
		OutNewerIdx = NewestIdx;
		OutOlderIdx = NewestIdx;
		OutAlpha    = 1.0f;
		return true;
	}

	const int32 OldestIdx = IndexAtAge(RingCount - 1);
	if (ServerTime <= Ring[OldestIdx].ServerTime)
	{
		// Older than our window: clamp to oldest. The caller has already
		// clamped DeltaSeconds to MaxRewindSeconds (master doc 7.4), so a hit
		// here means the shooter's ping exceeded the rewind cap — clamping
		// degrades gracefully into "no extra compensation."
		OutNewerIdx = OldestIdx;
		OutOlderIdx = OldestIdx;
		OutAlpha    = 0.0f;
		return true;
	}

	// Linear scan newest-to-oldest. RingCount ≤ 256 so the constant cost is
	// trivial; a binary search would marginally help at 72 entries but is
	// more code and obscures the ring-walk pattern.
	for (int32 Age = 1; Age < RingCount; ++Age)
	{
		const int32 NewerIdx = IndexAtAge(Age - 1);
		const int32 OlderIdx = IndexAtAge(Age);
		const double NewerT = Ring[NewerIdx].ServerTime;
		const double OlderT = Ring[OlderIdx].ServerTime;
		if (ServerTime <= NewerT && ServerTime >= OlderT)
		{
			OutNewerIdx = NewerIdx;
			OutOlderIdx = OlderIdx;
			const double Span = NewerT - OlderT;
			OutAlpha = (Span > KINDA_SMALL_NUMBER)
				? static_cast<float>((ServerTime - OlderT) / Span)
				: 1.0f;
			return true;
		}
	}

	// Logically unreachable; bail safely.
	OutNewerIdx = NewestIdx;
	OutOlderIdx = NewestIdx;
	OutAlpha    = 1.0f;
	return true;
}

bool UAFLPawnHitboxHistoryComponent::SampleAtTime(double ServerTime, TArray<FAFLHitboxBoneSample>& OutSamples) const
{
	OutSamples.Reset();

	int32 NewerIdx = 0;
	int32 OlderIdx = 0;
	float Alpha    = 0.0f;
	if (!FindBracketingSnapshots(ServerTime, NewerIdx, OlderIdx, Alpha))
	{
		return false;
	}

	const FAFLHitboxFrameSnapshot& Newer = Ring[NewerIdx];
	const FAFLHitboxFrameSnapshot& Older = Ring[OlderIdx];

	// Pair by bone name. Bone lists may differ between snapshots in the rare
	// case the mesh swapped at runtime; ignore unmatched entries.
	OutSamples.Reserve(Newer.Samples.Num());
	for (const FAFLHitboxBoneSample& NewerSample : Newer.Samples)
	{
		const FAFLHitboxBoneSample* OlderSample = Older.Samples.FindByPredicate(
			[&NewerSample](const FAFLHitboxBoneSample& S) { return S.Bone == NewerSample.Bone; });

		FAFLHitboxBoneSample Out;
		Out.Bone = NewerSample.Bone;

		if (OlderSample && NewerIdx != OlderIdx)
		{
			const FVector  Loc = FMath::Lerp(OlderSample->WorldXForm.GetLocation(), NewerSample.WorldXForm.GetLocation(), Alpha);
			const FQuat    Rot = FQuat::Slerp(OlderSample->WorldXForm.GetRotation(), NewerSample.WorldXForm.GetRotation(), Alpha).GetNormalized();
			const FVector  Scl = FMath::Lerp(OlderSample->WorldXForm.GetScale3D(), NewerSample.WorldXForm.GetScale3D(), Alpha);
			Out.WorldXForm = FTransform(Rot, Loc, Scl);
		}
		else
		{
			Out.WorldXForm = NewerSample.WorldXForm;
		}
		OutSamples.Add(Out);
	}
	return OutSamples.Num() > 0;
}
