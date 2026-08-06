# AIK Task — `B_AFL_Extractor` (branded, pulsing, audio extractor system)

Paste **§A** into UE **Tools → Agent Chat** (AIK, *AFL Blueprint & Gameplay* profile). Builds the reusable extractor art/FX system that wraps the existing `B_AFL_ExtractionZone` gameplay. Rodin hero meshes (Spaceship, Teleporter) are generating separately — this task builds the framework + materials/FX/audio; meshes drop into the skin slot when ready.

## §A — AIK PROMPT
```
GOAL: Build B_AFL_Extractor in /Game/BagMan/Extraction/ -- a modular Blueprint that
skins + juices the extraction objective. It WRAPS (does not replace) the existing
B_AFL_ExtractionZone gameplay (enter-volume = extract). Reusable across all maps.

INSPECT FIRST: open /Game/BagMan/Extraction/B_AFL_ExtractionZone and report how its
extraction volume / active state is exposed (overlap, "extraction available/active"
bool or event). Show your plan before creating.

COMPONENTS on B_AFL_Extractor (Actor):
- Scene root + a child B_AFL_ExtractionZone (or its trigger) so extraction works unchanged.
- "Skin" StaticMeshComponent -- empty slot; a variant mesh is assigned per instance
  (Rodin Extractor_Spaceship / Extractor_Teleporter import later). Enterable footprint.
- "Branding" DecalComponent -- IRONICS/BagMan logo, uses signature NeonColor.
- NiagaraComponent "EnergyFX" -- idle sparks/beam; intensifies when active.
- AudioComponent "Idle" (looping hum) + one-shot "Activate" cue; 3D attenuated.
- Arrow/Scene "AttachSocket" for coupling into other features (tunnel/truck/UFO).

MATERIAL: create M_AFL_Extractor_Pulse (emissive):
  Emissive = NeonColor (vector param) * PulseCurve, where
  PulseCurve = lerp(IdleMin, PeakMax, 0.5 + 0.5*sin(Time * PulseSpeed)).
  Params: NeonColor, PulseSpeed, IdleMin, PeakMax. Make MI_AFL_Extractor_Spaceship
  and MI_AFL_Extractor_Teleporter (different NeonColor per StandardSix palette).

BEHAVIOR (Event Graph / Timeline):
- Idle: slow pulse (PulseSpeed low), idle hum audio, low Niagara.
- When extraction becomes AVAILABLE/ACTIVE (bind to the zone state): ramp PulseSpeed up
  (fast pulse), play Activate cue, boost Niagara (beam/portal surge).
- On extract complete: flash + one-shot, then return to idle.
- Expose: NeonColor, PulseSpeed, SkinMesh, and a "Variant" enum (Spaceship/Teleporter)
  as instance-editable so designers reskin per placement.

VARIANTS (data only, meshes assigned later):
- Spaceship: engine-nacelle glow, boarding-ramp ingress, heavier hum.
- Teleporter: vertical portal beam (Niagara), pad ring pulse, rising whoosh cue.

VERIFY: place one B_AFL_Extractor in a test level, confirm it pulses, plays audio,
and still triggers extraction on overlap. Do NOT rename B_AFL_ExtractionZone.
```

## Notes
- Audio assets: if `SC_AFL_Extractor_Idle` / `_Activate` don't exist, stub with any placeholder cue and flag for a SFX pass.
- Rodin meshes: `Content/AFL/_Bridge/Rodin/extractor_gens.json` has the task UUIDs; download → conform (afl-blender-bridge: scale to character, UCX collision, LODs) → assign to the Skin slot + set the variant MI.
- Placement (bridge, me): drop `B_AFL_Extractor` on each map's extraction-zone transforms (ARCANEON has 3), set the Variant/NeonColor per spot, and support the modular attach (tunnel/truck) via AttachSocket.
