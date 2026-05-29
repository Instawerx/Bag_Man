# Blueprints & Visual Scripting Reference (UE5)

## AAA Blueprint Philosophy

> **Rule of thumb**: Logic that designers tweak → Blueprint. Logic that runs every frame or touches engine internals → C++.

| Belongs in Blueprint | Belongs in C++ |
|---|---|
| Ability activation triggers | Ability execution logic |
| UI widget logic | Attribute math, GE execution |
| Cutscene / level sequencer hooks | Replication, RPCs |
| Designer-tunable parameters | Physics, movement |
| Quick prototyping | Performance-critical paths |

---

## C++ → Blueprint Bridge

### Expose C++ to Blueprint
```cpp
// Function callable from Blueprint
UFUNCTION(BlueprintCallable, Category = "Inventory",
          meta=(ToolTip="Adds item to inventory. Returns false if full."))
bool AddItemToInventory(const FItemData& Item);

// Pure function (no exec pin, no side effects)
UFUNCTION(BlueprintPure, Category = "Stats")
float GetHealthPercent() const { return Health / MaxHealth; }

// Implementable in BP, called from C++
UFUNCTION(BlueprintImplementableEvent, Category = "Events")
void OnHealthChanged(float NewHealth, float OldHealth);

// C++ default + BP override
UFUNCTION(BlueprintNativeEvent, Category = "Combat")
float CalculateDamage(float BaseDamage, AActor* Instigator);
virtual float CalculateDamage_Implementation(float BaseDamage, AActor* Instigator);
```

### Blueprint-Implementable Interface
```cpp
UINTERFACE(BlueprintType)
class UInteractable : public UInterface { GENERATED_BODY() };

class IInteractable
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Interaction")
    void Interact(APawn* Interactor);
};

// Call interface safely (works on both C++ and BP implementations)
if (IInteractable* Interactable = Cast<IInteractable>(HitActor))
{
    IInteractable::Execute_Interact(HitActor, GetPawn());
}
```

---

## Async Blueprint Nodes (Latent Actions)

```cpp
// Create async BP node ("do X, then continue")
UCLASS()
class UAsyncTask_WaitForAbility : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable)
    FAbilityDelegate OnAbilityEnded;

    UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true"),
              Category="Abilities")
    static UAsyncTask_WaitForAbility* WaitForAbilityEnd(
        UAbilitySystemComponent* ASC,
        FGameplayTag AbilityTag);

    virtual void Activate() override;

private:
    TWeakObjectPtr<UAbilitySystemComponent> ASCWeakPtr;
    FGameplayTag TagToWatch;
    FDelegateHandle Handle;
};
```

---

## Blueprint Performance Guidelines

**Avoid in Blueprint:**
- Heavy loops (>100 iterations) — move to C++ task
- Per-frame Event Tick logic — use C++ with `TickInterval`
- String manipulation in hot paths — strings are heap-allocated
- `Get All Actors Of Class` — O(n) world scan, cache results

**Safe in Blueprint:**
- Event-driven logic (OnOverlap, OnDamaged, etc.)
- Calling C++ functions that do the heavy lifting
- UI binding (UMG)
- Sequencer / cinematic hooks

---

## Blueprint Function Libraries (Shared Utilities)

```cpp
UCLASS()
class UMyGameBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Accessible from any Blueprint in the project
    UFUNCTION(BlueprintPure, Category="Math",
              meta=(WorldContext="WorldContextObject"))
    static float GetDistanceToNearestEnemy(
        const UObject* WorldContextObject,
        AActor* Querier);

    UFUNCTION(BlueprintCallable, Category="Debug",
              meta=(DevelopmentOnly))
    static void DrawDebugArena(const UObject* WorldContextObject, FVector Center);
};
```

---

## Blueprint Struct Best Practices

```cpp
// Structs are value types — good for BP data passing
USTRUCT(BlueprintType)
struct FWeaponStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDamage = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireRate = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxAmmo = 30;

    // Custom comparison
    bool operator==(const FWeaponStats& Other) const
    {
        return FMath::IsNearlyEqual(BaseDamage, Other.BaseDamage)
            && FMath::IsNearlyEqual(FireRate, Other.FireRate)
            && MaxAmmo == Other.MaxAmmo;
    }
};
```
