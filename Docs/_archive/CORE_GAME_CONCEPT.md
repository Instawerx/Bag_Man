<!-- ═══════════════════════════════════════════════════════════════════════════
     PROVENANCE HEADER — added on archival, 2026-08-05, per operator ruling R2.
     This is the ONLY edit made to this file. Nothing below this header has been
     altered, reordered or removed.
     ═══════════════════════════════════════════════════════════════════════════ -->

> ## ⚠ ARCHIVED — UNRATIFIED TRANSCRIPT. NOT A SOURCE OF TRUTH.
>
> **What this is.** A raw, lightly-formatted transcript of a ChatGPT conversation exploring the game
> concept. It was never reviewed, ratified or adopted as a specification. It reads authoritative because
> of its length and confident voice — it is not.
>
> **Origin:** `https://chatgpt.com/share/69fd87d1-d1d8-83e8-8d34-b4c6037999bc` (recorded at line 2889 of
> this file, in the original tail).
>
> **Why it was archived — operator ruling R2 (2026-08-05).** Its status as an unratified transcript made
> it dangerous in place: agents and readers repeatedly treated it as a design authority, and several of
> its claims were contradicted by disk. It is retained rather than deleted because it is the origin
> record of a number of ideas that *did* survive.
>
> **What was salvaged, and where it went.** Its genuinely load-bearing content was reconciled into
> **`Docs/DOCTRINE.md`** — the Tier 1 laws. Doctrine sourced from this file is cited there as
> `CORE_GAME_CONCEPT.md` and is marked either *converged* (it agreed with a second, ratified source) or
> *rescued — unique* (it existed only here and was judged worth keeping). **DOCTRINE Appendix B records
> this file's provenance; Appendix A records the claims from it that disk contradicted and which were
> therefore excluded.**
>
> **How to use this file.** As a historical record only. If something here is not reflected in
> `Docs/DOCTRINE.md` or one of the eight Tier 2 SSOTs under `Docs/ssot/`, it was not adopted — treat it
> as an idea that was considered, not a decision that was made. Do not cite this file as authority, and
> do not re-adopt from it without an operator ruling.

---

⚡ CORE GAME CONCEPT (Refined)

“Neon Cyber Laser Arena: Extraction Combat”

Think:

Fast arena shooter + laser tag clarity
With light extraction stakes (not full Tarkov stress)
Built for short, repeatable sessions (8–12 min)
🔁 THE CORE LOOP (Player Perspective)
1. Queue → Drop In (Instant Gratification)
10–20 players spawn into a neon arena
Choose:
Loadout (laser types)
Ability kit (dash, blink, shield)

👉 No long setup. Get into action fast.

2. Hunt, Tag, Charge (Moment-to-Moment Fun)

Core actions:

Tag enemies with laser weapons
Build Energy Charge
Chain kills = Overdrive Mode

Twist:

Players don’t “die” traditionally
They overload and drop energy cores

👉 This keeps it arcade, not punishing

3. Collect & Risk (The Hook)
Eliminated players drop:
Energy cores
Temporary buffs
Map has:
High-risk energy zones
Rotating objectives

Now the decision:

Keep fighting for more… or secure what you have?

4. Extract or Cash Out (Tension Layer)
Extraction zones open periodically
Players must:
Survive
Reach zone
Hold position briefly

Fail = lose carried energy
Escape = convert energy → rewards

👉 This is your addiction engine

5. Meta Progression (Retention Engine)

Energy converts into:

🔹 Permanent Progression
New laser types (beam, burst, ricochet)
Abilities (teleport, clone decoy)
Robot shells (pure cosmetic or slight playstyle variance)
🔹 Session Boosters
Temporary buffs for next match
Risk/reward stacking
6. Return Loop (Why They Come Back)
“I almost extracted with 5x energy… one more run”
“I unlocked a new laser, want to try it”
“I can play smarter this time”

👉 That’s your loop closing

💰 MONETIZATION LOOP (Non-Toxic but Powerful)

If you mess this up, players leave. So:

What you SELL:
Skins (neon trails, laser effects, robot bodies)
Emotes / victory animations
Battle pass (seasonal progression)
Arena themes
What you DO NOT sell:
Power that breaks PvP fairness
🧠 ADDICTION LAYERS (This is the real sauce)
1. Near-Miss Psychology
Extraction almost complete → interrupted
Creates emotional spikes
2. Streak Multipliers
Kill streak = exponential rewards
Players chase “one more fight”
3. Visible Progress Mid-Match
Energy meter always climbing
Visual overload effects
4. Short Match Length
“Just one more game” works only if:
Matches are <12 minutes
⚙️ GAME MODES (Launch Strategy)

Start with ONE:

🟢 “Core Extraction Arena” (your flagship)

Later expand:

Team vs Team laser domination
Ranked competitive mode
Limited-time chaos modes
🎮 YOUR UNIQUE EDGE (Don’t skip this)

Your differentiator should be:

⚡ Laser Identity System

Each weapon:

Behaves visually different
Feels skill-based
Creates highlight moments

Examples:

Reflective beam (bank shots)
Continuous melt laser
Charge sniper beam
Chain lightning laser
🏗️ WHAT THIS MEANS FOR YOUR BUILD

When you go back into Unreal:

Focus ONLY on:

Tight movement
Laser hit feedback (SUPER important)
Energy drop + pickup loop
Extraction zone mechanic

Ignore everything else early.

🚨 Hard Truth

If the moment-to-moment laser combat isn’t insanely satisfying, nothing else matters.

Players won’t stick around for progression if:

Shooting feels weak
Hits aren’t clear
Movement isn’t fluid

🧠 FIRST PRINCIPLE (Don’t break Lyra’s spine)

Lyra is built around:

Modular gameplay features
Ability System (GAS)
Clean multiplayer replication

👉 Your rule:

Extend via new components + abilities — avoid editing core classes directly

🧩 WHAT TO KEEP (Do NOT touch early)

These systems are battle-tested. Keep them intact:

✅ Core Framework
LyraGameMode
LyraGameState
LyraPlayerState
LyraController
✅ Multiplayer Systems
Session handling
Replication logic
Team system (you may repurpose later)
✅ GAS (Gameplay Ability System)
Ability activation flow
Attribute system
Gameplay effects pipeline

👉 Translation:
You are NOT building multiplayer from scratch. This is your engine.

