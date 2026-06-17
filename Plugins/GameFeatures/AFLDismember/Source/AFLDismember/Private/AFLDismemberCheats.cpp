// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismember.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Effects/GE_AFL_Damage_Pulse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Pawn.h"               // APawn::GetActorLocation in the self-decap cheat
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"
#include "Targeting/AFLDamageTargetSkeletal.h"

#if UE_WITH_CHEAT_MANAGER

namespace
{
	// Dismember-consequence tags (declared in AFLCombatTags.ini). File-local native defines
	// for the TestLegTag/TestLimbTag cheats -- must be at namespace scope (the macro emits a
	// static FNativeGameplayTag, illegal inside a function body).
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Dismember_LeftLeg_Cheat, "State.Dismembered.LeftLeg");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Dismember_RightLeg_Cheat, "State.Dismembered.RightLeg");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Dismember_LeftArm_Cheat, "State.Dismembered.LeftArm");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Dismember_RightArm_Cheat, "State.Dismembered.RightArm");

	// Find the first AAFLDamageTargetSkeletal in any game world. Pattern
	// mirrors FindDummyHistory in AFLCombatCheats.cpp -- the slice expects
	// exactly one placed skeletal dummy, so the first hit wins.
	AAFLDamageTargetSkeletal* FindSkeletalDummy()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& WC : GEngine->GetWorldContexts())
		{
			UWorld* World = WC.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			for (TActorIterator<AAFLDamageTargetSkeletal> It(World); It; ++It)
			{
				return *It;
			}
		}
		return nullptr;
	}

	// S4-INC1: lethal zone-targeted hit to the first skeletal dummy, carrying an
	// arbitrary BoneName so the data-driven UAFLDismemberComponent resolves it to a
	// zone row (head -> Head row, thigh_l -> LeftLeg row, etc.). Shared by both the
	// head cheat (TestKillHead, bone fixed to "head") and the generic TestSever.
	void SeverBoneOnDummy(FName Bone, float Damage)
	{
		AAFLDamageTargetSkeletal* Target = FindSkeletalDummy();
		if (!Target)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember: no AAFLDamageTargetSkeletal in world -- place B_AFL_DamageTarget_Skel first"));
			return;
		}

		UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
		if (!TargetASC)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember: target has no ASC"));
			return;
		}

		// Seed Source.Damage on the (self) source ASC -- the exec calc captures this with
		// bSnapshot=true. source == target == dummy is fine: the exec calc only reads the
		// source's Damage attribute; the listener filters on Target == GetOwner() (matches
		// when target == dummy).
		TargetASC->ApplyModToAttribute(
			UAFLAttributeSet_Combat::GetDamageAttribute(),
			EGameplayModOp::Override, Damage);

		// Context + HitResult carrying the requested BoneName -- the exec calc re-fetches
		// HitResult from the context and forwards BoneName into FAFLOverkillMessage
		// (AFLDamageExecCalc.cpp L257-267) -> UAFLDismemberComponent resolves it via the ZoneSet.
		FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
		Context.AddInstigator(Target, Target);

		FHitResult Hit;
		Hit.BoneName = Bone;
		Hit.ImpactPoint = Target->GetActorLocation();
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
			UGE_AFL_Damage_Pulse::StaticClass(), /*Level=*/1.0f, Context);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember: failed to make spec"));
			return;
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

		UE_LOG(LogAFLDismember, Display,
			TEXT("AFL.Dismember: applied Damage=%.1f to %s (bone=%s) -- expect Event.Damage.Overkill.AFL + dismember log"),
			Damage, *GetNameSafe(Target), *Bone.ToString());
	}

	void HandleAFLDismemberTestKillHead(const TArray<FString>& Args)
	{
		const float Damage = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 200.0f;
		SeverBoneOnDummy(FName(TEXT("head")), Damage);
	}

	// S4-INC3 PHASE B-2: sever a bone on the LOCAL PLAYER PAWN (not the dummy) so the FULL decap
	// loop is watchable on the controllable hero -- head pops (HeadHealth drains), Health bar
	// UNTOUCHED, the decap camera pushes (State.Decapitated -> GA_AFL_DecapitatedCamera), and the
	// head loot-box spawns for self-retrieve. The damage GE goes to the pawn's ASC, which resolves
	// to the PlayerState ASC where the zone-HP attributes + the dismember listener live.
	// Default 60 dmg fully depletes the 54-HP head zone in one shot (decap immediately).
	void SeverBoneOnLocalPawn(UWorld* World, FName Bone, float Damage)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UAbilitySystemComponent* ASC =
			Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
		if (!ASC)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember.TestDecapSelf: no local pawn ASC"));
			return;
		}

		// Seed Source.Damage (snapshot-captured by the exec calc), then apply a Pulse-damage spec
		// to SELF carrying BoneName=head in the hit result -- exactly the dummy path, retargeted.
		ASC->ApplyModToAttribute(
			UAFLAttributeSet_Combat::GetDamageAttribute(), EGameplayModOp::Override, Damage);

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Pawn, Pawn);
		FHitResult Hit;
		Hit.BoneName = Bone;
		Hit.ImpactPoint = Pawn->GetActorLocation();
		Context.AddHitResult(Hit);

		FGameplayEffectSpecHandle SpecHandle =
			ASC->MakeOutgoingSpec(UGE_AFL_Damage_Pulse::StaticClass(), /*Level=*/1.0f, Context);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember.TestDecapSelf: failed to make spec"));
			return;
		}
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

		UE_LOG(LogAFLDismember, Display,
			TEXT("AFL.Dismember.TestDecapSelf: applied Damage=%.1f to LOCAL PAWN %s (bone=%s) -- expect AFL_SEVER head + State.Decapitated + loot-box, Health UNCHANGED"),
			Damage, *GetNameSafe(Pawn), *Bone.ToString());
	}

	// AFL.Dismember.TestDecapSelf [damage=60] -- decapitate the local player (watch the full loop).
	void HandleAFLDismemberTestDecapSelf(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		const float Damage = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 60.0f;
		SeverBoneOnLocalPawn(World, FName(TEXT("head")), Damage);
	}

	// AFL.Dismember.TestSeverSelf <bone> [damage=200] -- sever ANY bone on the LOCAL PLAYER pawn (the generic
	// limb sibling of TestDecapSelf). This makes the COMBAT-LOOT OWNER-retrieve branch SOLO-watchable: the
	// severed part is owned by YOU, so grabbing it reattaches it (RestoreZone) with NO Watts. (Contrast
	// AFL.Dismember.TestSever, which severs on the DUMMY -> the part is owned by the dummy, so a player
	// grabbing it is an ENEMY collect = +LootWatts. Use TestSever for the grant branch, TestSeverSelf for the
	// reattach branch.)
	void HandleAFLDismemberTestSeverSelf(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestSeverSelf: usage: AFL.Dismember.TestSeverSelf <BoneName> [damage=25] (e.g. upperarm_l, thigh_l)"));
			return;
		}
		const FName Bone(*Args[0]);
		// NON-LETHAL default: limb zones are ~36 HP (arm) / ~18 HP (leg) and the limb absorber OVERFLOWS the
		// remainder to health, so the dummy's 200 default would deplete the zone (sever) + dump ~164 into the
		// LOCAL player's health = SELF-KILL (watched: zoneHP=36 overflow=164 -> Overkill -> StartDeath -> respawn
		// -> stale-owner arms -> drag). 50 reliably severs the highest limb zone (36) with <=32 overflow
		// (non-lethal on ~100 health), keeping the player ALIVE so the SURVIVABLE owner self-retrieve (walk to
		// your own limb -> grab -> RestoreZone reattach) works as designed. (The head path absorbs the whole hit
		// -> no overflow -> TestDecapSelf keeps its 60 default.)
		const float Damage = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 50.0f;
		SeverBoneOnLocalPawn(World, Bone, Damage);
	}

	// S4-INC1: generic zone sever -- bone arg picks the zone via the data table.
	// Usage: AFL.Dismember.TestSever thigh_l [damage]   (thigh_l/thigh_r/calf_*/head/...)
	void HandleAFLDismemberTestSever(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestSever: usage: AFL.Dismember.TestSever <BoneName> [damage=200] (e.g. thigh_l)"));
			return;
		}
		const FName Bone(*Args[0]);
		const float Damage = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 200.0f;
		SeverBoneOnDummy(Bone, Damage);
	}

	// S4-INC1: TOGGLE State.Dismembered.Left/RightLeg on the LOCAL PLAYER's pawn (the moving
	// hero) so the leg-speed consequence (UAFLDismemberLegPenaltyComponent) is directly
	// watchable WITHOUT severing the hero (the sever path runs on the dummy; this proves the
	// tag->penalty half on the controllable pawn -- the same isolation the dash listener used).
	// Usage: AFL.Dismember.TestLegTag [l|r]   (default l). Add once -> speed halves; again -> restores.
	void HandleAFLDismemberTestLegTag(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		const bool bRight = Args.Num() > 0 && (Args[0].Equals(TEXT("r"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("right"), ESearchCase::IgnoreCase));
		const FGameplayTag LegTag = bRight ? TAG_Dismember_RightLeg_Cheat : TAG_Dismember_LeftLeg_Cheat;

		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UAbilitySystemComponent* ASC = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
		if (!ASC)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember.TestLegTag: no local pawn ASC"));
			return;
		}

		// Toggle: the leg-penalty component listens NewOrRemoved, so add->apply, remove->restore.
		if (ASC->HasMatchingGameplayTag(LegTag))
		{
			ASC->RemoveLooseGameplayTag(LegTag);
			UE_LOG(LogAFLDismember, Display, TEXT("AFL.Dismember.TestLegTag: REMOVED %s from %s -> expect MaxWalkSpeed restore"),
				*LegTag.ToString(), *GetNameSafe(Pawn));
		}
		else
		{
			ASC->AddLooseGameplayTag(LegTag);
			UE_LOG(LogAFLDismember, Display, TEXT("AFL.Dismember.TestLegTag: ADDED %s to %s -> expect MaxWalkSpeed x0.5"),
				*LegTag.ToString(), *GetNameSafe(Pawn));
		}
	}

	// S4-INC2: generalized -- TOGGLE the actual consequence GE for any limb on the LOCAL pawn,
	// so any consequence is watchable on the moving/firing hero. CRITICAL: this APPLIES THE GE
	// (not a loose tag), because the arm consequence is an ATTRIBUTE MODIFIER (RecoilMultiplier
	// x1.5) that lives on the GE -- a bare loose-tag add grants the tag but NEVER moves the
	// attribute, so the recoil would not change. Applying the GE grants the tag AND the modifier
	// uniformly (the same path SeverZone uses in real play). Legs read the tag, arms read the
	// attribute; the GE carries both, so this one path proves every limb correctly.
	// Usage: AFL.Dismember.TestLimbTag <leftleg|rightleg|leftarm|rightarm>
	void HandleAFLDismemberTestLimbTag(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestLimbTag: usage: TestLimbTag <leftleg|rightleg|leftarm|rightarm>"));
			return;
		}
		const FString Zone = Args[0].ToLower();
		FGameplayTag Tag;
		const TCHAR* GEPath = nullptr;
		if      (Zone == TEXT("leftleg")  || Zone == TEXT("ll")) { Tag = TAG_Dismember_LeftLeg_Cheat;  GEPath = TEXT("/AFLDismember/Effects/GE_AFL_Dismember_LeftLeg.GE_AFL_Dismember_LeftLeg_C"); }
		else if (Zone == TEXT("rightleg") || Zone == TEXT("rl")) { Tag = TAG_Dismember_RightLeg_Cheat; GEPath = TEXT("/AFLDismember/Effects/GE_AFL_Dismember_RightLeg.GE_AFL_Dismember_RightLeg_C"); }
		else if (Zone == TEXT("leftarm")  || Zone == TEXT("la")) { Tag = TAG_Dismember_LeftArm_Cheat;  GEPath = TEXT("/AFLDismember/Effects/GE_AFL_Dismember_LeftArm.GE_AFL_Dismember_LeftArm_C"); }
		else if (Zone == TEXT("rightarm") || Zone == TEXT("ra")) { Tag = TAG_Dismember_RightArm_Cheat; GEPath = TEXT("/AFLDismember/Effects/GE_AFL_Dismember_RightArm.GE_AFL_Dismember_RightArm_C"); }
		else
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("AFL.Dismember.TestLimbTag: unknown zone '%s' (use leftleg|rightleg|leftarm|rightarm)"), *Zone);
			return;
		}

		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UAbilitySystemComponent* ASC = Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
		if (!ASC)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember.TestLimbTag: no local pawn ASC"));
			return;
		}

		UClass* GEClass = StaticLoadClass(UGameplayEffect::StaticClass(), nullptr, GEPath);
		if (!GEClass)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("AFL.Dismember.TestLimbTag: failed to load GE %s"), GEPath);
			return;
		}

		// Toggle by the granted tag: present -> remove the GE (restores attr/tag), else apply it.
		if (ASC->HasMatchingGameplayTag(Tag))
		{
			const FGameplayEffectQuery Query =
				FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag));
			const int32 Removed = ASC->RemoveActiveEffects(Query);
			UE_LOG(LogAFLDismember, Display, TEXT("AFL.Dismember.TestLimbTag: REMOVED %s GE (%d) from %s -> consequence cleared"),
				*Zone, Removed, *GetNameSafe(Pawn));
		}
		else
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddInstigator(Pawn, Pawn);
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GEClass, 1.0f, Context);
			if (Spec.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				UE_LOG(LogAFLDismember, Display, TEXT("AFL.Dismember.TestLimbTag: APPLIED %s GE to %s (grants %s + any attr modifier)"),
					*Zone, *GetNameSafe(Pawn), *Tag.ToString());
			}
		}
	}

	FAutoConsoleCommand GAFLDismemberTestKillHeadCmd(
		TEXT("AFL.Dismember.TestKillHead"),
		TEXT("S4-04b: lethal head-targeted hit to the first AAFLDamageTargetSkeletal, triggering Event.Damage.Overkill.AFL with BoneName=head. Usage: AFL.Dismember.TestKillHead [damage=200]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLDismemberTestKillHead));

	FAutoConsoleCommand GAFLDismemberTestSeverCmd(
		TEXT("AFL.Dismember.TestSever"),
		TEXT("S4-INC1: lethal zone-targeted hit to the first AAFLDamageTargetSkeletal; the bone arg picks the zone via DA_AFL_DismemberZones. Usage: AFL.Dismember.TestSever <BoneName> [damage=200] (e.g. thigh_l, thigh_r)"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleAFLDismemberTestSever));

	// S4-INC3 PHASE B-2: decapitate the LOCAL PLAYER (the controllable hero) to watch the full
	// decap loop -- head pop + Health-bar-untouched + decap camera + retrievable head loot-box.
	FAutoConsoleCommand GAFLDismemberTestDecapSelfCmd(
		TEXT("AFL.Dismember.TestDecapSelf"),
		TEXT("S4-INC3 B-2: decapitate the LOCAL PLAYER pawn -> head pops (HeadHealth drains), Health bar UNCHANGED, decap camera pushes, head loot-box spawns for self-retrieve. Usage: AFL.Dismember.TestDecapSelf [damage=60]"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLDismemberTestDecapSelf));

	FAutoConsoleCommand GAFLDismemberTestSeverSelfCmd(
		TEXT("AFL.Dismember.TestSeverSelf"),
		TEXT("COMBAT-LOOT owner-branch watch: sever ANY bone on the LOCAL PLAYER pawn (generic sibling of TestDecapSelf). The severed part is owned by YOU -> grab it to reattach (RestoreZone, NO Watts). Usage: AFL.Dismember.TestSeverSelf <BoneName> [damage=200] (e.g. upperarm_l, thigh_l)"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLDismemberTestSeverSelf));

	// World-args variant: TestLegTag toggles the leg-dismember tag on the LOCAL pawn so the
	// leg-speed penalty is watchable on the moving hero (the tag->penalty half, isolated).
	FAutoConsoleCommand GAFLDismemberTestLegTagCmd(
		TEXT("AFL.Dismember.TestLegTag"),
		TEXT("S4-INC1: TOGGLE State.Dismembered.Left/RightLeg on the local player pawn -> UAFLDismemberLegPenaltyComponent halves/restores MaxWalkSpeed. Usage: AFL.Dismember.TestLegTag [l|r]"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLDismemberTestLegTag));

	// S4-INC2: any-limb consequence toggle on the local pawn (legs -> speed, arms -> recoil/spread).
	FAutoConsoleCommand GAFLDismemberTestLimbTagCmd(
		TEXT("AFL.Dismember.TestLimbTag"),
		TEXT("S4-INC2: TOGGLE State.Dismembered.<zone> on the local pawn. Arms drive RecoilMultiplier x1.5 (Pulse cone+kick widen). Usage: AFL.Dismember.TestLimbTag <leftleg|rightleg|leftarm|rightarm>"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLDismemberTestLimbTag));
}

#endif // UE_WITH_CHEAT_MANAGER
