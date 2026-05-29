# Gameplay Systems & AI Reference (UE5)

## Gameplay Ability System (GAS) — Full Setup

### AttributeSet
```cpp
UCLASS()
class UMyAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    // Macro generates getter, setter, and initter
    ATTRIBUTE_ACCESSORS(UMyAttributeSet, Health)
    ATTRIBUTE_ACCESSORS(UMyAttributeSet, MaxHealth)
    ATTRIBUTE_ACCESSORS(UMyAttributeSet, Damage) // Meta attribute — temp calc only

    virtual void PreAttributeChange(
        const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(
        const FGameplayEffectModCallbackData& Data) override;

private:
    UPROPERTY(BlueprintReadOnly, Category="Attributes", meta=(AllowPrivateAccess="true"))
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, Category="Attributes", meta=(AllowPrivateAccess="true"))
    FGameplayAttributeData MaxHealth;

    UPROPERTY(BlueprintReadOnly, Category="Attributes", meta=(AllowPrivateAccess="true"))
    FGameplayAttributeData Damage;
};

void UMyAttributeSet::PreAttributeChange(
    const FGameplayAttribute& Attribute, float& NewValue)
{
    // Clamp health to [0, MaxHealth]
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
}
```

### Fire Ability — Client Prediction + Server Projectile

```cpp
// GA_Fire.h
UCLASS()
class UGA_Fire : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_Fire();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category="Fire")
    TObjectPtr<UAnimMontage> FireMontage;

    UPROPERTY(EditDefaultsOnly, Category="Fire")
    TSubclassOf<AProjectile> ProjectileClass;

    UFUNCTION()
    void OnMontageCompleted();

private:
    UFUNCTION(Server, Reliable)
    void ServerSpawnProjectile(FVector_NetQuantize Origin,
                               FVector_NetQuantizeNormal Direction);
};

// GA_Fire.cpp
void UGA_Fire::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // Commit (checks/consumes cost & cooldown)
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Play montage locally (client-predicted — runs on both client and server)
    UAbilityTask_PlayMontageAndWait* MontageTask =
        UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, FireMontage, 1.f);
    MontageTask->OnCompleted.AddDynamic(this, &UGA_Fire::OnMontageCompleted);
    MontageTask->OnInterrupted.AddDynamic(this, &UGA_Fire::OnMontageCompleted);
    MontageTask->ReadyForActivation();

    // Spawn projectile on server only
    if (ActorInfo->IsNetAuthority())
    {
        ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
        if (IsValid(Character))
        {
            FVector Origin = Character->GetMesh()->GetSocketLocation(FName("muzzle"));
            FVector Direction = Character->GetControlRotation().Vector();
            ServerSpawnProjectile(Origin, Direction);
        }
    }
    else
    {
        // Client: send aim data to server via RPC
        ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
        if (IsValid(Character))
        {
            FVector Origin = Character->GetMesh()->GetSocketLocation(FName("muzzle"));
            FVector Direction = Character->GetControlRotation().Vector();
            ServerSpawnProjectile(Origin, Direction);
        }
    }
}

void UGA_Fire::ServerSpawnProjectile_Implementation(
    FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
    // Server authority — spawn is replicated to clients automatically
    FTransform SpawnTransform(Direction.Rotation(), Origin);
    AProjectile* Projectile = GetWorld()->SpawnActorDeferred<AProjectile>(
        ProjectileClass, SpawnTransform, GetOwningActorFromActorInfo());

    if (IsValid(Projectile))
    {
        Projectile->InitVelocity(Direction);
        UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
    }
}

void UGA_Fire::OnMontageCompleted()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo,
               CurrentActivationInfo, true, false);
}
```

> **Prediction note**: Use `FGameplayAbilityTargetData` + `UAbilityTask_WaitTargetData` for hitscan with server validation, or rely on `FVector_NetQuantize` RPC args + server-side lag compensation for projectile spawning.

---

### Gameplay Effect (C++ Application)
```cpp
// Apply damage via GE
void UMyDamageExecution::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    FAggregatorEvaluateParameters EvalParams;

    float BaseDamage = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
        DamageStatics().DamageDef, EvalParams, BaseDamage);

    // Apply resistances, armor, crits here...

    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
        UMyAttributeSet::GetHealthAttribute(),
        EGameplayModOp::Additive,
        -BaseDamage
    ));
}
```