🔧 WHAT TO OVERRIDE / EXTEND (Your actual work)
1. 🎮 Pawn / Character Layer
Start from:
LyraCharacter
Create:
BP_NeonRobotCharacter
Modify:
Movement feel:
Increase acceleration
Add air control
Lower friction for glide feel
Add:
Dash / Blink ability hooks (via GAS)
Energy charge component (custom)
2. 🔫 Weapon System → Convert to LASER SYSTEM

Lyra uses modular weapons—perfect for you.

Keep:
Weapon base classes
Equipment system
Replace:
Projectile weapons → Hitscan / Beam logic
Create new weapon classes:
🔹 GA_Laser_Primary_Fire
Instant hit (line trace)
High visual feedback
🔹 GA_Laser_Beam_Continuous
Sustained damage
Heat/overload mechanic
🔹 GA_Laser_Ricochet
Multi-bounce logic
Skill-based weapon
Key Change:

Replace:

Bullet spawn logic

With:

Line trace + Niagara beam FX
3. ⚡ ENERGY SYSTEM (Your Core Loop)

This does NOT exist in Lyra—you build it.

Create:
Component_EnergyCollector
Tracks:
Current carried energy
Max capacity
Overdrive threshold
Hook into:
Enemy elimination events

When player “dies”:

Drop:
Energy pickups (spawn actors)
Create:
BP_EnergyPickup

Features:

Auto-attract (magnetic feel)
Glow intensity based on value
4. 💥 “DEATH” → OVERLOAD SYSTEM

Replace traditional death.

Override:
Elimination handling in:
LyraHealthComponent
New behavior:
Player:
Overloads
Drops energy
Respawns quickly

👉 This keeps matches fast + arcade-like

5. 🚨 EXTRACTION SYSTEM (Core Differentiator)

Lyra doesn’t have this—you add it cleanly.

Create:
BP_ExtractionZone
States:
Inactive
Active
Contested
Logic:
Player enters zone
Starts extraction timer
Takes damage / leaves → cancel
Reward:
Convert carried energy → meta currency
Hook into:
GameState (for match flow timing)
6. 🎯 GAME MODE (Your Custom Mode)
Extend:
LyraGameMode
Create:
GM_ExtractionArena
Responsibilities:
Spawn players
Control extraction timing
Track match phases:
Warmup
Active
Extraction windows
End match
7. 🎨 VISUAL IDENTITY (Critical for your game)

This is where you stand out.

Replace:
Default muzzle flashes
Impact effects
With:
Neon beams (Niagara)
Glow-heavy materials (emissive)
Create:
M_Neon_Master
NS_Laser_Beam
NS_Energy_Explosion

👉 Rule:

Every shot should feel like a light show

8. 🧠 UI (Feedback Loop = Retention)

Lyra UI is modular—reuse it.

Extend:
HUD widgets
Add:
Energy meter (center focus)
Extraction timer
Overdrive indicator

👉 Feedback must be:

Immediate
Loud
Satisfying
🏗️ PROJECT STRUCTURE (Clean + Scalable)

/Game
  /NeonGame
    /Characters
    /Weapons
    /Abilities
    /EnergySystem
    /Extraction
    /UI
    /VFX




👉 Never dump into Lyra folders—keep your layer separate.

⚙️ CLAUDE PROMPT STRATEGY (Important for your workflow)

When generating code:

Ask for:

“Extend Lyra class without modifying base”
“Use GAS ability pattern”
“Ensure multiplayer replication compatibility”
🚀 BUILD ORDER (Don’t get lost)
Week 1:
Character movement tuned
Basic laser (hitscan)
Week 2:
Energy drop + pickup loop
Week 3:
Extraction zone working
Week 4:
VFX + juice + polish
⚠️ COMMON MISTAKES (Avoid these)

❌ Editing Lyra core files directly
❌ Building custom networking systems
❌ Overcomplicating weapons early
❌ Ignoring visual feedback

🎯 REALITY CHECK

If you execute this correctly, you’ll have:

A multiplayer-ready prototype in ~30 days
A loop that is:
Fast
Addictive
Expandable

If you don’t:

You’ll get stuck in engine-level problems for months

Alright—this is where your game either feels like a AAA system… or turns into spaghetti.

What you want is a clean, scalable GAS (Gameplay Ability System) architecture that supports:

Laser combat
Energy economy
Movement abilities
Extraction mechanics
Multiplayer replication

Below is a full architecture diagram + how everything connects in Lyra.

🧠 HIGH-LEVEL GAS ARCHITECTURE
[ Player Controller ]
        ↓
[ Player State ]  ← (Owns Ability System Component - ASC)
        ↓
[ Ability System Component (ASC) ]
        ↓
 ┌───────────────────────────────────────┐
 |         CORE GAS LAYERS               |
 ├───────────────────────────────────────┤
 | Abilities (GA_)                       |
 | Attributes (Attribute Sets)           |
 | Gameplay Effects (GE_)                |
 | Gameplay Tags                         |
 └───────────────────────────────────────┘
        ↓
[ Character / Pawn (Avatar Actor) ]
        ↓
[ Components (Energy, Weapons, etc.) ]
🔑 CRITICAL DESIGN DECISION (Lyra Way)

👉 ASC lives on PlayerState, NOT Character

Why:

Keeps abilities persistent across respawns
Multiplayer-safe
Matches Lyra architecture
🧩 YOUR CUSTOM GAS LAYERS

Now we build your game-specific systems.

1. 📊 ATTRIBUTE SETS (The backbone)

Create multiple—not one giant file.

🔹 AS_Combat
Health
Shield
DamageMultiplier
HeatLevel
🔹 AS_Energy
CarriedEnergy
MaxEnergy
OverdriveThreshold
ExtractionMultiplier
🔹 AS_Mobility
MoveSpeed
DashCooldown
BlinkCharges
AirControl

👉 These are replicated automatically via GAS.

⚡ 2. GAMEPLAY TAG SYSTEM (Your logic brain)

Use tags instead of booleans.

Core Tags:
State.Overdrive
State.Extracting
State.Overheated

Ability.Laser.Primary
Ability.Laser.Beam
Ability.Movement.Dash
Ability.Movement.Blink

Event.Hit
Event.Elimination
Event.EnergyPickup

👉 Tags drive:

Ability activation
Buffs/debuffs
UI state
🔫 3. ABILITY LAYER (GA_ Classes)

This is where gameplay lives.

