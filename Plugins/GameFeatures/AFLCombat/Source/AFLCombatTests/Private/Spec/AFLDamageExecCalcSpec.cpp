// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Combat.Pipeline — Sprint 1.5 Layer A: contract/math tests for SSOT §8.3 damage pipeline.
//
// Z2 rewrite: each row is a discrete IMPLEMENT_SIMPLE_AUTOMATION_TEST instead of one
// BEGIN_DEFINE_SPEC group. The Spec macro variant didn't auto-register in our DeveloperTool
// module; the simple-test macro is more widely used across engine and Lyra and is known to
// register reliably (e.g. Source/Runtime/UMG/Private/Binding/States/Tests/WidgetStateBitfieldTests.cpp
// uses the same pattern in a similar module type).
//
// Each test reuses the same FFixture struct for setup/teardown — vanilla AActor + real
// ULyraAbilitySystemComponent + UAFLAttributeSet_Combat in a transient UWorld, no GameMode,
// no PlayerController, no spawn lifecycle.

#include "AbilitySystemComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AFLCombatTests.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Messages/LyraVerbMessage.h"
#include "Misc/AutomationTest.h"


// SetByCaller tags consumed by UAFLDamageExecCalc.
static const FName NAME_Data_Damage_Headshot  = TEXT("Data.Damage.Headshot");
static const FName NAME_Data_Damage_Weakpoint = TEXT("Data.Damage.Weakpoint");
static const FName NAME_Data_Damage_Distance  = TEXT("Data.Damage.Distance");

// Verb tag broadcast by the ExecCalc on overkill.
static const FName NAME_Event_Damage_Overkill = TEXT("Event.Damage.Overkill");

// Asset paths (Blueprint-derived GEs use _C suffix).
static const TCHAR* PATH_GE_Damage_Instant  = TEXT("/AFLCombat/Effects/GE_AFL_Damage_Instant.GE_AFL_Damage_Instant_C");
static const TCHAR* PATH_GE_Combat_InitData = TEXT("/AFLCombat/Effects/GE_AFL_Combat_InitData.GE_AFL_Combat_InitData_C");

// Common flags: runs in all application contexts; appears under Product filter.
#define AFL_TEST_FLAGS \
    (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)


/**
 * FAFLDamageTestFixture
 *
 * RAII helper: creates a transient game world, spawns a vanilla actor, attaches a real
 * ULyraAbilitySystemComponent + UAFLAttributeSet_Combat, applies the InitData GE to seed
 * defaults, and registers an Event.Damage.Overkill listener.
 *
 * Destruction tears the world down cleanly so each test starts from a fresh state.
 */
struct FAFLDamageTestFixture
{
    UWorld* World = nullptr;
    AActor* TestActor = nullptr;
    ULyraAbilitySystemComponent* ASC = nullptr;
    UAFLAttributeSet_Combat* AttrSet = nullptr;
    TSubclassOf<UGameplayEffect> DamageGEClass;
    TSubclassOf<UGameplayEffect> InitDataGEClass;
    FGameplayMessageListenerHandle OverkillHandle;
    int32 ObservedOverkillCount = 0;
    FAutomationTestBase* Test = nullptr;

    UGameInstance* GameInstance = nullptr;

    explicit FAFLDamageTestFixture(FAutomationTestBase* InTest)
        : Test(InTest)
    {
        World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld=*/false);
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(World);

        // UGameplayMessageSubsystem is a UGameInstanceSubsystem and asserts in Get() when
        // the world has no GameInstance. Construct one and bind it to the world before
        // running BeginPlay so any GameInstanceSubsystem accesses (including ours and the
        // listener subsystem if its gate ever changes) resolve cleanly.
        GameInstance = NewObject<UGameInstance>(GEngine);
        GameInstance->InitializeStandalone();
        WorldContext.OwningGameInstance = GameInstance;
        World->SetGameInstance(GameInstance);

        FURL URL;
        World->InitializeActorsForPlay(URL);
        World->BeginPlay();

        TestActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);

        ASC = NewObject<ULyraAbilitySystemComponent>(TestActor, TEXT("AbilitySystemComponent"));
        ASC->RegisterComponent();
        ASC->InitAbilityActorInfo(TestActor, TestActor);

        AttrSet = NewObject<UAFLAttributeSet_Combat>(TestActor);
        ASC->AddAttributeSetSubobject(AttrSet);

        DamageGEClass   = LoadClass<UGameplayEffect>(nullptr, PATH_GE_Damage_Instant);
        InitDataGEClass = LoadClass<UGameplayEffect>(nullptr, PATH_GE_Combat_InitData);

        // Listener — register before any damage so all overkill broadcasts during the test land here.
        const FGameplayTag OverkillTag = FGameplayTag::RequestGameplayTag(NAME_Event_Damage_Overkill, /*ErrorIfNotFound=*/false);
        if (OverkillTag.IsValid())
        {
            OverkillHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
                OverkillTag,
                [this](FGameplayTag /*Channel*/, const FLyraVerbMessage& /*Message*/)
                {
                    ++ObservedOverkillCount;
                });
        }

        ApplyInitData();
    }

    ~FAFLDamageTestFixture()
    {
        if (OverkillHandle.IsValid())
        {
            OverkillHandle.Unregister();
        }
        if (World)
        {
            World->EndPlay(EEndPlayReason::Quit);
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(/*bBroadcastWorldDestroyedEvent=*/false);
        }
    }

    void ApplyInitData()
    {
        if (!ASC || !InitDataGEClass)
        {
            return;
        }
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InitDataGEClass, 1.0f, Context);
        if (SpecHandle.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }

    void OverrideAttribute(const FGameplayAttribute& Attribute, float Value)
    {
        if (ASC)
        {
            ASC->ApplyModToAttribute(Attribute, EGameplayModOp::Override, Value);
        }
    }

    float ReadAttribute(const FGameplayAttribute& Attribute) const
    {
        return ASC ? ASC->GetNumericAttribute(Attribute) : 0.0f;
    }

    /** Fires one damage GE with the given Source.Damage value + SetByCaller multipliers. */
    void FireDamage(float Base, float Headshot = 1.0f, float Weakpoint = 1.0f, float Distance = 1.0f)
    {
        if (!ASC || !DamageGEClass) { return; }

        ASC->ApplyModToAttribute(
            UAFLAttributeSet_Combat::GetDamageAttribute(),
            EGameplayModOp::Override,
            Base);

        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        Context.AddInstigator(TestActor, TestActor);
        FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DamageGEClass, 1.0f, Context);
        if (!SpecHandle.IsValid())
        {
            return;
        }

        FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
        Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot,  false), Headshot);
        Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint, false), Weakpoint);
        Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance,  false), Distance);

        ASC->ApplyGameplayEffectSpecToSelf(Spec);
    }
};


// -----------------------------------------------------------------------------
// T1 — BaseDamage=25, no preconditions → Health 100 -> 75, no overkill
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPipeline_T1_BaseDamage25,
    "AFL.Combat.Pipeline.T1_BaseDamage25", AFL_TEST_FLAGS)
bool FAFLPipeline_T1_BaseDamage25::RunTest(const FString& Parameters)
{
    FAFLDamageTestFixture Fx(this);
    Fx.FireDamage(/*Base=*/25.0f);
    TestEqual(TEXT("Health"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetHealthAttribute()), 75.0f, 0.5f);
    TestEqual(TEXT("Shield"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetShieldAttribute()),  0.0f, 0.5f);
    TestEqual(TEXT("OverkillCount"),  Fx.ObservedOverkillCount, 0);
    return true;
}

// -----------------------------------------------------------------------------
// T2 — BaseDamage=25, Armor=100 → Health 100 -> 87.5, no overkill
// Mitigation = 100/(100+100) = 0.5; EffDmg = 25 * 0.5 = 12.5
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPipeline_T2_Armor100,
    "AFL.Combat.Pipeline.T2_Armor100", AFL_TEST_FLAGS)
bool FAFLPipeline_T2_Armor100::RunTest(const FString& Parameters)
{
    FAFLDamageTestFixture Fx(this);
    Fx.OverrideAttribute(UAFLAttributeSet_Combat::GetArmorAttribute(), 100.0f);
    Fx.FireDamage(25.0f);
    TestEqual(TEXT("Health"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetHealthAttribute()), 87.5f, 0.5f);
    TestEqual(TEXT("Shield"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetShieldAttribute()),  0.0f, 0.5f);
    TestEqual(TEXT("OverkillCount"),  Fx.ObservedOverkillCount, 0);
    return true;
}

