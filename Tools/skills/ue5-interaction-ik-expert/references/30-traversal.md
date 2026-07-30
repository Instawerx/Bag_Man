# 30 — Traversal: Mantle/Vault, Slide-Under, Ladders, Vehicles, Ziplines, Grapple, Swimming

## Procedural Vaulting & Mantle (Motion Warping)

Isolates obstacle dimensions at runtime and warps root motion of a single montage to fit varied heights/depths.

Montage sync markers (Motion Warping Anim Notify States, sequential): `Warp_Mantle_Start` (horizontal approach + hand-plant), `Warp_Mantle_Apex` (vertical clearance), `Warp_Mantle_Exit` (landing point beyond obstacle).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_CalculateMantleTargets"
   CustomFunctionName="Execute_CalculateMantleTargets"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="ObstacleActor",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetObstacleBoundingBox"
   FunctionReference=(Name="GetActorBounds")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(1)=(PinName="Origin",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
   Pins(2)=(PinName="BoxExtent",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AddWarpTargetStart"
   FunctionReference=(Name="AddOrUpdateWarpTargetFromLocationAndRotation")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="WarpTargetName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Warp_Mantle_Start")
   Pins(3)=(PinName="TargetLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(4)=(PinName="TargetRotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Input")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AddWarpTargetApex"
   FunctionReference=(Name="AddOrUpdateWarpTargetFromLocationAndRotation")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="WarpTargetName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Warp_Mantle_Apex")
   Pins(3)=(PinName="TargetLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(4)=(PinName="TargetRotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Input")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: box/double-line trace cluster on obstacle encounter → `GetActorBounds` → `Warp_Mantle_Start` = closest edge minus **75.0 cm** capsule padding; `Warp_Mantle_Apex` = `Origin.Z` + `BoxExtent.Z` ceiling vector (hand-plant center) → register both warp targets back-to-back → play montage.

**Polish — Fallback Intercept**: if `Target_Apex_Location.Z - Actor_Location.Z` exceeds physiological max, reject the warp and route to an organic wall-thump impact animation.

## Context-Aware Vault-Over / Slide-Under

Variables: `Barrier_Clearance_Height` (Float); `Ceiling_Clearance_Gap` (Float); `Traversal_Stance_State` (Enum: Vault_Over / Slide_Under / Reject_Interaction).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_AnalyzeBarrierGeometry"
   CustomFunctionName="Execute_BarrierGeometryScan"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_VerticalHeightTrace"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_Visibility")
   Pins(4)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_CalculateHeadroomGap"
   FunctionReference=(Name="Subtract_FloatFloat")
   Pins(0)=(PinName="A",PinType=(PinCategory="float"),Direction="EGPD_Input") # Top boundary hit
   Pins(1)=(PinName="B",PinType=(PinCategory="float"),Direction="EGPD_Input") # Lower edge hit
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_SwitchEnum Name="Switch_SelectTraversalStance"
   Pins(0)=(PinName="Selection",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ETraversalStanceState"),Direction="EGPD_Input")
   Pins(1)=(PinName="Vault_Over",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(2)=(PinName="Slide_Under",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(3)=(PinName="Reject",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: on vault/slide key, two forward traces — chest height (**120.0 cm**) and ground level (**20.0 cm**).
- Lower hits, chest clears → downward trace over the edge measures `Barrier_Clearance_Height`; **40.0–110.0 cm** → `Vault_Over`.
- Chest hits, ground clears → upward trace measures `Ceiling_Clearance_Gap`; > **65.0 cm** → `Slide_Under`.
- Register obstacle front-to-back depth bounds to MotionWarpingComponent → play matching montage branch.

**Polish — Asymmetric Root-Velocity Matching**: play-rate scalar = forward velocity / baseline sprint velocity, passed to `PlayAnimMontage` so animation speed matches approach speed — kills visual sliding at the barrier.

## IK-Driven Procedural Ladder Climbing

Ladder asset: root socket `Base_Anchor` + sequential `Rung_01..N` sockets. Interface returns `Rung_Spacing` (Float) and rung count on mount.

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_LadderClimbing"
   # Step 1: Local vertical offset of character root vs ladder anchor
   Begin Node Type=RigVMModelFunctionNode Name="Function_GetCharacterLadderLocalZ"
      FunctionName="MathTransformInverse"
      Pins(0)=(Name="Value",Direction=Input,Value="Character_World_Transform")
      Pins(1)=(Name="Parent",Direction=Input,Value="Ladder_Base_Anchor_Transform")
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 2: Active rung indices per limb
   Begin Node Type=RigVMModelFunctionNode Name="Function_CalculateActiveRungs"
      FunctionName="MathFloatMod"
      Pins(0)=(Name="Value",Direction=Input)
      Pins(1)=(Name="Alpha",Direction=Input,Value="Rung_Spacing")
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 3: FBIK locking limbs to evaluated coordinates
   Begin Node Type=RigVMModelFunctionNode Name="Function_LadderFBIKSolver"
      FunctionName="FullBodyIK"
      Pins(0)=(Name="Root",Direction=Input,DefaultValue="pelvis")
      Pins(1)=(Name="Effectors",Direction=Input)
         # Effector 0: hand_l -> Left Hand Rung Vector
         # Effector 1: hand_r -> Right Hand Rung Vector
         # Effector 2: foot_l -> Left Foot Rung Vector
         # Effector 3: foot_r -> Right Foot Rung Vector
      Pins(2)=(Name="PullWeight",Direction=Input,DefaultValue="1.0")
   End Node
End Object
```

Flow: handler tracks upward movement; height / `Rung_Spacing` = target rung indices. Phase-shift limbs (left hand + right foot move together while right hand + left foot anchor). Push world targets thread-safe into FBIK effector pins.

**Polish — Procedural Hand Slippage/Weight**: slight Cos-wave noise offset driven by velocity applied to hand effectors — organic grip compression when climbing fast.

## Dynamic Vehicle Mounting / Entry

Montage `AM_Vehicle_Entry` markers: `Warp_Vehicle_Approach` (feet face handle), `Warp_Vehicle_DoorLatch` (hand to door handle), `Warp_Vehicle_SeatIn` (root into seat).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_RegisterVehicleWarpTargets"
   CustomFunctionName="Execute_RegisterVehicleWarpTargets"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="VehicleActor",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetVehicleInteractionTransforms"
   FunctionReference=(Name="GetVehicleSockets")
   Pins(0)=(PinName="TargetVehicle",PinType=(PinCategory="object"),Direction="EGPD_Input")
   Pins(1)=(PinName="OutApproachTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Output")
   Pins(2)=(PinName="OutLatchTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Output")
   Pins(3)=(PinName="OutSeatTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AddWarpVehicleApproach"
   FunctionReference=(Name="AddOrUpdateWarpTargetFromLocationAndRotation")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="WarpTargetName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Warp_Vehicle_Approach")
   Pins(3)=(PinName="TargetLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(4)=(PinName="TargetRotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Input")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AddWarpVehicleSeat"
   FunctionReference=(Name="AddOrUpdateWarpTargetFromLocationAndRotation")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="WarpTargetName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Warp_Vehicle_SeatIn")
   Pins(3)=(PinName="TargetLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(4)=(PinName="TargetRotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Input")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: `GetVehicleSockets` via the interface → register three warp targets → play montage; hand IK stays locked on the swinging handle socket (no clipping) → final frame of `Warp_Vehicle_SeatIn`: Anim Notify fires `AttachToComponent` + controller possession swap.

**Polish — Camera Horizon Correction**: during `Warp_Vehicle_SeatIn`, blend viewpoint rotation into the vehicle forward matrix over **0.65 s** with cubic ease-in/out via a Camera Modify layer.

## Spline-Driven Zipline / Slide

Parameters: `Current_Spline_Distance` (Float); `Zipline_Gravity_Acceleration` (Float | 980.0); `Cable_Spline_Ref` (SplineComponent ref from environmental actor).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_ProcessSplineMovement"
   CustomFunctionName="Execute_SplineMovementTrack"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="DeltaSeconds",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetLocationAtDistance"
   FunctionReference=(Name="GetLocationAtDistanceAlongSpline")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SplineComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="Distance",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(3)=(PinName="CoordinateSpace",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ESplineCoordinateSpace"),Direction="EGPD_Input",DefaultValue="World")
   Pins(4)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetRotationAtDistance"
   FunctionReference=(Name="GetQuaternionAtDistanceAlongSpline")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SplineComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="Distance",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(3)=(PinName="CoordinateSpace",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ESplineCoordinateSpace"),Direction="EGPD_Input",DefaultValue="World")
   Pins(4)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Quat"),Direction="EGPD_Output")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_ZiplineHandPinning"
   # Step 1: Lock hands to trolley bar via twin Basic IK solvers
   Begin Node Type=RigVMModelFunctionNode Name="Function_LockRightHandToTrolley"
      FunctionName="BasicIK"
      Pins(0)=(Name="Item",Direction=Input,DefaultValue="hand_r")
      Pins(1)=(Name="Target",Direction=Input,Value="Right_Trolley_Handle_Transform")
      Pins(2)=(Name="Weight",Direction=Input,Value="Zipline_IK_Blend_Alpha")
   End Node

   # Step 2: Travel speed -> torso lag angle
   Begin Node Type=RigVMModelFunctionNode Name="Function_CalculateInertialLag"
      FunctionName="MathTransformInverse"
      Pins(0)=(Name="Value",Direction=Input,Value="Current_Velocity_Vector")
      Pins(1)=(Name="Parent",Direction=Input,DefaultValue="pelvis")
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 3: Spine lean responding to momentum
   Begin Node Type=RigVMModelFunctionNode Name="Function_ApplyInertialSpineLean"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="spine_01")
      Pins(1)=(Name="Transform",Direction=Input) # Pitch from accel/decel spikes
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::LocalSpace")
      Pins(3)=(Name="Weight",Direction=Input,Value="Zipline_IK_Blend_Alpha")
   End Node
End Object
```

Flow: on interaction, movement mode → **Flying**, reset distance. Each frame, evaluate spline pitch at the player's position, velocity step = pitch × `Zipline_Gravity_Acceleration` × DeltaSeconds → accumulate distance → `GetLocationAtDistanceAlongSpline` / `GetQuaternionAtDistanceAlongSpline` → set root capsule transform. Twin Basic IK clamp `hand_r`/`hand_l` to trolley handles; low-weight FBIK from hips down lets legs dangle/sway.

**Polish**:
- **Kinetic Landing Handoff**: at spline end, read linear velocity at the detach frame, pass into CharacterMovement launch functions — slide/bunny-hop naturally into a run.
- **Torsional Slack Correction**: Sin-wave oscillator × travel-progress percentage (peaks at midpoint) drives subtle body/camera roll conveying cable tension.

## Grappling Hook / Rope Trajectory Climbing

Globals: `Grapple_Anchor_World` (Vector); `Rope_Current_Slack` (Float — belly sag depth); `Is_Hanging_Active` (Bool).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_CalculateGrapplePath"
   CustomFunctionName="Execute_GrapplePathCalculations"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetCableLength"
   FunctionReference=(Name="Distance")
   Pins(0)=(PinName="V1",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input") # Hand world position
   Pins(1)=(PinName="V2",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input") # Grapple_Anchor_World
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetCableProperties"
   FunctionReference=(Name="SetComponentTickEnabled")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/CableComponent.CableComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="bEnabled",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(3)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_GrappleIKClimbing"
   # Step 1: Direction line from chest up to hook anchor
   Begin Node Type=RigVMModelFunctionNode Name="Function_GetGrappleVector"
      FunctionName="MathVectorSubtract"
      Pins(0)=(Name="A",Direction=Input,Value="Grapple_Anchor_World")
      Pins(1)=(Name="B",Direction=Input) # chest/spine_03 bone position
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 2: Pin hands onto the rope line via Basic IK
   Begin Node Type=RigVMModelFunctionNode Name="Function_PinHandToGrappleLine"
      FunctionName="BasicIK"
      Pins(0)=(Name="Item",Direction=Input,DefaultValue="hand_r")
      Pins(1)=(Name="Target",Direction=Input) # Position along rope direction vector
      Pins(2)=(Name="Weight",Direction=Input,Value="Grapple_IK_Blend_Weight")
   End Node

   # Step 3: Pelvis drop conveying harness gravity
   Begin Node Type=RigVMModelFunctionNode Name="Function_ApplyHarnessWeight"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="pelvis")
      Pins(1)=(Name="Transform",Direction=Input) # Negative local Z offset
      Pins(2)=(Name="Weight",Direction=Input,Value="Grapple_IK_Blend_Weight")
   End Node
End Object
```

Flow: valid grapple trace hit → store `Grapple_Anchor_World`, enable a `CableComponent` visual → update cable length while swinging/climbing; scale `Rope_Current_Slack` down with climb progress (taut under tension) → `Is_Hanging_Active` = True: hands clamp onto the rope line alternating procedurally hand-over-hand; pelvis pulled down; legs drape via low-weight FBIK.

**Polish — Kinetic Tension Snap**: at the taut-catch frame, fire high-frequency low-amplitude gamepad haptics + a rapid short rotational kickback on upper spine bones in the Control Rig — conveys sudden weight change when the cable catches.

## Procedural Swimming & Buoyancy

Globals: `Water_Surface_Z` (Float); `Immersion_Depth_Alpha` (0.0 waist → 1.0 neck); `Swim_Paddle_Speed` (Float, from forward input velocity).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_EvaluateWaterImmersion"
   CustomFunctionName="Execute_WaterImmersionEvaluation"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetWaterVolumeDimensions"
   FunctionReference=(Name="GetWaterSurfaceHeight")
   Pins(0)=(PinName="TargetVolume",PinType=(PinCategory="object"),Direction="EGPD_Input")
   Pins(1)=(PinName="ActorLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="OutSurfaceZ",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_NormalizeImmersionDepth"
   FunctionReference=(Name="MapRangeClamped")
   Pins(0)=(PinName="Value",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(1)=(PinName="InLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="-20.0") # Waist deep rel. root
   Pins(2)=(PinName="InRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="90.0") # Neck deep rel. root
   Pins(3)=(PinName="OutLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="0.0")
   Pins(4)=(PinName="OutRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="1.0")
   Pins(5)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object
```

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_ProceduralWaterSwimming"
   # Step 1: Cosine phase offset rotating pelvis to horizontal
   Begin Node Type=RigVMModelFunctionNode Name="Function_RotatePelvisToHorizontal"
      FunctionName="MathMathCos"
      Pins(0)=(Name="Value",Direction=Input) # Engine Time x Swim_Paddle_Speed
      Pins(1)=(Name="Result",Direction=Output)
   End Node

   # Step 2: Spine forward offset for treading posture
   Begin Node Type=RigVMModelFunctionNode Name="Function_SetSpineSwimPosture"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="spine_02")
      Pins(1)=(Name="Transform",Direction=Input) # Pitch forward by Immersion_Depth_Alpha
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::LocalSpace")
   End Node

   # Step 3: Sinusoidal cycles into arm/leg effectors -> automated strokes
   Begin Node Type=RigVMModelFunctionNode Name="Function_LimbPaddleCycle"
      FunctionName="FullBodyIK"
      Pins(0)=(Name="Root",Direction=Input,DefaultValue="pelvis")
      Pins(1)=(Name="Effectors",Direction=Input)
         # Effector 0: hand_l -> cyclical circle coordinate offsets
         # Effector 1: hand_r -> inverted cyclical circle coordinate offsets
   End Node
End Object
```

Flow: on fluid-volume entry, query `Water_Surface_Z`, measure root-to-surface distance → map to `Immersion_Depth_Alpha`; > **0.5** → floating/swimming movement state → pelvis/spine tilt forward up to **75.0°** to horizontal posture; offset Sin/Cos waves drive hand/foot effector circles in FBIK, stroke speed scaling with movement input.

**Polish — Surface Tension Floating Adaptor**: feed water surface height into Spring Interp (damping 0.5, stiffness 35.0) nudging neck/head bones up as waves pass — face stays cleanly above water.