---

## Enhanced Input

```cpp
// MyCharacter.cpp — binding Enhanced Input actions
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    UEnhancedInputComponent* EIC =
        CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

    // Add mapping context
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AMyCharacter::Jump);
    EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyCharacter::Move);
    EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyCharacter::Look);
    EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AMyCharacter::StartFire);
}

void AMyCharacter::Move(const FInputActionValue& Value)
{
    FVector2D MovementVector = Value.Get<FVector2D>();
    AddMovementInput(GetActorForwardVector(), MovementVector.Y);
    AddMovementInput(GetActorRightVector(), MovementVector.X);
}
```

---

## Behavior Trees (Classic AI)

```cpp
// Custom BTTask
UCLASS()
class UBTTask_AttackTarget : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Blackboard")
    FBlackboardKeySelector TargetKey;

    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

EBTNodeResult::Type UBTTask_AttackTarget::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* Controller = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
    if (!IsValid(Target)) return EBTNodeResult::Failed;

    // Trigger ability via GAS. UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent
    // resolves IAbilitySystemInterface correctly regardless of whether the pawn owns the
    // ASC directly or delegates ownership (e.g. to PlayerState in Lyra-derived projects).
    // See cpp-engine.md GAS Setup for the canonical AAA pattern.
    APawn* OwnerPawn = Controller->GetPawn();
    if (UAbilitySystemComponent* ASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerPawn))
    {
        ASC->TryActivateAbilitiesByTag(AttackGameplayTag);
    }

    return EBTNodeResult::Succeeded;
}
```

---

## StateTree (UE5 — Preferred for Complex AI)

```cpp
// StateTree is data-driven; define evaluators and tasks in C++

USTRUCT()
struct FMyAIStateTreeSchema : public FStateTreeSchema
{
    // Define what context data is available to this StateTree
    GENERATED_BODY()
};

// StateTree Task
USTRUCT(meta=(DisplayName="Chase Player"))
struct FMyChaseTask : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()
    using FInstanceDataType = FMyChaseTaskInstanceData;

    EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
        const FStateTreeTransitionResult& Transition) const;
    EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const;
};
```

---

## EQS (Environment Query System)

```cpp
// Run EQS query from C++
UAIBlueprintHelperLibrary::RunEQSQuery(
    this,           // World context
    QueryTemplate,  // UEnvQuery asset
    Querier,        // Who is asking
    EEnvQueryRunMode::SingleResult,
    UEnvQueryInstanceBlueprintWrapper::StaticClass()
);

// Or via async
FEnvQueryRequest QueryRequest(QueryTemplate, QueryOwner);
QueryRequest.Execute(EEnvQueryRunMode::SingleResult,
    this, &AMyAIController::OnEQSQueryFinished);

void AMyAIController::OnEQSQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
    if (Result->IsSuccessful())
    {
        FVector BestLocation = Result->GetItemAsLocation(0);
        MoveToLocation(BestLocation);
    }
}
```

---

## Mass AI (Crowds / 1000+ Agents)

```cpp
// Mass is ECS-based — define fragments (components), processors
USTRUCT()
struct FMyAgentFragment : public FMassFragment
{
    GENERATED_BODY()
    float Speed = 150.f;
    FVector TargetLocation;
};

// Processor runs on all entities with matching fragments
UCLASS()
class UMyAgentMovementProcessor : public UMassProcessor
{
    GENERATED_BODY()
public:
    UMyAgentMovementProcessor();
    virtual void Execute(FMassEntityManager& EntityManager,
                         FMassExecutionContext& Context) override;
};

void UMyAgentMovementProcessor::Execute(
    FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    Context.ForEachEntityChunk(EntityQuery, [this](FMassExecutionContext& Ctx)
    {
        TArrayView<FMyAgentFragment> Agents = Ctx.GetMutableFragmentView<FMyAgentFragment>();
        for (FMyAgentFragment& Agent : Agents)
        {
            // Move each agent toward target — runs in parallel on worker threads
        }
    });
}
```