🔥 COMBAT ABILITIES
GA_Laser_Primary
Hitscan trace
Applies damage effect
Generates energy on hit
GA_Laser_Beam
Channeling ability
Applies continuous damage
Builds heat
GA_Laser_Ricochet
Multi-trace logic
Skill-based weapon
⚡ ENERGY ABILITIES
GA_Energy_Pickup
Triggered on overlap
Applies energy gain effect
GA_Overdrive
Activates when threshold reached
Buffs:
Damage
Speed
Visual FX
🌀 MOVEMENT ABILITIES
GA_Dash
Impulse movement
Cooldown-based
GA_Blink
Short teleport
Charge system
🚨 OBJECTIVE ABILITIES
GA_Extract
Channel ability
Interrupted by damage
Converts energy → rewards
💥 4. GAMEPLAY EFFECTS (GE_)

These do the actual math.

🔹 Damage

GE_Damage

-Health
+Heat
🔹 Energy Gain

GE_EnergyGain

+CarriedEnergy
🔹 Overdrive Buff

GE_OverdriveBuff

+DamageMultiplier
+MoveSpeed
🔹 Extraction Convert

GE_ExtractionReward

-CarriedEnergy
+MetaCurrency (off-GAS system)

👉 Effects are:

Instant
Duration-based
Stacking
🔁 5. EVENT FLOW (Moment-to-Moment Gameplay)
🔫 Shooting Flow
Input → GA_Laser_Primary
      → Line Trace Hit
      → Apply GE_Damage
      → Trigger Event.Hit
      → Apply GE_EnergyGain (on success)
💀 Elimination Flow
Health <= 0
→ Event.Elimination
→ Drop Energy Actor
→ Respawn Player
⚡ Energy Loop
Pickup → GA_Energy_Pickup
       → Apply GE_EnergyGain
       → Check Overdrive Threshold
       → Activate GA_Overdrive
🚨 Extraction Flow
Enter Zone
→ Activate GA_Extract
→ Channel Time
→ Apply GE_ExtractionReward
→ Reset Energy
🧬 6. ABILITY GRANT SYSTEM (Loadouts)

Abilities are not hardcoded—they’re granted dynamically.

Use:
Ability Sets (Lyra feature)
Example Loadout:

Starter Kit

GA_Laser_Primary
GA_Dash

Advanced Kit

GA_Laser_Beam
GA_Blink
GA_Overdrive

👉 This enables:

Loadouts
Progression
Monetization hooks
🧠 7. COOLDOWNS & COSTS

Handled via:

Gameplay Effects

Example:

GE_DashCooldown

Cooldown: 3s
Tag: Cooldown.Dash

👉 Ability checks:

If tag exists → cannot activate

🎯 8. AI / BOT COMPATIBILITY (Future-Proofing)

Same system works for bots:

They use same abilities
Just different input triggers
🏗️ FINAL ARCHITECTURE DIAGRAM
        [ Input ]
           ↓
   [ Gameplay Ability ]
           ↓
   [ Gameplay Tags Check ]
           ↓
   [ Gameplay Effect Applied ]
           ↓
   [ Attribute Modified ]
           ↓
   [ Events Triggered ]
           ↓
   [ UI + VFX Feedback ]
⚠️ CRITICAL RULES (Don’t break these)
❌ Don’t:
Put gameplay logic in Character BP
Hardcode stats outside GAS
Use Tick for ability logic
✅ Do:
Use Abilities for actions
Use Effects for math
Use Tags for logic
🚀 WHAT YOU NOW HAVE

If you follow this:

Fully modular ability system
Multiplayer-safe combat
Expandable weapons + abilities
Clean separation of systems

We’re going to design a laser weapon system that is:

Instantly readable (laser tag clarity)
Skill-based (high ceiling)
Spectacle-heavy (neon identity)
GAS-native (clean, multiplayer-safe)
🧠 WEAPON SYSTEM ARCHITECTURE (How it plugs into GAS)
[ Input ]
   ↓
GA_Laser_* (Ability)
   ↓
Targeting Logic (Trace / Beam / Bounce)
   ↓
Apply GE_Damage + GE_EnergyGain
   ↓
Trigger Gameplay Events (Hit / Kill)
   ↓
Spawn VFX (Niagara) + SFX + Camera Feedback
🔫 CORE DESIGN RULES (Don’t break these)
1. Every weapon must have:
Clear visual identity
Distinct skill expression
Different risk/reward profile
2. No “generic rifle reskins”

If two weapons feel similar → delete one.

⚡ YOUR 5 SIGNATURE LASER WEAPONS

These are your launch lineup. Each fills a role.

🔴 1. PULSE CARBINE (Baseline weapon)
Feel:

Fast, responsive, reliable

GAS Ability:

GA_Laser_Pulse

Mechanics:
Hitscan
Medium fire rate
Small recoil spread
Effects:
GE_Damage_Light
GE_EnergyGain_Small
Skill Expression:
Tracking aim
Headshot multiplier (optional)
Why it exists:

Your “default comfort weapon”
→ Keeps casual players engaged

🔵 2. PRISM BEAM (Tracking melt weapon)
Feel:

Continuous laser beam

GAS Ability:

GA_Laser_Beam

Mechanics:
Channeling ability
Damage ramps over time
Builds Heat
Attributes used:
HeatLevel
Effects:
GE_Damage_OverTime
GE_HeatGain
Risk:
Overheat → forced cooldown
Skill Expression:
Target tracking
Positioning

👉 This weapon is your visual showstopper

🟣 3. RICOCHET LANCER (Skill cannon)
Feel:

Bank shots, trick kills

GAS Ability:

GA_Laser_Ricochet

Mechanics:
Line trace → reflect vector
Max 3–5 bounces
Damage increases per bounce
Effects:
GE_Damage_Scaling
Skill Expression:
Geometry awareness
Prediction

👉 This creates viral clips

🟡 4. NOVA BURST (Close-range destroyer)
Feel:

Shotgun explosion of light

GAS Ability:

GA_Laser_Nova

Mechanics:
Cone trace
High burst damage
Very short range
Effects:
GE_Damage_High
GE_Knockback (optional)
Risk:
Must be close → high danger
Skill Expression:
Movement timing
Ambush play
⚪ 5. SINGULARITY CANNON (High-risk, high-reward)
Feel:

Charge → release devastating beam

GAS Ability:

GA_Laser_Charge

Mechanics:
Hold to charge
Release → massive beam
Can penetrate targets
Effects:
GE_Damage_Massive
GE_EnergyGain_Large
Risk:
Immobile while charging
Highly visible
Skill Expression:
Timing
Prediction
Positioning

👉 This is your “highlight weapon”

⚙️ SHARED WEAPON SYSTEM COMPONENTS
🔋 1. HEAT SYSTEM (Prevents spam)