// -----------------------------------------------------------------------------
// T3 — Shield 50/50, BaseDamage=30 → Shield 50 -> 20, Health stays 100, no overkill
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPipeline_T3_Shield50_NoBleed,
    "AFL.Combat.Pipeline.T3_Shield50_NoBleed", AFL_TEST_FLAGS)
bool FAFLPipeline_T3_Shield50_NoBleed::RunTest(const FString& Parameters)
{
    FAFLDamageTestFixture Fx(this);
    Fx.OverrideAttribute(UAFLAttributeSet_Combat::GetMaxShieldAttribute(), 50.0f);
    Fx.OverrideAttribute(UAFLAttributeSet_Combat::GetShieldAttribute(),    50.0f);
    Fx.FireDamage(30.0f);
    TestEqual(TEXT("Health"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetHealthAttribute()), 100.0f, 0.5f);
    TestEqual(TEXT("Shield"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetShieldAttribute()),  20.0f, 0.5f);
    TestEqual(TEXT("OverkillCount"),  Fx.ObservedOverkillCount, 0);
    return true;
}

// -----------------------------------------------------------------------------
// T4 — Shield 50/50, BaseDamage=80 → Shield 50 -> 0, Health 100 -> 70, no overkill
// EffDmg=80, Shield absorbs 50; remainder 30 hits health (< 50 OverkillThreshold).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPipeline_T4_Shield50_Bleed,
    "AFL.Combat.Pipeline.T4_Shield50_Bleed", AFL_TEST_FLAGS)
bool FAFLPipeline_T4_Shield50_Bleed::RunTest(const FString& Parameters)
{
    FAFLDamageTestFixture Fx(this);
    Fx.OverrideAttribute(UAFLAttributeSet_Combat::GetMaxShieldAttribute(), 50.0f);
    Fx.OverrideAttribute(UAFLAttributeSet_Combat::GetShieldAttribute(),    50.0f);
    Fx.FireDamage(80.0f);
    TestEqual(TEXT("Health"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetHealthAttribute()), 70.0f, 0.5f);
    TestEqual(TEXT("Shield"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetShieldAttribute()),  0.0f, 0.5f);
    TestEqual(TEXT("OverkillCount"),  Fx.ObservedOverkillCount, 0);
    return true;
}

// -----------------------------------------------------------------------------
// T5 — BaseDamage=100 (no armor/shield) → Health 100 -> 0, overkill fires once
// HealthDelta=100 > OverkillThreshold=50.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPipeline_T5_Overkill,
    "AFL.Combat.Pipeline.T5_Overkill", AFL_TEST_FLAGS)
bool FAFLPipeline_T5_Overkill::RunTest(const FString& Parameters)
{
    FAFLDamageTestFixture Fx(this);
    Fx.FireDamage(100.0f);
    TestEqual(TEXT("Health"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetHealthAttribute()), 0.0f, 0.5f);
    TestEqual(TEXT("Shield"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetShieldAttribute()), 0.0f, 0.5f);
    TestEqual(TEXT("OverkillCount"),  Fx.ObservedOverkillCount, 1);
    return true;
}

// -----------------------------------------------------------------------------
// T6 — BaseDamage=30, Headshot=2x → Health 100 -> 40, overkill fires once
// Raw=60; no mitigation → 60 hits health (> 50 OverkillThreshold).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPipeline_T6_Headshot_Overkill,
    "AFL.Combat.Pipeline.T6_Headshot_Overkill", AFL_TEST_FLAGS)
bool FAFLPipeline_T6_Headshot_Overkill::RunTest(const FString& Parameters)
{
    FAFLDamageTestFixture Fx(this);
    Fx.FireDamage(30.0f, /*Headshot=*/2.0f);
    TestEqual(TEXT("Health"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetHealthAttribute()), 40.0f, 0.5f);
    TestEqual(TEXT("Shield"),         Fx.ReadAttribute(UAFLAttributeSet_Combat::GetShieldAttribute()),  0.0f, 0.5f);
    TestEqual(TEXT("OverkillCount"),  Fx.ObservedOverkillCount, 1);
    return true;
}
