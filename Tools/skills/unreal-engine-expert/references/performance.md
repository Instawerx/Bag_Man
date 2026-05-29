# Performance & Optimization Reference (UE5)

## Profiling First — Always

Never optimize without data. Use these tools in order:

1. **`stat unit`** — Frame, Game, Draw, GPU budget at a glance
2. **`stat fps`** + **`stat unitgraph`** — Frame time history
3. **Unreal Insights** (`UnrealInsights.exe`) — CPU/GPU timeline, memory, networking
4. **`profilegpu`** console command — Per-pass GPU breakdown
5. **RenderDoc** integration — Per-draw GPU capture
6. **`memreport -full`** — Memory breakdown by category

```
// Key stat commands
stat unit          // Overall frame budget
stat game          // GameThread breakdown  
stat gpu           // GPU pass timings
stat scenerendering // Draw call counts
stat memory        // Memory categories
stat particles     // Niagara stats
stat net           // Network bandwidth
```

---

## Tick Optimization

```cpp
// ❌ Bad — ticking every frame for something that changes rarely
void AMyActor::Tick(float DeltaTime)
{
    CheckIfNearPlayer(); // Runs 60x/sec
}

// ✅ Good — timer-based polling
void AMyActor::BeginPlay()
{
    GetWorldTimerManager().SetTimer(
        ProximityTimerHandle,
        this, &AMyActor::CheckIfNearPlayer,
        0.2f,   // Every 200ms is fine for proximity checks
        true
    );
}

// ✅ Even better — event-driven via overlap
void AMyActor::BeginPlay()
{
    TriggerVolume->OnComponentBeginOverlap.AddDynamic(
        this, &AMyActor::OnPlayerEntered);
}

// Reduce tick group for non-physics actors
AMyActor::AMyActor()
{
    PrimaryActorTick.TickGroup = TG_DuringPhysics; // or TG_PostUpdateWork
    PrimaryActorTick.TickInterval = 0.1f;           // 10Hz instead of 60Hz
}
```

---

## Draw Call Reduction

```cpp
// Instanced Static Mesh (ISM) for repeated static geometry
UPROPERTY(VisibleAnywhere)
TObjectPtr<UInstancedStaticMeshComponent> ISMComponent;

// Add instances (much cheaper than spawning N actors)
for (const FTransform& T : SpawnTransforms)
{
    ISMComponent->AddInstance(T);
}

// Hierarchical ISM for foliage (adds distance culling)
UPROPERTY(VisibleAnywhere)
TObjectPtr<UHierarchicalInstancedStaticMeshComponent> HISMComponent;
```

---

## Object Pooling

```cpp
// Simple actor pool
UCLASS()
class UActorPoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    template<class T>
    T* Acquire(TSubclassOf<T> ActorClass, const FTransform& SpawnTransform);

    void Release(AActor* Actor);

private:
    TMap<UClass*, TArray<TObjectPtr<AActor>>> Pool;
};

template<class T>
T* UActorPoolSubsystem::Acquire(TSubclassOf<T> ActorClass, const FTransform& SpawnTransform)
{
    TArray<TObjectPtr<AActor>>& Available = Pool.FindOrAdd(ActorClass);
    if (Available.Num() > 0)
    {
        T* Actor = Cast<T>(Available.Pop());
        Actor->SetActorTransform(SpawnTransform);
        Actor->SetActorHiddenInGame(false);
        Actor->SetActorTickEnabled(true);
        return Actor;
    }
    return GetWorld()->SpawnActor<T>(ActorClass, SpawnTransform);
}

void UActorPoolSubsystem::Release(AActor* Actor)
{
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorTickEnabled(false);
    Pool.FindOrAdd(Actor->GetClass()).Add(Actor);
}
```

---

## Memory Optimization

```cpp
// Async load and unload asset bundles
void UMyLoadManager::LoadBundle(FPrimaryAssetId BundleId)
{
    TArray<FName> Bundles = { FName("GameplayBundle") };
    UAssetManager::Get().LoadPrimaryAsset(BundleId, Bundles,
        FStreamableDelegate::CreateUObject(this, &UMyLoadManager::OnBundleLoaded));
}

void UMyLoadManager::UnloadBundle(FPrimaryAssetId BundleId)
{
    UAssetManager::Get().UnloadPrimaryAsset(BundleId); // Frees hard refs
}
```

---

## Scalability Settings (Large Team Standard)

```ini
; DefaultScalability.ini — define quality tiers
[EffectsQuality@2]
r.Niagara.QualityLevel=2
r.ParticleLODBias=0
fx.Niagara.MaxSystemInstances=1000

[EffectsQuality@1]
r.Niagara.QualityLevel=1
fx.Niagara.MaxSystemInstances=500

[EffectsQuality@0]
r.Niagara.QualityLevel=0
fx.Niagara.MaxSystemInstances=200
```

```cpp
// Apply scalability tier at runtime
Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
Levels.EffectsQuality = 2; // 0=Low 1=Medium 2=High 3=Epic
Scalability::SetQualityLevels(Levels);
```

---

## Common Performance Red Flags

| Issue | Detection | Fix |
|---|---|---|
| Tick overload | `stat game` > 8ms | Timer/event-driven, reduce TickInterval |
| Too many draw calls | `stat scenerendering` DrawPrimitiveCalls > 2000 | ISM/HISM, merge meshes, Nanite |
| Shader permutations | Long cook times, spikes | Reduce dynamic branches in materials |
| GC stalls | Insights GC spikes | Reduce UPROPERTY count, use pools |
| Shadow cascade cost | `profilegpu` shadows high | Reduce DynamicShadowDistanceMovableLight |
| CPU-GPU sync | `stat unit` GPU waiting | Async compute, reduce readback calls |