Each weapon can:

Generate heat
Overheat → disable firing
Attribute:
HeatLevel
Gameplay Tag:
State.Overheated
⚡ 2. ENERGY GENERATION (Feeds your core loop)

Every successful hit:

Grants energy
Scaling:
Headshots → bonus
Multi-hit → bonus
Risky weapons → higher gain
🎯 3. HIT CONFIRMATION (CRITICAL)

If this feels weak → game dies.

On Hit:
Bright flash at impact
Sound cue (sharp, satisfying)
Crosshair feedback
Damage number (optional)
🌈 4. VISUAL SYSTEM (Niagara)

Each weapon must have:

Unique beam style
Unique color identity
Impact effect
Example:
Pulse → short streaks
Beam → continuous ribbon
Ricochet → angular reflections
🧠 ADVANCED: WEAPON MODIFIERS (Future monetization + depth)

Add later:

Examples:
Split beam
Chain lightning
Lifesteal laser
Energy magnet effect

👉 Implement as:

Additional Gameplay Effects
Or Ability variants
🔁 COMBAT LOOP (How weapons feed addiction)
Shoot → Hit → Gain Energy
→ Build Overdrive
→ Increased Damage
→ More Eliminations
→ More Energy
→ Extract or Risk More
⚠️ BALANCING RULES (Save yourself later)
Rule 1:

High skill = high reward

Rule 2:

High power = high risk

Rule 3:

No weapon dominates all ranges

🚀 IMPLEMENTATION ORDER (Don’t overbuild)
Step 1:

Implement:

Pulse Carbine
Basic hit feedback
Step 2:

Add:

Beam weapon + heat
Step 3:

Add:

Energy gain system
Step 4:

Add:

One “fun weapon” (Ricochet or Nova)
Step 5:

Polish VFX until it feels AAA

🎯 REALITY CHECK

Players will forgive:

Bugs
Missing content

They will NOT forgive:

Weak weapon feel
Poor hit feedback
Confusing visuals

We’ll implement the Pulse Carbine first because it establishes:

Input flow
GAS firing pattern
Replication
Hit detection
Damage pipeline
Niagara beam FX
Hit feedback
Energy gain loop

Once this works, every other laser weapon becomes a variation.

This implementation is designed specifically for:

Lyra Starter Game
Unreal 5 GAS
Multiplayer replication
C++ + Blueprint hybrid workflow
🎯 FINAL RESULT

When finished:

LMB Click
→ GAS Ability Activates
→ Server validates shot
→ Hitscan trace
→ Damage applied
→ Energy awarded
→ Neon beam spawned
→ Hit FX + sound
→ Cooldown + heat updated
🧱 SYSTEMS WE’LL BUILD
GA_Laser_Pulse
├── Weapon Instance
├── Line Trace Logic
├── Damage Gameplay Effect
├── Energy Gain Effect
├── Niagara Beam FX
├── Hit Confirmation
└── Replication
📁 FILE STRUCTURE
/Game/NeonGame/
    Weapons/
        GA_Laser_Pulse
        GE_Damage_Pulse
        GE_EnergyGain_Small
        BP_LaserWeaponBase

    VFX/
        NS_PulseBeam
        NS_HitSpark

    Audio/
        SFX_PulseShot
        SFX_HitConfirm
⚡ STEP 1 — CREATE ATTRIBUTE SET
AS_Combat

Add:

UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health)
FGameplayAttributeData Health;

UPROPERTY(BlueprintReadOnly)
FGameplayAttributeData Shield;
AS_Energy
UPROPERTY(BlueprintReadOnly)
FGameplayAttributeData CarriedEnergy;
🔫 STEP 2 — CREATE DAMAGE EFFECT
GE_Damage_Pulse

Type:

Instant Gameplay Effect

Modifiers:

Health = -12

Optional:

Shield interaction later
⚡ STEP 3 — CREATE ENERGY GAIN EFFECT
GE_EnergyGain_Small

Modifiers:

CarriedEnergy = +5
🔥 STEP 4 — CREATE ABILITY
GA_Laser_Pulse

Parent:

LyraGameplayAbility_RangedWeapon

This is important:
Lyra already solves:

Prediction
Replication
Target data

You inherit the good stuff.

🧠 ABILITY FLOW
ActivateAbility()
    ↓
Get Target Data
    ↓
Server Trace Validation
    ↓
Apply Damage Effect
    ↓
Apply Energy Gain
    ↓
Spawn Beam FX
    ↓
End Ability
⚙️ STEP 5 — INPUT BINDING

In your Input Mapping Context:

Input.Fire

Bind to:

GA_Laser_Pulse

Use:

Ability Input Tag

Example:

InputTag.Weapon.Fire
💥 STEP 6 — HITSCAN TRACE

Inside ability:

Trace Setup
FVector Start = CameraLocation;
FVector End = Start + (ForwardVector * 10000.f);
Perform Trace
FHitResult Hit;

GetWorld()->LineTraceSingleByChannel(
    Hit,
    Start,
    End,
    ECC_Visibility,
    QueryParams
);
🎯 STEP 7 — APPLY DAMAGE

If hit actor has ASC:

UAbilitySystemBlueprintLibrary::ApplyGameplayEffectToTarget(
    SourceASC,
    TargetASC,
    DamageEffect,
    1.f
);
⚡ STEP 8 — ENERGY REWARD

On successful hit:

ApplyGameplayEffectToSelf(
    EnergyGainEffect
);
🌈 STEP 9 — NEON BEAM FX (Niagara)

Create:

NS_PulseBeam
Niagara Parameters
BeamStart
BeamEnd
BeamColor
Spawn FX
UNiagaraFunctionLibrary::SpawnSystemAtLocation(
    GetWorld(),
    BeamFX,
    Start
);

Set vector params:

BeamStart = Start
BeamEnd = HitLocation
💥 STEP 10 — HIT IMPACT FX

If hit:

Spawn:

NS_HitSpark

At:

Hit.ImpactPoint
🔊 STEP 11 — AUDIO FEEDBACK

On fire:

SFX_PulseShot

On confirmed hit:

SFX_HitConfirm
📡 STEP 12 — MULTIPLAYER REPLICATION

Critical rule:

Client:
Predicts shot instantly
Server:
Validates trace
Applies actual damage

Use:

LocalPredicted

Ability Net Execution Policy:

Local Predicted
🧠 WHY THIS MATTERS

Without prediction:

Weapons feel laggy online

With prediction:

Feels instant
Server still authoritative

Lyra already supports this elegantly.

🎮 STEP 13 — HIT FEEDBACK (MOST IMPORTANT FEEL LAYER)

Add:

Tiny crosshair expansion
Screen shake
Hit marker flash

Even subtle feedback massively improves satisfaction.

⚡ STEP 14 — HEAT SYSTEM (LIGHT VERSION)

Add to weapon:

CurrentHeat
MaxHeat

Each shot:

+5 Heat

Decay over time:

-10/sec

If overheated:

GameplayTag:
State.Overheated

Prevent firing.

🏗️ BLUEPRINT EVENT FLOW
Input Fire
↓
Activate GA_Laser_Pulse
↓
Line Trace
↓
Hit?
 ├── YES:
 │     Apply Damage
 │     Gain Energy
 │     Spawn Hit FX
 │     Play Hit Sound
 │
 └── NO:
       Spawn Beam Only

↓
Apply Heat
↓
Cooldown
↓
End Ability
🚨 COMMON FAILURE POINTS
❌ Don’t:
Spawn projectiles for this weapon
Put damage logic in Character BP
Trust client hit detection fully
✅ Do:
Use GAS Effects for all combat math
Keep server authoritative
Use Niagara for visual beams only
🎨 VISUAL STYLE GUIDE (Important)

Your beam should:

Be THICK
Have bloom/emissive glow
Slightly taper
Pulse brightness near impact

Think:

TRON + laser tag + anime energy weapon

NOT:

military rifle tracer
🧪 FIRST PLAYTEST GOAL

You are NOT testing balance yet.

You are testing:

Does firing feel satisfying?
Is hit feedback addictive?
Does movement + shooting feel fluid?

That’s all.

sequence builds the real game in the correct order:


Prism Beam → establishes visual identity


Dash Ability → establishes movement skill ceiling


Energy Pickup Loop → establishes addiction/progression loop


By the end of these 3 systems, you’ll have:


Combat


Mobility


Reward cycle


That’s your first vertical slice.

🔵 PART 1 — PRISM BEAM (Continuous Laser Weapon)
This weapon changes your game from:

“shooter prototype”

into:

“cyber neon laser arena”


🧠 DESIGN GOALS
The beam should feel:


Dangerous


Continuous


Melting


High-pressure


Player fantasy:
"I am surgically burning through enemies with concentrated energy."

⚙️ CORE MECHANICS
Hold Fire↓Continuous beam active↓Damage ticks every X ms↓Heat builds↓Overheat disables beam temporarily

🧱 SYSTEMS
GA_Laser_Beam├── Channeling Ability├── Tick Damage├── Heat Generation├── Niagara Continuous Beam├── Beam Lock Validation└── Overheat State

📁 FILES
Weapons/    GA_Laser_Beam    GE_Damage_Beam    GE_HeatGain    GE_OverheatedVFX/    NS_PrismBeam

🔥 GAS SETUP
Ability Type
Parent:
LyraGameplayAbility
Activation:
While Input Held
Net Policy:
Local Predicted

💥 DAMAGE MODEL
Instead of one-shot hit:


Tick damage every:


0.1 sec

Damage Tick
12 DPS
Per tick:
1.2 damage

🌈 BEAM TRACE
Every tick:
LineTraceSingleByChannel()

🎯 TARGET LOCK FEEL
Add:


Slight aim assist magnetism


Beam stickiness


Dynamic beam bend (tiny)


This makes the beam feel:
advanced and alive

🌡️ HEAT SYSTEM
Every tick:
+2 Heat
Decay:
-5/sec when inactive

Overheat Threshold
100 Heat
If exceeded:


Apply:


State.Overheated
Disable firing:
2.5 sec

🌈 NIAGARA BEAM
NS_PrismBeam
Parameters:
BeamStartBeamEndBeamWidthBeamColor

✨ VISUAL BEHAVIOR
The beam should:


Pulse


Distort slightly


Flicker under heat


Intensify on target hit



🔊 AUDIO
Layered audio:


Base hum


Active burn loop


Heat warning escalation


At high heat:
pitch rises

📡 MULTIPLAYER
Client:


Predicts beam instantly


Server:


Validates trace


Applies damage



🎮 GAME FEEL
Add:


Tiny camera pull


Light controller vibration


Beam screen bloom



🧪 SUCCESS TEST
If players:


continue firing too long,


overheat accidentally,


panic during cooldown…


you nailed the risk/reward.

⚡ PART 2 — DASH ABILITY
Movement is your skill ceiling.

🧠 DESIGN GOALS
Dash should feel:


Instant


Slick


Aggressive


NOT:
slow RPG dodge roll
Think:
cyberpunk arena skating

⚙️ DASH FLOW
Input Press↓Impulse movement↓Brief invulnerability (optional)↓Cooldown

🧱 FILES
Abilities/    GA_Dash    GE_DashCooldown

🎮 DASH PARAMETERS
Distance: 800Duration: 0.12 secCooldown: 3 sec

⚡ IMPLEMENTATION
Use:
LaunchCharacter()
Direction:
Movement Input Vector
Fallback:
Forward Vector

🌈 VISUALS
Spawn:
NS_DashTrail
Add:


Motion blur burst


Chromatic aberration pulse



🧠 ADVANCED FEEL
At dash start:


Temporarily reduce friction


This creates:
glide momentum

🛡️ OPTIONAL I-FRAMES
Very short:
0.1 sec
Enough to:


dodge beams


feel skillful


NOT enough to abuse.

🔊 AUDIO
Use:


Sharp energy crack


Doppler-style movement sound



📡 REPLICATION
Server authoritative movement:


client predicts


server validates


Lyra already helps here.

🧪 SUCCESS TEST
Dash should enable:


aggressive pushes


beam dodges


Nova shotgun escapes



⚡ PART 3 — ENERGY PICKUP LOOP
This is your retention engine.
Without this:


kills feel flat


extraction lacks tension



🧠 DESIGN GOALS
Energy pickups should feel:


Valuable


Magnetic


Dangerous to chase



⚙️ FLOW
Enemy overloads↓Drops energy cores↓Players rush pickup↓Gain carried energy↓Risk extraction

🧱 FILES
Energy/    BP_EnergyPickup    Component_EnergyCollector    GE_EnergyGain

💠 ENERGY PICKUP ACTOR
Components
Sphere CollisionNiagara GlowRotating MeshAudio Loop

🌈 VISUAL STYLE
Think:
floating neon plasma crystal
NOT:
ammo box

