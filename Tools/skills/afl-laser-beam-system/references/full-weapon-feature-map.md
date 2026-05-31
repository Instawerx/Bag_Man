# Full Laser-Weapon Feature Map (tutorial → Lyra-canonical)

The reference YouTube build (HUD option, input, weapon switch, child BP, mesh/material
swap, cooldown, camera shakes, Niagara beam, sparks/smoke, automatic vs charge fire,
charge timeline, cancel/cleanup) is a **raw-Blueprint** approach: a child weapon BP that
owns Timelines, casts, and manual HUD wiring. BAG MAN is Lyra/GAS under STEP-0 doctrine,
so each feature has a canonical home that already exists. **Configure, don't reinvent.**

This table is the contract. Each row says what the tutorial does, the Lyra equivalent,
and whether you already have it. Read top-to-bottom — it's the build order too.

| Tutorial chapter | Lyra-canonical equivalent | Status |
|---|---|---|
| 02:02 HUD option + input mapping | LyraQuickBar slot (auto-reflects equipped inventory) + `ULyraInputConfig` mapping `InputTag.Weapon.*` to InputActions in an IMC | **Mostly config** |
| 04:17 Weapon switching + Laser Rifle child BP | `LyraQuickBarComponent` cycle/set-slot (already input-wired) + `ULyraInventoryItemDefinition` → `InventoryFragment_EquippableItem` → `ULyraEquipmentDefinition ED_AFL_LaserRifle`. "Child BP" = `B_WeaponInstance_AFL_LaserRifle` (child of `ULyraRangedWeaponInstance`) | **New data, no new system** |
| 07:09 Mesh/material swap + cooldown var | Mesh = equipment actor mesh in the EquipmentDefinition; material variants = MID / skin system. Cooldown = **Cooldown GE** `GE_AFL_LaserRifle_Cooldown` granting `Cooldown.Weapon.LaserRifle`, set as the ability's `CooldownGameplayEffectClass` — NOT a float | **Pattern already used (dash cooldown)** |
| 08:30 Camera shakes (charge/fire/beam) | `UCameraShakeBase` assets `CS_AFL_Laser_{Charge,Fire,Beam}` triggered **from the GameplayCues**, not from ability code — so they predict + replicate for free | **New assets, cue-driven** |
| 10:36 Build the Niagara beam | Adopt `NS_AFL_Laser_*` from the imported library + the looping beam cue | **Done** (this skill) |
| 14:19 Sparks/smoke bound to Beam End | `GameplayCue.Weapon.Laser.Impact` spawns spark/smoke (`M_AFL_Spark`/`M_AFL_Smoke_8x8`) at `Params.Location`/`Normal`; optional end-cap sparks attached to the beam-end | **Mostly done** (impact cue) |
| 17:25 Automatic vs charge logic | Two GameplayAbilities: `UAFLAG_Laser_AutoFire` (≈ your Pulse) and `UAFLAG_Laser_Charge`. Fire-mode = which is active, toggled by `InputTag.Weapon.FireMode` + a `ULyraRangedWeaponInstance` property | **Auto ≈ have; Charge = new** |
| 21:09 Charging logic (VFX/audio/shake/timeline) | Charge ability + a charge **Curve** (the "timeline") + looping `GameplayCue.Weapon.Laser.Charge` reading `NormalizedMagnitude` to scale Niagara/MetaSound/shake | **New** |
| 33:15 Stop firing / cancel / cleanup | `EndAbility(bWasCancelled)` → `RemoveGameplayCue` (beam + charge) + cancel the fire timer/task + clear charge state. Looping cue `OnRemove` deactivates Niagara | **Discipline + partly done (cue OnRemove)** |

## What you already handle vs what's genuinely new

- **Already (carried-forward known-good C++ + this skill):** the hitscan trace
  (predict-and-send), damage ExecCalc, lag-comp, heat, the beam Niagara + beam/impact
  cues. Automatic fire is essentially Pulse on a repeating input.
- **New to reach full parity with the video:** the **charge fire mode**, the **charge
  cue** (charge-scaled VFX/audio/shake), the three **camera-shake assets**, the weapon's
  **Cooldown GE**, the **fire-mode toggle**, and the explicit **cancel/cleanup** path.

Everything new is "assemble Lyra pieces," not "invent a system."

## New GameplayCue tags (add to the AFL tags table)

```
GameplayCue.Weapon.Laser.Charge   # looping — charge VFX/audio/shake, scaled by charge level
GameplayCue.Weapon.Camera.Shake   # (optional) generic shake cue if not folding shake into the above
```
(Beam / BeamFlash / Impact / Muzzle already defined in integration-architecture.md.)

## The two fire-mode abilities

