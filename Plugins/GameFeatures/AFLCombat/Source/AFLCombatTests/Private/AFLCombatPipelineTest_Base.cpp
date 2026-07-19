// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCombatPipelineTest_Base.h"

#include "AFLCombatTests.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: Health now lives on the Lyra set -> assert against it
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Messages/LyraVerbMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCombatPipelineTest_Base)


// File-specific suffix on the C++ symbol (the FName *value* stays as the
// canonical tag string). Required because UBT Unity builds merge multiple
// .cpp files into one translation unit, and anonymous namespaces collapse
// into a single TU-level namespace under that merge.
namespace
{
	// SetByCaller magnitude tags (consumed by UAFLDamageExecCalc).
	const FName NAME_Data_Damage_Headshot_PipelineBase  = TEXT("Data.Damage.Headshot");
	const FName NAME_Data_Damage_Weakpoint_PipelineBase = TEXT("Data.Damage.Weakpoint");
	const FName NAME_Data_Damage_Distance_PipelineBase  = TEXT("Data.Damage.Distance");

	// Verb tag broadcast by the ExecCalc on overkill.
	const FName NAME_Event_Damage_Overkill_PipelineBase = TEXT("Event.Damage.Overkill");
}

// One tick of settle time (seconds) before assertions run. Damage GE is Instant
// so Health/Shield are written synchronously; the listener delivery is queued
// to the message subsystem and processed on the next frame.
static constexpr float kSettleDuration = 0.05f;


AAFLCombatPipelineTest_Base::AAFLCombatPipelineTest_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

// ----------------------------------------------------------------------------
// AFunctionalTest overrides
// ----------------------------------------------------------------------------

void AAFLCombatPipelineTest_Base::PrepareTest()
{
	Super::PrepareTest();

	CachedASC.Reset();
	OverkillListenerHandle = FGameplayMessageListenerHandle();
	ObservedOverkillCount = 0;
	WaitedForAttributeSet = 0.0f;
	bDamageFired = false;
	SettleTime = 0.0f;

	UE_LOG(LogAFLCombatTests, Display, TEXT("[%s] PrepareTest"), *RowLabel);
}

bool AAFLCombatPipelineTest_Base::IsReady_Implementation()
{
	UAbilitySystemComponent* ASC = ResolvePlayerASC();
	if (ASC == nullptr)
	{
		WaitedForAttributeSet += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		if (WaitedForAttributeSet > AttributeGrantTimeout)
		{
			UE_LOG(LogAFLCombatTests, Warning,
				TEXT("[%s] IsReady timeout waiting for player ASC after %.2fs"),
				*RowLabel, WaitedForAttributeSet);
		}
		return false;
	}

	if (!ASC->HasAttributeSetForAttribute(UAFLAttributeSet_Combat::GetHealthAttribute()))
	{
		WaitedForAttributeSet += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		if (WaitedForAttributeSet > AttributeGrantTimeout)
		{
			UE_LOG(LogAFLCombatTests, Warning,
				TEXT("[%s] IsReady timeout: AFLAttributeSet_Combat not granted within %.2fs"),
				*RowLabel, AttributeGrantTimeout);
		}
		return false;
	}

	CachedASC = ASC;
	return true;
}

void AAFLCombatPipelineTest_Base::StartTest()
{
	Super::StartTest();

	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (ASC == nullptr)
	{
		FinishTest(EFunctionalTestResult::Failed, FString::Printf(TEXT("[%s] StartTest: ASC lost between IsReady and StartTest"), *RowLabel));
		return;
	}

	UE_LOG(LogAFLCombatTests, Display, TEXT("[%s] StartTest"), *RowLabel);

	ApplyPreconditions(ASC);
	RegisterOverkillListener();
	FireDamage(ASC);

	bDamageFired = true;
	SettleTime = 0.0f;
}

void AAFLCombatPipelineTest_Base::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bDamageFired)
	{
		return;
	}

	SettleTime += DeltaSeconds;
	if (SettleTime < kSettleDuration)
	{
		return;
	}

	// One-shot: prevent re-entry after FinishTest.
	bDamageFired = false;

	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (ASC == nullptr)
	{
		UnregisterOverkillListener();
		FinishTest(EFunctionalTestResult::Failed, FString::Printf(TEXT("[%s] Tick: ASC lost before assertions"), *RowLabel));
		return;
	}

	EvaluateExpectations(ASC);
	UnregisterOverkillListener();
}

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