🧲 MAGNETIC COLLECTION
Within radius:
500 units
Pickup lerps toward player:
VInterpTo()
This feels MUCH better than:
walk directly over item

⚡ VALUE TIERS
Small
+10 EnergyBlue
Medium
+25 EnergyPurple
Large
+50 EnergyGold

🔥 DROP LOGIC
On overload:
Drop % of carried energy
Example:
70%

🎮 RISK DESIGN
Large energy piles should:


attract fights


create ambushes


pull players into danger



🔊 FEEDBACK
On pickup:


rising synth sound


UI pulse


energy counter animation



⚡ OVERDRIVE INTEGRATION
When:
Energy >= Threshold
Activate:
State.Overdrive
Buff:


move speed


damage


visuals



🌈 OVERDRIVE VISUALS
Player:


glowing circuitry


brighter emissive


beam amplification



🔁 YOUR FULL GAME LOOP NOW
Fight↓Beam enemies↓Dash aggressively↓Collect energy↓Enter Overdrive↓Extract or continue risking
That’s your game.

🚀 NEXT AFTER THIS
Once these 3 systems work:
PRIORITY ORDER
1.
Extraction Zone
2.
Overdrive Mode
3.
Ricochet Weapon
4.
Map Objective Events
5.
Ranked Match Flow

🎯 MOST IMPORTANT THING NOW
DO NOT:


add more weapons yet


build menus


make cosmetics


First:

make these 3 systems FEEL incredible.

That’s the foundation of everything.

🚀 NEXT AFTER THIS

Once these 3 systems work:

PRIORITY ORDER
1.

Extraction Zone

2.

Overdrive Mode

3.

Ricochet Weapon

4.

Map Objective Events

5.

Ranked Match Flow

🧠 THE 10 CORE AAA PILLARS

These are the non-negotiable foundations.

1. 🎮 GAME FEEL FIRST (Highest Priority)

Everything dies if:

movement feels floaty
hit feedback feels weak
weapons lack personality

AAA shooters succeed because:

every input feels rewarding
Your priorities:
Movement:
dash momentum
beam tracking
slide friction tuning
acceleration curves
Feedback:
hit markers
screen shake
reactive audio
bloom pulses
enemy dismemberment
2. 🤖 ROBOT DISMEMBERMENT SYSTEM (This is GOLD)

This is one of your strongest unique identity opportunities.

You should LEAN HARD into this.

🧠 Why it works psychologically

Players LOVE:

visible damage
physical comedy
reactive destruction

Especially:

rolling robot heads

That’s memorable.
That creates clips.
That creates social sharing.

⚙️ SYSTEM DESIGN
Damage Zones
Head
Left Arm
Right Arm
Left Leg
Right Leg
Torso
💥 DISMEMBERMENT RULES
Headshots
Pop head off
Head becomes physics object
Robot survives briefly headless
Arm Removal

Lose:

weapon stability
recoil control
Leg Removal
movement impairment
crawling/gliding mode
😂 FUN FACTOR

Your game should NOT feel gore-heavy.

It should feel:

chaotic robotic sports violence

Think:

sparks
wires
goofy rolling heads
exaggerated physics
🧲 THE HEAD ROLL SYSTEM (Very Important)

This can become your:

signature feature
Make heads:
physics-enabled
bounce realistically
emit funny voice lines
blink LEDs while rolling
Add:
“Head Punt”

Players can kick heads.

Add:
“Head Taunts”

Heads say:

"WARNING: dignity compromised."
🚩 3. THE “FLAG” SYSTEM (Excellent idea)

Your squishy stress-log object is MUCH better than a normal flag.

This becomes:

a moving high-value chaos object
🧠 DESIGN GOALS

The object should:

create fights
create betrayals
create panic
create hilarious drops
🧸 OBJECT CONCEPT

Think:

squishy glowing cyber stress-toy relic

NOT military flag.

⚙️ CORE RULES
Carrier gets:
score multiplier
extraction multiplier
cosmetic XP boost
BUT:

Carrier suffers:

high vulnerability
🎯 IMPORTANT MECHANIC

If carrier gets hit:

DROP IMMEDIATELY

No health threshold.

This creates:

chaos
scrambling
reversals
🧠 REPOSITIONING SYSTEM

When dropped:

object destabilizes
teleports after delay
respawns in harder locations
🔥 BEST VERSION

The object should:

squirm
squeak
glow brighter over time
become more unstable
🎮 THIS BECOMES:

Your equivalent of:

Halo skulls
Rocket League ball moments
Smash Bros item chaos
4. ⚔️ WEAPON FOUNDATION (12 Weapons)

Good launch target.

But:

12 UNIQUE weapons
not 12 stat variations.

🧠 AAA RULE

Every weapon must:

look different
sound different
alter movement/combat style
Suggested Roles
2 Beam Weapons
2 Precision Weapons
2 Close Range
2 Mobility Weapons
2 Utility Weapons
2 Wildcard/Chaos Weapons
🎨 6 SKINS EACH (Good launch baseline)

BUT:
Do NOT manually build skin logic per weapon.

Build:

master material skin architecture
Skin Layers
Base Material
Glow Pattern
Animated Emissive
Decals
Reactive Effects
🔥 Reactive Skins (VERY IMPORTANT)

AAA engagement comes from:

skins that evolve during gameplay

Examples:

heats up on streak
pulses during overdrive
changes color on low health
5. 🛒 MARKETPLACE + ECONOMY

This is HUGE engineering.

Do NOT rush this early.

PHASED APPROACH
Phase 1

Simple store:

direct purchases
battle pass
rotating shop
Phase 2

Player inventory backend

Phase 3

P2P trading marketplace

⚠️ WARNING

Player trading introduces:

fraud
duplication exploits
economy inflation
support nightmares

You need:

authoritative backend inventory service

NOT client-trusted inventory.

Suggested Stack

Backend:

PlayFab
EOS
custom inventory DB later
🎰 MARKET ENGAGEMENT FEATURES
Rotating Store

Daily:

featured skins
limited FX
Collections

Set bonuses:

Neon Ronin Set
Rarity Tiers
Common
Rare
Epic
Mythic
Glitched
6. 🧑‍🤝‍🧑 SOCIAL LOBBY LOUNGE (VERY SMART)

This is where retention explodes.

🧠 WHY IT MATTERS

Modern multiplayer games succeed because:

players hang out even when not playing
🏟️ YOUR VERSION

Think:

cyber sportsbook arena lounge
Features
Spectator Walls