### Automatic — `UAFLAG_Laser_AutoFire`
Extend `ULyraGameplayAbility_RangedWeapon` (the canonical Lyra gun ability — it already
does the predicted trace and weapon-instance spread). NetExecutionPolicy LocalPredicted.
- On `InputPressed`: `CommitAbility` (consumes cooldown/cost), fire one shot, then start a
  repeating fire loop at the weapon's fire-rate via `UAbilityTask_WaitDelay` re-arm (or the
  weapon instance's interval).
- Each shot: the existing predict-and-send trace + damage GE, then
  `ExecuteGameplayCue(BeamFlash)` + `ExecuteGameplayCue(Impact)` with the predicted
  `Location`/`Normal`.
- On `InputReleased`: `EndAbility`. No looping cue (hitscan flashes are discrete).

### Charge — `UAFLAG_Laser_Charge`
Extend `ULyraGameplayAbility`. NetExecutionPolicy LocalPredicted.
```cpp
// charge state
UPROPERTY(EditDefaultsOnly, Category="AFL|Charge")
TObjectPtr<UCurveFloat> ChargeCurve;          // the "timeline": 0..1 over MaxChargeTime
UPROPERTY(EditDefaultsOnly) float MaxChargeTime = 1.5f;
UPROPERTY(EditDefaultsOnly) float MinChargeToFire = 0.2f;
UPROPERTY(EditDefaultsOnly) FScalableFloat BaseDamage; // scaled by charge at fire
```
Flow:
1. `InputPressed` → `CommitAbilityCooldown` deferred (charge first, commit on fire), add the
   **looping charge cue**:
   ```cpp
   FGameplayCueParameters Cue; Cue.SourceObject = WeaponInstance;
   ASC->AddGameplayCue(Tag_Charge, Cue);
   ```
   Start a charge timer/task tracking elapsed time.
