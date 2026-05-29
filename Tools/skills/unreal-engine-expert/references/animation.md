# Animation & Rigging Reference (UE5)

## Animation Blueprint Architecture

**AAA Pattern: Layered ABP with LinkedAnimGraph**

```
MainCharacter_ABP
├── Locomotion Layer (speed, direction, lean)
├── Upper Body Layer (AimOffset, weapon poses)
├── Additive Layer (procedural IK, hit reactions)
└── LinkedAnimGraph → WeaponSpecific_ABP (hot-swappable)
```

```cpp
// Swap linked ABP at runtime (weapon change)
void AMyCharacter::EquipWeapon(AWeaponBase* NewWeapon)
{
    if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
    {
        if (UMyCharacterAnimInstance* CharAnim =
            Cast<UMyCharacterAnimInstance>(AnimInst))
        {
            CharAnim->SetWeaponAnimLayer(NewWeapon->GetAnimLayerClass());
        }
    }
}

// In AnimInstance
void UMyCharacterAnimInstance::SetWeaponAnimLayer(
    TSubclassOf<UAnimInstance> LayerClass)
{
    GetOwningComponent()->LinkAnimClassLayers(LayerClass);
}
```

---

## Animation Instance — Thread-Safe Updates

```cpp
UCLASS()
class UMyCharacterAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    // Called on worker thread — read character state here
    virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

    // Called on game thread — only if you must touch UObjects
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    // Cached values read by Anim Graph
    UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
    bool bIsInAir = false;
};

void UMyCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    // Use FAnimInstanceProxy for thread-safe character access
    if (const ACharacter* Character = Cast<ACharacter>(GetOwningActor()))
    {
        Speed = Character->GetVelocity().Size2D();
        bIsInAir = Character->GetCharacterMovement()->IsFalling();
    }
}
```

---

## Motion Matching (UE5.4+)

```cpp
// Motion Matching replaces traditional state machine locomotion
// Setup in AnimBP:
// 1. Add Motion Matching node
// 2. Point to PoseSearch Database asset
// 3. Feed Trajectory component data

// Generate trajectory for Motion Matching
void UMyCharacterAnimInstance::UpdateMotionMatchingTrajectory()
{
    if (UPoseSearchTrajectoryComponent* TrajComp =
        GetOwningActor()->FindComponentByClass<UPoseSearchTrajectoryComponent>())
    {
        // Trajectory is auto-updated; tune via component settings
        // Key params: HistoryLength, PredictionLength, SamplingInterval
    }
}
```

---

## Control Rig

```cpp
// Drive Control Rig from C++
void AMyCharacter::UpdateIKTargets()
{
    if (UControlRig* CR = ControlRigComponent->GetControlRig(0))
    {
        // Set foot IK targets
        CR->SetControlValue<FVector>(
            FName("foot_l_ctrl"),
            LeftFootIKTarget,
            ERigControlType::Position
        );

        CR->SetControlValue<FVector>(
            FName("foot_r_ctrl"),
            RightFootIKTarget,
            ERigControlType::Position
        );
    }
}
```

---

## IK Rig & Full Body IK — Foot Placement on Uneven Terrain

### 1. Line Traces for Ground Detection (C++)

```cpp
void AMyCharacter::SolveFootIK(float DeltaTime)
{
    auto TraceFootPlacement = [&](FName SocketName, FVector& OutLocation, FVector& OutNormal)
    {
        FVector SocketPos = GetMesh()->GetSocketLocation(SocketName);
        FVector Start = SocketPos + FVector(0, 0, 60.f);
        FVector End   = SocketPos - FVector(0, 0, 120.f);

        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
        {
            OutLocation = Hit.ImpactPoint;
            OutNormal   = Hit.ImpactNormal;
        }
        else
        {
            OutLocation = SocketPos;
            OutNormal   = FVector::UpVector;
        }
    };

    TraceFootPlacement(FName("foot_l"), LeftFootIKTarget,  LeftFootNormal);
    TraceFootPlacement(FName("foot_r"), RightFootIKTarget, RightFootNormal);

    // Pelvis adjustment — pull down so the lower foot can still reach
    float LeftDelta  = LeftFootIKTarget.Z  - GetMesh()->GetSocketLocation(FName("foot_l")).Z;
    float RightDelta = RightFootIKTarget.Z - GetMesh()->GetSocketLocation(FName("foot_r")).Z;
    float TargetPelvisOffset = FMath::Min(LeftDelta, RightDelta);

    // Smooth to avoid snapping on steep terrain transitions
    CurrentPelvisOffset = FMath::FInterpTo(CurrentPelvisOffset, TargetPelvisOffset, DeltaTime, 10.f);
}
```

### 2. AnimInstance — Thread-Safe IK Data Pass

```cpp
void UMyCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    if (const AMyCharacter* Char = Cast<AMyCharacter>(GetOwningActor()))
    {
        LeftFootIKLocation  = Char->LeftFootIKTarget;
        RightFootIKLocation = Char->RightFootIKTarget;
        PelvisOffset        = Char->CurrentPelvisOffset;

        // Blend IK out while airborne — avoid fighting physics
        const float TargetBlend = Char->GetCharacterMovement()->IsFalling() ? 0.f : 1.f;
        IKBlendWeight = FMath::FInterpTo(IKBlendWeight, TargetBlend, DeltaSeconds, 10.f);
    }
}
```

### 3. AnimGraph Setup

- **Modify Bone** node: offset Pelvis by `PelvisOffset` Z before IK evaluation
- **Two Bone IK** (per foot): set Effector to `LeftFootIKLocation` / `RightFootIKLocation`, drive Alpha with `IKBlendWeight`
- **Full Body IK** node: alternative for more joints — handles knee/hip push automatically

### 4. Control Rig (Production-Grade Alternative)

```cpp
// Drive foot effectors from C++ into Control Rig
if (UControlRig* CR = ControlRigComponent->GetControlRig(0))
{
    CR->SetControlValue<FVector>(FName("foot_l_ctrl"), LeftFootIKTarget,  ERigControlType::Position);
    CR->SetControlValue<FVector>(FName("foot_r_ctrl"), RightFootIKTarget, ERigControlType::Position);
}
```

Control Rig + FBIK solver is the preferred AAA approach — configure the IK Rig asset with goal bones and the solver handles full chain compensation automatically.

---

## Anim Notifies (C++)

```cpp
// Custom anim notify
UCLASS()
class UAnimNotify_SpawnProjectile : public UAnimNotify
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)
    FName SocketName = FName("muzzle_socket");

    virtual void Notify(USkeletalMeshComponent* MeshComp,
                        UAnimSequenceBase* Animation,
                        const FAnimNotifyEventReference& EventReference) override;
};

void UAnimNotify_SpawnProjectile::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    if (ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner()))
    {
        // Only spawn on server
        if (Character->HasAuthority())
        {
            FTransform SpawnTransform = MeshComp->GetSocketTransform(SocketName);
            Character->GetWorld()->SpawnActor<AProjectile>(
                ProjectileClass, SpawnTransform);
        }
    }
}
```

---

## Montage Control from C++

```cpp
// Play montage and bind to end event
void AMyCharacter::PlayAttackMontage(UAnimMontage* Montage, float Rate)
{
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (!AnimInstance || !Montage) return;

    float Duration = AnimInstance->Montage_Play(Montage, Rate);
    if (Duration > 0.f)
    {
        // Bind end delegate
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &AMyCharacter::OnAttackMontageEnded);
        AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
    }
}

void AMyCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (!bInterrupted)
    {
        // Combo window closed, reset attack state
        ResetComboState();
    }
}
```