UAbilitySystemComponent* AAFLCombatPipelineTest_Base::ResolvePlayerASC() const
{
	// BM-DEBT-AUDIT-001 / closes BM-DEBT-008: Lyra's ASC lives on LyraPlayerState
	// (IAbilitySystemInterface), not on the pawn. Pre-fix the test-base resolver
	// returned nullptr for any Lyra-stack PIE/automation run; tests passed only by
	// happenstance (mocked ASCs or null-tolerant assertions). Now resolves via the
	// engine helper, parallel to UAFLCombatCheats::GetPlayerASC.
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APlayerState* PS = PC->PlayerState)
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
		}
	}
	return nullptr;
}

void AAFLCombatPipelineTest_Base::ApplyAttributeOverride(UAbilitySystemComponent* ASC, const FString& ShortName, float Value)
{
	if (ASC == nullptr)
	{
		return;
	}

	FGameplayAttribute Attr;
	if      (ShortName.Equals(TEXT("Health"),            ESearchCase::IgnoreCase)) Attr = ULyraHealthSet::GetHealthAttribute();        // CONVERGENCE: Health lives on the Lyra set
	else if (ShortName.Equals(TEXT("MaxHealth"),         ESearchCase::IgnoreCase)) Attr = ULyraHealthSet::GetMaxHealthAttribute();     // CONVERGENCE
	else if (ShortName.Equals(TEXT("Shield"),            ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetShieldAttribute();
	else if (ShortName.Equals(TEXT("MaxShield"),         ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetMaxShieldAttribute();
	else if (ShortName.Equals(TEXT("Armor"),             ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetArmorAttribute();
	else if (ShortName.Equals(TEXT("OverkillThreshold"), ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetOverkillThresholdAttribute();
	else if (ShortName.Equals(TEXT("Damage"),            ESearchCase::IgnoreCase)) Attr = UAFLAttributeSet_Combat::GetDamageAttribute();
	else
	{
		UE_LOG(LogAFLCombatTests, Warning, TEXT("[%s] ApplyAttributeOverride: unknown attribute '%s'"), *RowLabel, *ShortName);
		return;
	}

	ASC->ApplyModToAttribute(Attr, EGameplayModOp::Override, Value);
}

void AAFLCombatPipelineTest_Base::ApplyPreconditions(UAbilitySystemComponent* ASC)
{
	// Clamp-safe order: MaxShield before Shield so the Shield clamp doesn't
	// truncate against a default MaxShield=0.
	if (PreconditionMaxShield >= 0.0f)
	{
		ApplyAttributeOverride(ASC, TEXT("MaxShield"), PreconditionMaxShield);
	}
	if (PreconditionShield >= 0.0f)
	{
		ApplyAttributeOverride(ASC, TEXT("Shield"), PreconditionShield);
	}
	if (PreconditionArmor >= 0.0f)
	{
		ApplyAttributeOverride(ASC, TEXT("Armor"), PreconditionArmor);
	}
	if (PreconditionOverkillThreshold >= 0.0f)
	{
		ApplyAttributeOverride(ASC, TEXT("OverkillThreshold"), PreconditionOverkillThreshold);
	}
}

void AAFLCombatPipelineTest_Base::RegisterOverkillListener()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FGameplayTag OverkillTag = FGameplayTag::RequestGameplayTag(NAME_Event_Damage_Overkill_PipelineBase, false);
	if (!OverkillTag.IsValid())
	{
		UE_LOG(LogAFLCombatTests, Warning, TEXT("[%s] RegisterOverkillListener: Event.Damage.Overkill tag missing"), *RowLabel);
		return;
	}

	OverkillListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		OverkillTag,
		this,
		&AAFLCombatPipelineTest_Base::OnOverkillReceived);
}

void AAFLCombatPipelineTest_Base::UnregisterOverkillListener()
{
	if (OverkillListenerHandle.IsValid())
	{
		OverkillListenerHandle.Unregister();
		OverkillListenerHandle = FGameplayMessageListenerHandle();
	}
}

void AAFLCombatPipelineTest_Base::FireDamage(UAbilitySystemComponent* ASC)
{
	if (ASC == nullptr)
	{
		return;
	}

	UClass* GEClass = LoadClass<UGameplayEffect>(nullptr,
		TEXT("/AFLCombat/Effects/GE_AFL_Damage_Instant.GE_AFL_Damage_Instant_C"));
	if (GEClass == nullptr)
	{
		UE_LOG(LogAFLCombatTests, Warning, TEXT("[%s] FireDamage: GE_AFL_Damage_Instant not loaded"), *RowLabel);
		return;
	}

	// Write Source.Damage; ApplyModToAttribute server-gates internally.
	ASC->ApplyModToAttribute(
		UAFLAttributeSet_Combat::GetDamageAttribute(),
		EGameplayModOp::Override,
		DamageBase);

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(ASC->GetOwnerActor(), ASC->GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GEClass, /*Level=*/1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogAFLCombatTests, Warning, TEXT("[%s] FireDamage: MakeOutgoingSpec failed"), *RowLabel);
		return;
	}

	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot_PipelineBase,  false), DamageHeadshot);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint_PipelineBase, false), DamageWeakpoint);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance_PipelineBase,  false), DamageDistance);

	ASC->ApplyGameplayEffectSpecToSelf(Spec);

	UE_LOG(LogAFLCombatTests, Display,
		TEXT("[%s] FireDamage: Base=%.1f Headshot=%.2f Weakpoint=%.2f Distance=%.2f"),
		*RowLabel, DamageBase, DamageHeadshot, DamageWeakpoint, DamageDistance);
}