2. Each frame/update → compute `Norm = Clamp(Elapsed/MaxChargeTime,0,1)`, push it to the
   cue so VFX/audio/shake scale:
   ```cpp
   FGameplayCueParameters P; P.NormalizedMagnitude = Norm; P.SourceObject = WeaponInstance;
   ASC->ExecuteGameplayCue(Tag_Charge_Tick, P);   // or update the looping cue's params
   ```
   (Simplest: the looping charge cue notify ticks and reads charge level off the weapon
   instance, mirroring the beam cue's tick pattern — keeps params off the wire.)
3. `InputReleased`:
   - if `Norm >= MinChargeToFire`: `CommitAbility` (cooldown now), fire a shot whose damage
     GE uses **SetByCaller** = `BaseDamage * ChargeCurve->Eval(Norm)`; trigger BeamFlash +
     Impact cues; `RemoveGameplayCue(Tag_Charge)`; `EndAbility`.
   - else: treat as cancel (below).

### Fire-mode toggle
Store `EAFLLaserFireMode {Automatic, Charge}` on `B_WeaponInstance_AFL_LaserRifle`.
`InputTag.Weapon.FireMode` triggers a tiny ability that flips it and (optionally) plays a
UI/audio tick. The active fire InputTag activates whichever ability matches the current
mode — granted together in the AbilitySet, gated by an `ActivationRequiredTags` /
`ActivationBlockedTags` pair (`State.Weapon.Mode.Auto` / `State.Weapon.Mode.Charge`).

## Camera shakes — cue-driven, never in ability code

Create `CS_AFL_Laser_Charge` (looping, ramps with charge), `CS_AFL_Laser_Fire` (one-shot
kick), `CS_AFL_Laser_Beam` (subtle loop while channeling). Play them **inside the cue
notifies** so they ride the predicted/replicated cosmetic channel:
- Charge cue `OnActive` → `StartCameraShake(CS_AFL_Laser_Charge)`, scale per charge level in
  Tick; `OnRemove` → `StopCameraShake`.
- BeamFlash/Fire cue `OnExecute` → one-shot `CS_AFL_Laser_Fire`.
Only play shake for the **locally-controlled** player (check `IsLocallyControlled` in the
cue) — never shake spectators' cameras.

## Cleanup discipline (the chapter most raw-BP builds get wrong)

A laser that doesn't clean up leaves a beam stuck on, a charge sound looping, or a shake
that never stops. In GAS this is automatic **if** you respect the contract:
- All teardown lives in `EndAbility` (both normal end and cancel): `RemoveGameplayCue` for
  every looping cue the ability added; cancel any repeating timer/AbilityTask; clear charge
  state.
- The looping cue's `OnRemove` is the single place Niagara/audio/shake actually stop — it
  already deactivates the beam NC (see integration-architecture.md). Mirror it for the
  charge cue.
- Interruptions (weapon switch, stun, death) cancel via `CancelAbilitiesWithTags` on the
  ability's `CancelAbilitiesWithTag` set — so switching away from the laser mid-charge tears
  the charge down with no extra code.

## AIK prompt sequence for the new features (run after the beam prompts in aik-briefing.md)

### Prompt 8 — charge + fire-mode tags
```
Add GameplayCue.Weapon.Laser.Charge (looping) and the state tags
State.Weapon.Mode.Auto / State.Weapon.Mode.Charge to the AFL plugin-local tags table and
NativeGameplayTags. Show the indexed count.
```

### Prompt 9 — the Laser Rifle weapon definition  (attach: an existing AFL ranged weapon for reference)
```
Create the canonical Lyra weapon for the AFL Laser Rifle:
- B_WeaponInstance_AFL_LaserRifle extending ULyraRangedWeaponInstance, with an
  EAFLLaserFireMode property (Automatic/Charge, default Automatic) and an
  IAFLLaserVisualProvider implementation backed by DA_AFL_LaserVisual_LaserRifle.
- ED_AFL_LaserRifle (ULyraEquipmentDefinition): equipment actor with the laser rifle mesh,
  muzzle socket "Muzzle", and AbilitySet DA_AFL_AbilitySet_LaserRifle.
- ID_AFL_LaserRifle (ULyraInventoryItemDefinition) with InventoryFragment_EquippableItem ->
  ED_AFL_LaserRifle, plus the QuickBar/HUD icon fragment.
Follow AFL naming; place under Plugins/GameFeatures/AFLWeapons/.
```

### Prompt 10 — cooldown GE
```
Create GE_AFL_LaserRifle_Cooldown: a native C++ parent (Duration) + BP child granting
Cooldown.Weapon.LaserRifle via TargetTagsGameplayEffectComponent, same shape as
GE_AFL_DashCooldown. Reference it as CooldownGameplayEffectClass on the fire abilities.
```

### Prompt 11 — auto-fire ability
```
Create UAFLAG_Laser_AutoFire extending ULyraGameplayAbility_RangedWeapon, LocalPredicted.
Reuse the existing predicted trace + damage path. On each shot also ExecuteGameplayCue
BeamFlash and Impact with the predicted Location/Normal and SourceObject = the weapon
instance. Loop at the weapon's fire rate while InputTag.Weapon.Fire is held; EndAbility on
release. ActivationRequiredTags: State.Weapon.Mode.Auto.
```

### Prompt 12 — charge ability
```
Create UAFLAG_Laser_Charge extending ULyraGameplayAbility, LocalPredicted, with
ChargeCurve (UCurveFloat), MaxChargeTime, MinChargeToFire, BaseDamage (FScalableFloat).
On press: AddGameplayCue Charge (SourceObject = weapon) and begin tracking charge time.
On release: if charge >= MinChargeToFire, CommitAbility, fire a shot whose damage GE uses
SetByCaller = BaseDamage * ChargeCurve.Eval(normChargeLevel), ExecuteGameplayCue BeamFlash +
Impact, RemoveGameplayCue Charge, EndAbility; else cancel. ActivationRequiredTags:
State.Weapon.Mode.Charge. Put ALL teardown in EndAbility for both end and cancel.
```

### Prompt 13 — charge cue + camera shakes
```
Create AAFLCueNotify_LaserCharge extending AGameplayCueNotify_Actor (looping): OnActive
spawn the charge/muzzle Niagara (NS_AFL_OrbLaser_Center_* from the weapon's visual
provider) attached to the muzzle and, if locally controlled, StartCameraShake
CS_AFL_Laser_Charge; Tick scales the Niagara charge param and shake scale by the weapon's
current charge level; OnRemove stops Niagara + shake. Also create CS_AFL_Laser_Fire (one-
shot, played by the BeamFlash cue) and CS_AFL_Laser_Beam (loop, played by the beam cue).
Only shake for the locally-controlled player.
```

### Prompt 14 — fire-mode toggle + HUD/input
```
Add InputTag.Weapon.FireMode to the InputConfig and a small UAFLAG_Laser_ToggleFireMode
ability that flips EAFLLaserFireMode on the weapon instance and swaps the active
State.Weapon.Mode.* tag. Confirm the Lyra QuickBar HUD slot shows the Laser Rifle when
equipped (no per-weapon HUD code — verify it reflects inventory) and that
InputTag.Weapon.Switch.Next/Prev cycle to it.
```

## PIE acceptance additions (extend the matrix in integration-architecture.md)

| # | Check | Pass |
|---|---|---|
| 10 | Weapon switch | Laser Rifle appears in the QuickBar HUD slot when picked up; cycling input equips it |
| 11 | Automatic fire | Hold fire → repeats at fire-rate; release → stops; cooldown GE gates over-fire |
| 12 | Charge fire | Hold → charge cue VFX/audio/shake ramp; release past min → stronger shot scaled by charge |
| 13 | Fire-mode toggle | FireMode input flips Auto↔Charge; the matching ability activates |
| 14 | Camera shake | Charge ramps shake, fire kicks, beam loops — only on the local player |
| 15 | Cancel/cleanup | Releasing below min, or switching weapons mid-charge, stops all VFX/audio/shake with nothing stuck on |
