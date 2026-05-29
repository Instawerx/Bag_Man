# C++ & Engine Programming Reference

## Module Setup (Best Practice)

```
MyGame/
├── MyGame.Build.cs         # Game module
├── MyGameEditor.Build.cs   # Editor-only module
└── Source/
    ├── Private/
    └── Public/
```

```csharp
// MyGame.Build.cs
public class MyGame : ModuleRules
{
    public MyGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Slate", "SlateCore"
        });
    }
}
```

---

## Subsystems (Preferred over Singletons)

```cpp
// MyGameSubsystem.h
UCLASS()
class MYGAME_API UMyGameSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Access pattern
    static UMyGameSubsystem* Get(const UObject* WorldContext);

private:
    // Internal state
    TMap<FGameplayTag, TArray<TWeakObjectPtr<AActor>>> TaggedActors;
};

// MyGameSubsystem.cpp
void UMyGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Init logic here — GI is valid, World may not be yet
}

UMyGameSubsystem* UMyGameSubsystem::Get(const UObject* WorldContext)
{
    if (!IsValid(WorldContext)) return nullptr;
    UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext);
    return GI ? GI->GetSubsystem<UMyGameSubsystem>() : nullptr;
}
```

**Subsystem types:**
| Type | Lifetime | Use For |
|---|---|---|
| `UGameInstanceSubsystem` | Entire session | Progression, save, matchmaking |
| `UWorldSubsystem` | Per-world/map | Gameplay services, spawning |
| `ULocalPlayerSubsystem` | Per local player | Input, UI, player prefs |
| `UEngineSubsystem` | Editor+Runtime | Cross-world tooling |

---

## Gameplay Ability System (GAS) Setup

```cpp
// AbilitySystemComponent setup on Character
// MyCharacter.h
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

UPROPERTY()
TObjectPtr<UMyAttributeSet> AttributeSet;

// Implement IAbilitySystemInterface
virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

// MyCharacter.cpp — init for PlayerState-owned ASC (multiplayer AAA standard)
void AMyCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    AMyPlayerState* PS = GetPlayerState<AMyPlayerState>();
    if (ensure(PS))
    {
        AbilitySystemComponent = PS->GetAbilitySystemComponent();
        AbilitySystemComponent->InitAbilityActorInfo(PS, this);
        AttributeSet = PS->GetAttributeSet();
    }
}
```

---

## Large Team Architecture (80+ Developers)

### Modular Gameplay Plugins

Split the project into Feature Plugins to reduce coupling and compile times:

```
MyGame/
├── Plugins/
│   ├── GameFeatures/
│   │   ├── ShooterCore/        # Weapons, damage, projectiles
│   │   ├── InventorySystem/    # Items, pickups, equipment
│   │   ├── AIModule/           # Enemy logic, behavior
│   │   └── UIFramework/        # HUD, menus, widget base
│   └── ThirdParty/
└── Source/
    └── MyGame/                 # Thin game module, just wires plugins together
```

Each plugin has its own `Build.cs`, assets, and tests. Teams own plugins, not files.

### IWYU (Include What You Use) — Mandatory for Large Teams

```csharp
// Build.cs — enforce IWYU
PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
bEnforceIWYU = true; // Fail build on missing includes
```

```cpp
// ✅ Explicit includes — no relying on CoreMinimal pulling things in
#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

// ❌ Never rely on transitive includes in AAA projects
```

### Asset Naming Conventions (Enforced via Asset Validation)

| Asset Type | Prefix | Example |
|---|---|---|
| Blueprint | `BP_` | `BP_EnemySoldier` |
| Static Mesh | `SM_` | `SM_Rock_01` |
| Skeletal Mesh | `SK_` | `SK_PlayerCharacter` |
| Material | `M_` | `M_MetalRusty` |
| Material Instance | `MI_` | `MI_MetalRusty_Red` |
| Texture | `T_` | `T_Rock_D` (D=diffuse) |
| Niagara System | `NS_` | `NS_Explosion` |
| Animation Blueprint | `ABP_` | `ABP_PlayerCharacter` |
| Data Asset | `DA_` | `DA_WeaponStats_Rifle` |

### Redirectors — Never Break References

```ini
; Config/DefaultEngine.ini — asset redirector on rename
[CoreRedirects]
+ObjectRedirects=(OldName="/Game/OldPath/BP_MyActor",NewName="/Game/NewPath/BP_MyActor")
+ClassRedirects=(OldName="OldClassName",NewName="NewClassName")
```

```cpp
// Correct async load pattern — never LoadObject at runtime
void UMyLoadingComponent::RequestAssetLoad(const TSoftObjectPtr<UStaticMesh>& MeshRef)
{
    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

    LoadHandle = Streamable.RequestAsyncLoad(
        MeshRef.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &UMyLoadingComponent::OnMeshLoaded)
    );
}

void UMyLoadingComponent::OnMeshLoaded()
{
    // Safe to use now
    if (UStaticMesh* Mesh = MeshRef.Get())
    {
        StaticMeshComponent->SetStaticMesh(Mesh);
    }
}
```

---

## Delegates & Events (Large Team Patterns)

```cpp
// Prefer multicast delegates for events, not tight coupling
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float /*NewHealth*/, float /*MaxHealth*/);

// Dynamic multicast for Blueprint bindability
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActorDied, AActor*, DeadActor);

UPROPERTY(BlueprintAssignable, Category = "Events")
FOnActorDied OnActorDied;
```

---

## Thread Safety

```cpp
// Use FRWScopeLock for read-heavy shared data
FRWLock DataLock;
TArray<FMyData> SharedData;

// Read (many threads concurrently)
void ReadData()
{
    FRWScopeLock ReadLock(DataLock, SLT_ReadOnly);
    // safe read
}

// Write (exclusive)
void WriteData(const FMyData& NewData)
{
    FRWScopeLock WriteLock(DataLock, SLT_Write);
    SharedData.Add(NewData);
}

// Always dispatch to game thread for UObject access
AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UMyClass>(this)]()
{
    if (UMyClass* This = WeakThis.Get())
    {
        This->SomeUObjectOperation();
    }
});
```