Watch live matches.

Betting Tokens (non-gambling)

Predict winners:

earn cosmetics currency
Mini-games
head kicking
beam basketball
hover races
Social Flexing
inspect skins
emote zones
hologram MVP statues
🎮 THIS IS CRITICAL:

Idle social engagement increases:

retention
cosmetic sales
friend invites
7. 🗺️ MAP FOUNDATION (6 Maps)

Good starting number.

AAA MAP RULES

Every map must have:

one memorable mechanic
Examples
Arena 1

Moving laser walls

Arena 2

Gravity shifts

Arena 3

Energy storms

Arena 4

Head bounce jump pads

Arena 5

Vertical rail system

Arena 6

Shrinking extraction zones

IMPORTANT:

Maps should support:

beam visibility
movement flow
readable combat
8. 📊 LIVE SERVICE TOOLING

AAA games live/die on analytics.

TRACK EVERYTHING
Weapon stats
accuracy
kill rates
pick rates
Player behavior
quit timing
extraction success
favorite maps
Economy
skin purchases
retention correlation
Use:
PlayFab telemetry
EOS analytics
custom dashboards later
9. 🧠 PLAYER ENGAGEMENT SYSTEMS

This is where “one more match” comes from.

Core Systems
Daily Challenges
Get 3 ricochet kills
Seasonal Events
Head Hunt Festival
Ranked Rewards

Exclusive cosmetics.

Kill Cam Highlights

Auto-generate viral clips.

10. 🎥 SPECTATOR + STREAMER SYSTEMS

VERY important for growth.

Add:
Observer Camera
Replay System
Match Highlight Generator
Streamer UI Mode
🚨 MOST IMPORTANT AAA DEVELOPMENT RULE

DO NOT build:

all systems simultaneously

Build in:

vertical slices
🏗️ RECOMMENDED AAA DEVELOPMENT PHASES
PHASE 1 — “THE FUN TEST”

(2–3 months)

ONLY:

Pulse
Beam
Dash
Energy
Dismemberment
1 map

Goal:

Is this FUN?
PHASE 2 — “SOCIAL CHAOS”

(2 months)

Add:

stress-object mode
extraction
social lounge
overdrive
PHASE 3 — “CONTENT”

(3–5 months)

Add:

remaining weapons
skins
maps
progression
PHASE 4 — “LIVE SERVICE”

(ongoing)

Add:

battle pass
store
analytics
ranked
tournaments
🚀 BIGGEST SUGGESTION I’LL GIVE YOU

Your game’s identity should become:

"Competitive cyber sports chaos."

NOT:

generic futuristic shooter

That means:

goofy robot destruction
dramatic beam duels
head physics
stress-object scramble
neon spectacle
high mobility

That’s memorable.

🎯 YOUR HIGHEST VALUE FEATURES

The systems most likely to make your game stand out are:

TOP TIER
Robot dismemberment physics
Prism Beam combat
Stress-object chaos mode
Social sportsbook lounge
Reactive neon skins
NEXT STEP I RECOMMEND

You now need:

A FULL AAA PRODUCTION ARCHITECTURE

Meaning:

server/backend stack
database structure
matchmaking architecture
inventory system
anti-cheat plan
content pipeline
live ops tooling
team roles
sprint structure

That’s the layer that determines whether this scales into a real product or collapses under complexity.

🏗️ FULL AAA PRODUCTION ARCHITECTURE

For Your Multiplayer Cyber Neon Robot Arena Game
Built on:

Unreal Engine
Lyra Starter Game
Epic Online Services (EOS)
Steamworks

Goal:

AAA scalable multiplayer architecture
with efficient hosting costs,
live-service scalability,
and fast development velocity.

This architecture is optimized for:

Small-to-mid team efficiency
Fast iteration
Low hosting waste
Future console expansion
Long-term live-service support
🧠 CORE AAA PRINCIPLE

Your architecture should separate:

GAMEPLAY
from
ONLINE SERVICES
from
LIVE CONTENT

If these become tangled:

updates become dangerous
scaling becomes expensive
bugs become catastrophic
🏗️ HIGH-LEVEL SYSTEM ARCHITECTURE
                ┌─────────────────────┐
                │   Unreal Client     │
                │  (Steam/EGS Build)  │
                └─────────┬───────────┘
                          │
          ┌───────────────┼────────────────┐
          │               │                │
          ▼               ▼                ▼

 ┌────────────────┐ ┌───────────────┐ ┌─────────────────┐
 │ EOS Services   │ │ Steamworks    │ │ Backend APIs    │
 │ Auth/Friends   │ │ Commerce      │ │ Inventory/Econ  │
 │ Sessions       │ │ Overlay       │ │ Analytics        │
 └────────────────┘ └───────────────┘ └─────────────────┘
                          │
                          ▼
                ┌─────────────────┐
                │ Dedicated Servers│
                │ Unreal DS Fleet  │
                └─────────────────┘
⚡ YOUR ONLINE STRATEGY
EOS SHOULD HANDLE:

✅ Crossplay
✅ Matchmaking
✅ Friends
✅ Sessions
✅ Voice Chat
✅ Authentication
✅ Presence
✅ Parties

STEAM SHOULD HANDLE:

✅ Storefront
✅ Payments
✅ Achievements
✅ Overlay
✅ Community Hub
✅ Workshop later (optional)

🚨 IMPORTANT

Do NOT build:

your own auth system
your own friends system
your own matchmaking

EOS already solves this.

🧱 RECOMMENDED BACKEND STACK
TIER 1 (Launch Architecture)

This is your ideal efficient stack.

🎮 GAME CLIENT
Unreal Engine + Lyra

Responsibilities:

gameplay
GAS
prediction
rendering
UI
🌐 ONLINE SERVICES
EOS

Responsibilities:

login
crossplay
lobbies
parties
matchmaking
🧠 BACKEND
PlayFab OR custom lightweight backend

Recommended:

PlayFab initially

Handles:

inventory
cosmetics
progression
analytics
player data
🖥️ SERVERS
Dedicated Unreal Servers

Hosted on:

AWS GameLift
OR
Kubernetes later
🧠 BEST HOSTING STRATEGY

DO NOT use peer-to-peer gameplay hosting.

Use:

Dedicated authoritative servers

Why:

anti-cheat
fairness
scalability
competitive integrity
🎮 DEDICATED SERVER MODEL
Match Servers

Each server instance hosts:

1 active match
Example Match Capacity
12–20 players
Server Specs

Early scale:

2–4 vCPU
8 GB RAM
Linux Dedicated Build
💰 COST-EFFICIENT HOSTING STRATEGY
BEST PRACTICE:
Containerized Dedicated Servers

Use:

Docker + Kubernetes

later.

But initially:

GameLift fleets

is faster.

🚀 RECOMMENDED SCALE PATH
PHASE 1 — PROTOTYPE

Hosting:

Single region dedicated servers

Cheap + simple.

PHASE 2 — EARLY ACCESS

Add:

autoscaling
region selection
server orchestration
PHASE 3 — GLOBAL SCALE

Move to:

multi-region Kubernetes orchestration

Only once DAU justifies it.

🧠 DATABASE ARCHITECTURE

Separate databases by responsibility.

PLAYER DATABASE

Stores:

AccountID
Inventory
Cosmetics
Progression
Currencies
Stats

Use:

PostgreSQL
ANALYTICS DATABASE

Stores:

weapon metrics
retention
match stats
economy data

Use:

BigQuery or ClickHouse

later.

Initially:

PlayFab analytics
ECONOMY SERVICE (CRITICAL)

This becomes your:

source of truth
NEVER TRUST CLIENTS

The client should NEVER decide:

owned items
currencies
trades
unlocks

All validated server-side.

🛒 MARKETPLACE ARCHITECTURE
STORE SERVICE

Handles:

daily rotations
pricing
sales
featured cosmetics
bundles
INVENTORY SERVICE

Tracks:

weapon skins
equipped items
trading ownership
limited items
TRADING SYSTEM

VERY advanced.

Do NOT build early.

P2P TRADING SAFETY REQUIREMENTS

You need:
✅ transactional inventory locking
✅ rollback protection
✅ anti-duplication validation
✅ escrow trading flow

SAFE TRADE FLOW
Player A offers item
↓
Server locks item
↓
Player B confirms
↓
Server validates ownership
↓
Atomic exchange
↓
Unlock inventory
🎮 MATCHMAKING ARCHITECTURE

EOS handles:

sessions
parties
matchmaking pools
MATCH TYPES
Casual

Fast matchmaking.

Ranked

MMR-based.

Event Modes

Temporary queues.

RANKED FOUNDATION

Track:

MMR
Extraction Rate
Combat Score
Objective Score
🛡️ ANTI-CHEAT ARCHITECTURE

This matters EARLY.

REQUIRED
Easy Anti Cheat (EAC)

Integrated with EOS.

SERVER AUTHORITATIVE RULES

Server validates:

damage
movement
pickups
inventory
trades
CLIENT PREDICTS ONLY

Client predicts:

visuals
movement feel
responsiveness
🎥 REPLAY + SPECTATOR SYSTEM

AAA requirement.

Build EARLY.

Why?

You need:

highlights
esports
moderation
killcams
social clips
Unreal Replay System

Use:

built-in replay framework
🎮 SOCIAL LOUNGE ARCHITECTURE

This is VERY smart for retention.

Lounge Server Type

Separate from matches.

Responsibilities
spectating
chat
mini-games
social flexing
betting tokens
party forming
IMPORTANT:

Use lower tick rates here.

Saves massive server cost.

📊 LIVE OPS TOOLING

AAA games are data-driven.

ADMIN DASHBOARD

You NEED:

weapon balance controls
economy controls
live event scheduling
skin rotations
RECOMMENDED

Initially:

PlayFab dashboards

Later:

custom admin panel
🎨 CONTENT PIPELINE

This determines whether production scales or becomes chaos.

STANDARDIZED PIPELINES
Weapons

Use:

Data Assets

NOT hardcoded stats.

Cosmetics

Use:

master material instances
Maps

Use:

modular kits
Build:
content validation scripts

to catch errors automatically.

🧠 TEAM STRUCTURE (IDEAL)
CORE ENGINE TEAM

Handles:

networking
GAS
optimization
backend integration
GAMEPLAY TEAM

Handles:

weapons
movement
game modes
LIVE OPS TEAM

Handles:

economy
events
balancing
CONTENT TEAM

Handles:

maps
skins
VFX
COMMUNITY TEAM

Handles:

social systems
Discord
moderation
🚀 OPTIMIZATION STRATEGY (VERY IMPORTANT)

Your game will be VFX heavy.

PRIORITIES
Use:
Niagara pooling
instanced materials
async loading
LODs aggressively
Beam Optimization

Do NOT:

replicate beam particles

Replicate:

beam start/end only

Clients render locally.

🧪 CI/CD PIPELINE (AAA LEVEL)

Critical.

REQUIRED AUTOMATION
Automated:
builds
dedicated server packaging
patch deployment
crash reporting
RECOMMENDED STACK
GitHub + Jenkins/GitHub Actions
AUTO PIPELINE
Commit
↓
Build Validation
↓
Dedicated Server Build
↓
Client Build
↓
Deploy Staging
↓
QA Testing
🎯 LIVE SERVICE CONTENT LOOP

Your retention system should operate like:

Weekly:
    challenges
    rotating shop

Monthly:
    new cosmetics
    map variants

Seasonal:
    ranked reset
    events
    battle pass
🧠 MOST IMPORTANT AAA LESSON

Do NOT build:

all features at launch

Build:

scalable foundations first
🏆 YOUR MOST IMPORTANT TECH FOUNDATIONS

These are the systems that determine success:

TOP PRIORITY
1. Movement Feel
2. Weapon Feel
3. Replication Quality
4. Dismemberment Physics
5. Matchmaking Stability
6. Backend Inventory Integrity
7. Live Ops Tooling
8. Social Retention Systems
🚀 RECOMMENDED DEVELOPMENT ORDER
STAGE 1 — CORE FUN
movement
pulse
beam
dash
dismemberment
STAGE 2 — GAME LOOP
energy
extraction
stress-object
overdrive
STAGE 3 — ONLINE
EOS matchmaking
dedicated servers
parties
progression
STAGE 4 — CONTENT
maps
skins
remaining weapons
STAGE 5 — LIVE SERVICE
store
analytics
battle pass
trading
spectator lounge

FINAL RECOMMENDATION

Your strongest path is NOT:

hardcore esports sim

Your strongest path is:

competitive neon robot sports chaos

That identity supports:

viral clips
spectators
social retention
cosmetics
streamer engagement
long-term replayability

And your robot dismemberment + rolling head system is genuinely strong differentiation if executed with precision.

Full build efficiently as possible.


https://chatgpt.com/share/69fd87d1-d1d8-83e8-8d34-b4c6037999bc