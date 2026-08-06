# IRONICS — Health Consumable (SSOT)

**Status:** Design SSOT for the AFL health consumable — a **full 3-surface game asset** (world pickup +
store SKU + loadout consumable), NOT a cosmetic. Dated 2026-07-18. Read this before any health-item build block.
**Supersedes** the master doc's part-#8 "boolean cosmetic matrix-row" framing (corrected in
`IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md` §3 + §1/§9 to point here).

**Companion docs:** `IRONICS_MARKETPLACE_MASTER_ARCHITECTURE.md` (part #8), `IRONICS_LOOT_SYSTEM_DESIGN.md`
(in-match *resource* pickups — a DIFFERENT thing: "NOT currency", no cross-session ownership),
`AFL_ECONOMY_ARCHITECTURE_ADR.md`.

---

## 0. THE 5 RULINGS (operator law — locked 2026-07-18, non-negotiable)

1. **World pickup = BOTH shapes.** (ii) **carriable ITEM** is primary — walk over → add `ID_AFL_Heal` to
   inventory, use later. (i) **instant-heal MED-STATION** is a variant — walk over → heal immediately, grants
   no item.
2. **Store persistence = CONSUMABLE INVENTORY ITEM** (PlayFab `RemainingUses`), **NOT** VirtualCurrency.
   Quantity is a per-SKU owned COUNT, not a currency code.
3. **Scope = ALL THREE PHASES** — the whole asset (world pickup → store SKU → loadout consumable), sequenced.
4. **Heal = FIXED amount ≈ 50% MaxHealth (CVar-tunable)** — NOT a full heal.
5. **Weapon spawners = repoint the 10 `B_WeaponSpawner` → `ID_BagMan_PulseCarbine`** (rifle-class all-rounder).
   Separate + cheap; rides along in Phase 1.

---

## 1. THE SHARED SPINE (build once — CONFORM-TO-PROVEN; templates already exist)

All three surfaces ride one item + one heal path. Both have working templates on disk (Lyra's `_Unused`
heal chain + AFL's own pickup/heal pieces).

**Item — `ID_AFL_Heal`** (`ULyraInventoryItemDefinition`), fragments (shape from `ID_AFL_Pistol` + Lyra
`ID_HealPickup`):
- `InventoryFragment_PickupIcon` — world-pickup icon/name.
- `InventoryFragment_QuickBarIcon` — occupies a QuickBar slot, selectable/usable.
- `InventoryFragment_SetStats` — seeds a **count tag = QUANTITY/charges** (recommend a dedicated
  `AFL.Consumable.Charges` tag, not the weapon `MagazineAmmo` tag the template hacks).
- `InventoryFragment_EquippableItem` → `WID_AFL_Heal`. **Drop `ReticleConfig`** (no aim for a consumable).

**Use — `WID_AFL_Heal`** (`LyraEquipmentDefinition`) grants `AbilitySet_AFL_Heal` → `GA_AFL_Heal`:
- On activate: `CommitAbility` with **cost = `LyraAbilityCost_ItemTagStack`** on the charge tag (decrements one
  charge — the proven consume mechanic from `GA_HealPickup`).
- Build a spec of **`UAFLGE_OverloadRestore`** (reused as-is — a pure Instant/Additive-Health/SetByCaller
  shell; no hardwired floor), `SetSetByCallerMagnitude(Data.Health.Restore, 0.5 * MaxHealth)` (ruling ④,
  CVar `afl.Heal.Consumable.Fraction`), `ApplyGameplayEffectSpecToSelf`. Targets **`UAFLAttributeSet_Combat.Health`**
  (AFL's own health, not Lyra's). Overheal auto-caps in `PreAttributeChange`. Pattern = `AFLDeathComponent.cpp:253`.
- Optionally add `GameplayCue.Character.Heal` to the spec for feedback.

**Art (harvest → IRONICS reskin):** `SM_healthpackFull` (+ `SM_healthpackPart`) + `MI_Item_HealthPack*` +
`T_HealthPack*` → `SM_AFL_HealthPack` / `MI_AFL_HealthPack`. Feedback cue = reskin `GCN_Character_Heal`
(`NS_Heal` + `sfx_Heal` + shake + haptics).

**Net-new in the spine (modest):** the authored IRONICS assets (`ID_AFL_Heal` / `WID_AFL_Heal` /
`AbilitySet_AFL_Heal` / `GA_AFL_Heal`), a *lighter* use-ability base (the template's `GA_HealPickup` derives
from the heavy ranged-weapon ability — adapt to a light `LyraGameplayAbility`), the fixed-heal magnitude policy
(ruling ④), the `AFL.Consumable.Charges` tag, adding the cue to the (currently silent) heal GE, and the art
reskin.

---

## 2. THE THREE SURFACES

| Surface | Shape | Phase | Effort |
|---|---|---|---|
| **① World pickup** | `AAFLHealthPickup` (clone of `AAFLEnergyPickup` / `UAFLOverlapCollectComponent`). BOTH modes (ruling ①): (ii) grant `ID_AFL_Heal` item [default]; (i) instant-heal med-station [`bInstantHeal` variant]. Replaces INFINEON's 5 broken `B_AbilitySpawner`. | **1** | conform (S) |
| **② Store SKU** | Buy N health packs; quantity tracked; decrement on use. **The ONE genuinely-new architecture** — see §3. PlayFab `RemainingUses` (ruling ②). | **2** | NEW (L) |
| **③ Loadout consumable** | Carry into match. **Grant** = System A (`UAFLAG_GrantLoadout`, generic QuickBar) — conform. **Pre-match CHOICE + UI + persist** = a new `EAFLLoadoutAxis::Consumable` on System B reading a gameplay-item source (not the cosmetic catalog), backed by §3's owned-count. | **3** | grant=conform (S); choice/UI=new (M) |

---

## 3. THE KEYSTONE — the counted-inventory subsystem (Phase 2, the ONE new architecture)

**The gap:** the AFL economy today has exactly two shapes — **currency** (`int32 Volts/Watts`) and **boolean
ownership** (`OwnedCosmeticIds`, a SET, idempotent — "you cannot own two of anything"). **"Player owns 3 health
packs" is unrepresentable.** No per-SKU count exists anywhere.

**What must be built (net-new, all routing through the ONE proven `IAFLCosmeticPersistence` seam — not a
parallel system):**
1. **Counted owned-store on the wallet** — a replicated per-SKU count (`TMap<FName,int32>` or a Lyra
   `FGameplayTagStackContainer` FastArray) alongside `OwnedCosmeticIds`, with `OnRep`.
2. **Persistence-seam pair** — `LoadOwnedCounts` / `SaveOwnedCounts` (or a `TMap<FName,int32>` field added to
   `FAFLEconomyRecord`). The seam is balance + boolean-set only today.
3. **Increment-on-buy / re-purchasable** — replace the idempotent `Add` + the "already owned → deny" guard
   (`AFLWalletComponent.cpp:322`) with a count increment by purchased quantity.
4. **Server decrement-on-use** — a server-authoritative `ConsumeItem(SKU, N)` that decrements + re-persists.
   (In-match, the item's `LyraAbilityCost_ItemTagStack` decrements the QuickBar instance's stack; the
   store-owned count is the cross-session ledger the loadout draws from at spawn.)
5. **PlayFab consumable modeling** (ruling ②) — seed the health SKU as a **consumable Inventory item**
   (`RemainingUses`), and STOP flattening the inventory parse (`AFLEconomyPersistenceSubsystem.cpp:174–182`
   currently reads only `ItemId`, discarding `RemainingUses`) — read + persist the per-item count.

**Reusable (not new):** the wallet component shape (replicated `PlayerStateComponent`), `FAFLCatalogEntry`
(add a `Consumable` type + a grant-quantity field), the `IAFLCosmeticPersistence` seam pattern, the anti-spoof
PlayFab purchase rail, and Lyra's `FGameplayTagStackContainer` for the in-match stack.

**Why this is NOT a "parallel inventory" (reconciles the master doc's principle):** it is a **count dimension
on the same catalog + entitlement + persistence seam** — the same `FAFLCatalogEntry`, the same wallet, the same
`IAFLCosmeticPersistence`. Boolean ownership becomes `count ≥ 1`; a consumable is `count decrements on use`. One
registry, one seam — a new *quantity* field, not a second inventory. (The master doc's "no parallel inventory"
was written for boolean cosmetics + the leasing arc; a counted consumable is an approved count-dimension
extension of the same spine.)

---

## 4. PHASE PLAN

- **Phase 1 — PLAYABLE (cheap, high value; no store, no persistence):** the shared spine (§1) + BOTH
  world-pickup variants + System-A grant (everyone spawns with one pack) + INFINEON placement (replace the 5
  broken `B_AbilitySpawner`) + the weapon-spawner repoint (ruling ⑤). Health works in-match end-to-end.
- **Phase 2 — THE STORE KEYSTONE (§3):** the counted-inventory subsystem (wallet count + seam pair +
  increment-buy + decrement-use + PlayFab `RemainingUses`). The health SKU becomes buyable in quantity.
- **Phase 3 — THE LOADOUT CHOICE:** `EAFLLoadoutAxis::Consumable` on System B + a gameplay-item data feed +
  the locker/market UI axis, reading Phase 2's owned-count so a player carries the packs they own.

---

## 5. PHASE 1 FILESET (C++ vs asset vs operator)

**C++ (needs a UBT build — AFLCombat):**
- `AAFLHealthPickup` — clone `AAFLEnergyPickup`; `UAFLOverlapCollectComponent` walk-over; server-only
  `HandleCollected`; `bInstantHeal` flag → (i) apply `UAFLGE_OverloadRestore` directly vs (ii) grant
  `ID_AFL_Heal` to inventory.
- `GA_AFL_Heal` — IF a lighter base than the BP template is wanted (recommended); else a BP.
- (maybe) a 2nd "Consumables" `TArray` on `UAFLAG_GrantLoadout` so the pack grants to a slot without displacing
  a weapon — small; only if the single generic array isn't used.

**Asset (bridge / editor authorable, no build):**
- `ID_AFL_Heal`, `WID_AFL_Heal`, `AbilitySet_AFL_Heal` (+ `GA_AFL_Heal` if BP).
- `AFL.Consumable.Charges` GameplayTag (ini) — editor restart to register.
- Art reskin: `SM_AFL_HealthPack` + `MI_AFL_HealthPack` + textures; reskinned feedback cue.
- `BP_AFL_HealthPickup` (tiered variants on `AAFLHealthPickup`, like `BP_AFL_EnergyPickup_S/M/L`).
- Add `ID_AFL_Heal` to `GA_AFL_GrantLoadout`'s grant list (everyone spawns with a pack).
- `WeaponPickupData_AFL_PulseCarbine` (`ULyraWeaponPickupDefinition`) wrapping `ID_BagMan_PulseCarbine` + mesh.

**Operator-in-editor:**
- Place `BP_AFL_HealthPickup` in INFINEON (replace the 5 broken `B_AbilitySpawner`), WP-guarded save.
- Repoint the 10 `B_WeaponSpawner` `WeaponDefinition` → `WeaponPickupData_AFL_PulseCarbine` (`EditInstanceOnly`).
- PIE-verify (HOST→INFINEON): pickup collect → carry → use → heal ≈50%; spawn-with-pack; rifle spawners give
  the PulseCarbine. Then commit.

**Build order:** tag+art → item spine (ID/WID/AbilitySet/GA) → `AAFLHealthPickup` C++ + BP tiers → System-A
grant edit → `WeaponPickupData_AFL_PulseCarbine` → **UBT build AFLCombat** → operator place/repoint/PIE → commit.
