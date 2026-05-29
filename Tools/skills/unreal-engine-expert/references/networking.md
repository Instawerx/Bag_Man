# Multiplayer & Networking Reference (UE5)

## Replication Fundamentals

```cpp
// Actor replication setup
AMyActor::AMyActor()
{
    bReplicates = true;
    bAlwaysRelevant = false;          // Use relevancy culling
    NetUpdateFrequency = 60.f;        // Max updates/sec to clients
    MinNetUpdateFrequency = 10.f;     // Min when nothing changes
    NetCullDistanceSquared = 225000000.f; // ~15000 units
}

void AMyActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Always replicate
    DOREPLIFETIME(AMyActor, Health);

    // Conditional — only to owner
    DOREPLIFETIME_CONDITION(AMyActor, Ammo, COND_OwnerOnly);

    // With notification callback
    DOREPLIFETIME_CONDITION_NOTIFY(AMyActor, bIsAlive, COND_None, REPNOTIFY_Always);
}
```

---

## RPCs

```cpp
// Server RPC — called on client, executes on server
UFUNCTION(Server, Reliable, WithValidation)
void ServerFire(FVector Origin, FVector Direction);

void AMyCharacter::ServerFire_Implementation(FVector Origin, FVector Direction)
{
    // Validate then execute on server
    PerformHitscan(Origin, Direction);
}

bool AMyCharacter::ServerFire_Validate(FVector Origin, FVector Direction)
{
    // Anti-cheat: validate inputs are sane
    return Direction.IsNormalized() && !Origin.ContainsNaN();
}

// Multicast — server calls, runs on all clients
UFUNCTION(NetMulticast, Unreliable)
void MulticastPlayHitEffect(FVector HitLocation);

// Client RPC — server calls, runs on owning client only
UFUNCTION(Client, Reliable)
void ClientReceiveInventory(const TArray<FItemData>& Items);
```

---

## RepNotify

```cpp
// Declare
UPROPERTY(ReplicatedUsing = OnRep_Health)
float Health;

UFUNCTION()
void OnRep_Health(float OldHealth); // Pass old value for delta

// Implement
void AMyCharacter::OnRep_Health(float OldHealth)
{
    // Called on clients when Health changes
    float Delta = Health - OldHealth;
    HealthBar->SetPercent(Health / MaxHealth);

    if (Delta < 0.f)
    {
        PlayHitReaction(FMath::Abs(Delta));
    }
}
```

---

## GAS Networking

GAS handles its own replication — use it correctly:

```cpp
// Activate ability (client-predicted)
AbilitySystemComponent->TryActivateAbility(AbilityHandle);

// Apply GameplayEffect (server-authoritative)
// Always apply on server; GAS replicates effect to clients
if (HasAuthority())
{
    FGameplayEffectContextHandle Context =
        AbilitySystemComponent->MakeEffectContext();
    FGameplayEffectSpecHandle Spec =
        AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, Level, Context);

    AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
        *Spec.Data.Get(), TargetASC);
}
```

---

## Iris (UE5.2+ Next-Gen Replication)

```cpp
// Opt-in to Iris in DefaultEngine.ini
// [/Script/Engine.ReplicationDriverConfig]
// ReplicationDriverClassName="/Script/IrisCore.ReplicationSystem"

// Iris replication fragment (replaces GetLifetimeReplicatedProps for Iris)
void AMyActor::RegisterReplicationFragments(
    UE::Net::FFragmentRegistrationContext& Context,
    UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
    // Iris handles dirty tracking automatically
    UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(
        this, Context, RegistrationFlags);
}
```

---

## Relevancy & Priority

```cpp
// Custom relevancy (e.g., always relevant to team members)
bool AMyActor::IsNetRelevantFor(
    const AActor* RealViewer,
    const AActor* ViewTarget,
    const FVector& SrcLocation) const
{
    // Relevant to same team
    if (AMyPlayerState* PS = RealViewer->GetPlayerState<AMyPlayerState>())
    {
        if (PS->TeamID == TeamID) return true;
    }
    return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

// Priority (higher = more bandwidth, synced more often)
float AMyActor::GetNetPriority(
    const FVector& ViewPos, const FVector& ViewDir,
    AActor* Viewer, AActor* ViewTarget,
    UActorChannel* InChannel, float Time, bool bLowBandwidth) const
{
    float Priority = Super::GetNetPriority(ViewPos, ViewDir, Viewer,
                                           ViewTarget, InChannel, Time, bLowBandwidth);
    // Boost priority for actors being aimed at
    if (IsAimedAt(ViewPos, ViewDir)) Priority *= 2.f;
    return Priority;
}
```

---

## Common Networking Pitfalls

| Pitfall | Fix |
|---|---|
| Calling `GetPlayerController()` on a non-owning client | Check `IsLocallyControlled()` first |
| Using `Reliable` for cosmetic RPCs | Use `Unreliable` for hit effects, sounds |
| Replicating every frame | Use `NetUpdateFrequency` + dirty checking |
| Authority checks missing | Always `if (HasAuthority())` before state mutation |
| Large struct replication | Break into delta-friendly fields or use FastArraySerializer |

## FastArraySerializer (for replicated arrays)

```cpp
USTRUCT()
struct FInventoryList : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FInventoryItem> Items;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryItem,
               FInventoryList>(Items, DeltaParms, *this);
    }
};
```