void AAFLCombatPipelineTest_Base::EvaluateExpectations(UAbilitySystemComponent* ASC)
{
	// CONVERGENCE: Health migrated to ULyraHealthSet (UAFLDamageExecCalc outputs to its Damage meta now), so assert
	// the SAME set the HUD bar + death path read. Shield stays on the AFL set (zone-HP/armor/shield never converged).
	const float ObservedHealth = ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
	const float ObservedShield = ASC->GetNumericAttribute(UAFLAttributeSet_Combat::GetShieldAttribute());

	const FString HealthLabel  = FString::Printf(TEXT("[%s] Health"), *RowLabel);
	const FString ShieldLabel  = FString::Printf(TEXT("[%s] Shield"), *RowLabel);
	const FString OverkillLabel = FString::Printf(TEXT("[%s] OverkillCount"), *RowLabel);

	AssertEqual_Float(ObservedHealth, ExpectedHealth, HealthLabel, Tolerance);
	AssertEqual_Float(ObservedShield, ExpectedShield, ShieldLabel, Tolerance);
	AssertEqual_Int(ObservedOverkillCount, ExpectedOverkillCount, OverkillLabel);

	UE_LOG(LogAFLCombatTests, Display,
		TEXT("[%s] Observed: Health=%.2f (expected %.2f) Shield=%.2f (expected %.2f) OverkillCount=%d (expected %d)"),
		*RowLabel,
		ObservedHealth, ExpectedHealth,
		ObservedShield, ExpectedShield,
		ObservedOverkillCount, ExpectedOverkillCount);

	// AFunctionalTest tracks assertion failures via internal counters and
	// surfaces them through Result. We call FinishTest with Default and let
	// the framework decide pass/fail based on assertion history.
	FinishTest(EFunctionalTestResult::Default, FString::Printf(TEXT("[%s] complete"), *RowLabel));
}

void AAFLCombatPipelineTest_Base::OnOverkillReceived(FGameplayTag Channel, const FLyraVerbMessage& Message)
{
	ObservedOverkillCount++;
	UE_LOG(LogAFLCombatTests, Display,
		TEXT("[%s] Overkill #%d: Instigator=%s Target=%s Magnitude=%.1f"),
		*RowLabel,
		ObservedOverkillCount,
		*GetNameSafe(Message.Instigator),
		*GetNameSafe(Message.Target),
		Message.Magnitude);
}
