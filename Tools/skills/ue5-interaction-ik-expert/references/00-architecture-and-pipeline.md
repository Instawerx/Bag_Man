# 00 — Core Architecture, Attach/Detach Physics, Data Pipeline, Trace Validation

## Game-Agnostic System Architecture

```
[BP_Interactive_Actor] -> (Implements) -> [BPI_Interact] -> (Reads Sockets) -> [BPC_InteractionHandler] -> (Drives) -> [Character AnimInstance]
```

- **Blueprint Interfaces (BPI_Interact)**: never hard "Cast To" specific blueprints. Interact via universal interface functions like `GetIKTargetSockets` or `OnInteractionTriggered`.
- **Component-Driven Logic (BPC_InteractionHandler)**: interaction mechanics live entirely inside an Actor Component so the whole pick/throw/warp system drops onto any pawn instantly.
- **Premium UX Signalling**: always expose an explicit distance/angle validation check before triggering. If the player faces away, smoothly reject input or warp back gracefully.

## AAA Schema: Attach/Detach & Physics Logic

Prevents floating objects, delayed attachments, jitter during lift/throw via a strict synchronous pipeline:

```
[Detect Interaction] -> [Disable Object Collision] -> [Attach & Weld] -> [Interpolate IK Target]
                                                                |
[Apply Physics Impulse] <- [Set Linear Velocity] <- [Detach & Restore Collision] <--- (On Throw)
```

Step-by-step:
1. **Disable Collision First**: `SetCollisionEnabled(NoCollision)` on the target mesh before anything else — prevents the physics asset fighting the character capsule.
2. **Attach**: `AttachComponentToComponent` → socket = hand socket (e.g., `Grip_Socket_R`); Location/Rotation/Scale = **Snap to Target**; **Weld Simulated Bodies = True** (merges physics bodies cleanly).
3. **Throw/Roll Transition**: `DetachFromComponent` with all rules **Keep World** → `SetCollisionEnabled(QueryAndPhysics)` → `SetSimulatePhysics(true)`.
4. **AAA Velocity Hand-off**: don't rely solely on `AddImpulse`. Read character/hand-socket tangential velocity via `GetPhysicsLinearVelocity` and pass that vector into the object's `SetPhysicsLinearVelocity` at the exact detach frame so momentum feels seamless.

### Blueprint Graph Text — BPC_InteractionHandler initialization

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_Comment Name="Comment_Init"
   NodeComment="Initialize Interaction System on BeginPlay"
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraph_Event Name="Event_BeginPlay"
   EventName="ReceiveBeginPlay"
   FunctionReference=(Name="ReceiveBeginPlay")
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_VariableGet Name="Get_Owner"
   VariableReference=(Name="Owner")
   Pins(0)=(PinName="Owner",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetMotionWarping"
   FunctionReference=(Name="GetComponentByClass")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(2)=(PinName="ComponentClass",PinType=(PinCategory="class",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Input",DefaultObject=/Script/MotionWarping.MotionWarpingComponent)
   Pins(3)=(PinName="ReturnValue",PinType=(PinCategory="object",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Output")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

### Blueprint Graph Text — Object Attachment (Lifting)

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_AttachObject"
   CustomFunctionName="Execute_AttachObject"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="TargetInteractable",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Output")
   Pins(2)=(PinName="SocketName",PinType=(PinCategory="name"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetMesh"
   FunctionReference=(Name="GetComponentByClass")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(1)=(PinName="ComponentClass",PinType=(PinCategory="class",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Input",DefaultObject=/Script/Engine.PrimitiveComponent)
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_DisableSimulatePhysics"
   FunctionReference=(Name="SetSimulatePhysics")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="InSimulate",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="false")
   Pins(3)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetCollisionNoCollision"
   FunctionReference=(Name="SetCollisionEnabled")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="NewType",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionEnabled"),Direction="EGPD_Input",DefaultValue="NoCollision")
   Pins(3)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AttachComponentToComponent"
   FunctionReference=(Name="AttachComponentToComponent")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SceneComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="Parent",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SceneComponent"),Direction="EGPD_Input")
   Pins(3)=(PinName="InSocketName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(4)=(PinName="LocationRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EAttachmentRule"),Direction="EGPD_Input",DefaultValue="SnapToTarget")
   Pins(5)=(PinName="RotationRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EAttachmentRule"),Direction="EGPD_Input",DefaultValue="SnapToTarget")
   Pins(6)=(PinName="ScaleRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EAttachmentRule"),Direction="EGPD_Input",DefaultValue="KeepWorld")
   Pins(7)=(PinName="bWeldSimulatedBodies",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(8)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

### Blueprint Graph Text — Motion Warping Registration (Door/Latch Alignment)

Component setup: add `MotionWarping` component to the Character Blueprint. In the interaction Animation Montage, add a Motion Warping Anim Notify State, Warp Name exactly `WarpTarget_Latch`, Warp Translation = True, Warp Rotation = True. Break the door's interaction socket transform into World Location/Rotation, call `AddOrUpdateWarpTargetFromLocationAndRotation` (name must match exactly), then `PlayAnimMontage` immediately — root motion warps over the notify window.

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_SetupMotionWarp"
   CustomFunctionName="Execute_SetupMotionWarp"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="WarpTargetTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Output")
   Pins(2)=(PinName="WarpTargetName",PinType=(PinCategory="name"),Direction="EGPD_Output",DefaultValue="WarpTarget_Latch")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_BreakTransform"
   FunctionReference=(Name="BreakTransform")
   Pins(0)=(PinName="InTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Input")
   Pins(1)=(PinName="Location",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
   Pins(2)=(PinName="Rotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Output")
   Pins(3)=(PinName="Scale",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AddOrUpdateWarpTarget"
   FunctionReference=(Name="AddOrUpdateWarpTargetFromLocationAndRotation")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="WarpTargetName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(3)=(PinName="TargetLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(4)=(PinName="TargetRotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Input")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

### Blueprint Graph Text — Detach & Throw Physics Hand-off

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_DetachAndThrow"
   CustomFunctionName="Execute_DetachAndThrow"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="TargetInteractable",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Output")
   Pins(2)=(PinName="ThrowLinearVelocity",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
   Pins(3)=(PinName="ThrowAngularVelocity",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_DetachFromActor"
   FunctionReference=(Name="DetachFromActor")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(2)=(PinName="LocationRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EDetachRule"),Direction="EGPD_Input",DefaultValue="KeepWorld")
   Pins(3)=(PinName="RotationRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EDetachRule"),Direction="EGPD_Input",DefaultValue="KeepWorld")
   Pins(4)=(PinName="ScaleRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EDetachRule"),Direction="EGPD_Input",DefaultValue="KeepWorld")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_EnableSimulatePhysics"
   FunctionReference=(Name="SetSimulatePhysics")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="InSimulate",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(3)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetCollisionQueryAndPhysics"
   FunctionReference=(Name="SetCollisionEnabled")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="NewType",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionEnabled"),Direction="EGPD_Input",DefaultValue="QueryAndPhysics")
   Pins(3)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetPhysicsLinearVelocity"
   FunctionReference=(Name="SetPhysicsLinearVelocity")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="NewVelocity",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="bAddToCurrent",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="false")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetPhysicsAngularVelocity"
   FunctionReference=(Name="SetPhysicsAngularVelocityInDegrees")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PrimitiveComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="NewAngularVelocity",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="bAddToCurrent",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="false")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

## AAA Schema: Control Rig Graph (Hand Pinning)

Dynamically glue a hand to a moving gun slide / swinging door handle without snapping artifacts via dynamic weight blending and space switching:

```
[Forward Dynamics / Anim] -> [Get Control Transform (IK_Target)] -> [FABRIK / Full Body IK] -> [Alpha Blend (0.0 <-> 1.0)]
```

1. Create global controls `ctrl_hand_l_IK` and `ctrl_hand_r_IK` in the Control Rig hierarchy.
2. Expose public variables: `LeftHand_IK_Transform` / `RightHand_IK_Transform` (Transform), `LeftHand_IK_Alpha` (Float).
3. Graph: `Set Transform` node targeting `ctrl_hand_l_IK` fed by the public transform variable → FABRIK or Full Body IK solver (Root: `clavicle_l`/`shoulder_l`, Effector: `hand_l`, Target: `ctrl_hand_l_IK` transform).
4. Feed `LeftHand_IK_Alpha` into the solver's **Weight** pin; interpolate alpha from Blueprint with `FInterpTo` to avoid visual pops.

## Thread-Safe AnimInstance → Control Rig Data Pipeline

```
[BPC_InteractionHandler] -> (Updates Data) -> [Character Blueprint]
                                                    | (Property Access / Thread-Safe)
                                             [AnimInstance (AnimGraph)]
                                                    | (Control Rig Node Mapping)
                                             [Control Rig (Solvers)]
```

Push data down via Property Access / Control Rig input variables — never read character variables every frame inside the Control Rig.

**AnimInstance variables** (Category: IK_Data): `IK_LeftHand_Transform`, `IK_RightHand_Transform` (Transform); `IK_LeftHand_Alpha`, `IK_RightHand_Alpha` (Float, default 0.0).

**Thread-safe update** — use `BlueprintThreadSafeUpdateAnimation` (never `GetPlayerCharacter -> Cast To` in the update event):

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_BlueprintThreadSafeUpdateAnimation"
   CustomFunctionName="BlueprintThreadSafeUpdateAnimation"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="DeltaSeconds",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetOwningActor"
   FunctionReference=(Name="GetOwningActor")
   Pins(0)=(PinName="ReturnValue",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetComponentByClass"
   FunctionReference=(Name="GetComponentByClass")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(1)=(PinName="ComponentClass",PinType=(PinCategory="class",PinSubCategoryClass=/Script/Engine.ActorComponent"),Direction="EGPD_Input",DefaultObject=/Script/Engine.ActorComponent)
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.ActorComponent"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetIKTargetData"
   FunctionReference=(Name="GetIKTargetData")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object"),Direction="EGPD_Input")
   Pins(2)=(PinName="OutLeftTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Output")
   Pins(3)=(PinName="OutRightTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Output")
   Pins(4)=(PinName="OutLeftAlpha",PinType=(PinCategory="float"),Direction="EGPD_Output")
   Pins(5)=(PinName="OutRightAlpha",PinType=(PinCategory="float"),Direction="EGPD_Output")
   Pins(6)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

**AnimGraph Control Rig node mapping** — select the Control Rig node in the AnimGraph, check "Use" next to input variables to expose them as input pins, then wire AnimInstance variables in:

```
Begin Object Class=/Script/AnimGraph.AnimNode_ControlRig Name="AnimNode_ControlRig_Interaction"
   SourceRigClass=/Script/ControlRig.ControlRigClass'/Game/Characters/Rigs/CR_Mannequin_Interaction.CR_Mannequin_Interaction'
   Pins(0)=(PinName="Source",PinType=(PinCategory="struct",PinSubCategory="PoseLink"),Direction="EGPD_Input")
   Pins(1)=(PinName="In_LeftHand_Transform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Input")
   Pins(2)=(PinName="In_RightHand_Transform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Input")
   Pins(3)=(PinName="In_LeftHand_Alpha",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(4)=(PinName="In_RightHand_Alpha",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(5)=(PinName="Result",PinType=(PinCategory="struct",PinSubCategory="PoseLink"),Direction="EGPD_Output")
End Object
```

**Control Rig input declaration** — declare variables as Inputs with "Is Exposed" = True:

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_IK_Pipeline"
   Begin Node Type=RigVMModelExposedValueParameterNode Name="Parameter_In_LeftHand_Transform"
      ValueType="FTransform"
      Direction=Input
   End Node

   Begin Node Type=RigVMModelExposedValueParameterNode Name="Parameter_In_LeftHand_Alpha"
      ValueType="float"
      Direction=Input
   End Node

   Begin Node Type=RigVMModelFunctionNode Name="Function_TransformInverse"
      FunctionName="MathTransformInverse"
      Pins(0)=(Name="Value",Direction=Input)
      Pins(1)=(Name="Parent",Direction=Input)
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   Begin Node Type=RigVMModelFunctionNode Name="Function_SetControlTransform"
      FunctionName="SetControlTransform"
      Pins(0)=(Name="Control",Direction=Input,DefaultValue="ctrl_hand_l_IK")
      Pins(1)=(Name="Transform",Direction=Input)
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EControlRigTransformSpace::LocalSpace")
   End Node
End Object
```

**Polish — frame-rate gating & interp**: interpolate alphas in the character component (`FInterpTo` driven by `GetWorldDeltaSeconds`, Interp Speed ≈ 15.0). For heavy objects / high-recoil weapons substitute **Spring Interp** for physical mass/weight lag.

## Line Trace Validation Layer (Interface-Gated Interaction Scan)

Runs in BPC_InteractionHandler. Variables: `Trace_Distance` (Float, 250.0); `Interaction_Channels` (ECollisionChannel array, target `ECC_PhysicsBody` or custom `ECC_GameTraceChannel_Interactable`).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_PerformInteractionScan"
   CustomFunctionName="Execute_InteractionScan"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetCameraLocationAndVectors"
   FunctionReference=(Name="GetActorEyesViewPoint")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(1)=(PinName="OutLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
   Pins(2)=(PinName="OutRotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_LineTraceSingleByChannel"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_Visibility")
   Pins(4)=(PinName="bTraceComplex",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(5)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(6)=(PinName="ReturnValue",PinType=(PinCategory="bool"),Direction="EGPD_Output")
   Pins(7)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_DoesImplementInterface"
   FunctionReference=(Name="DoesImplementInterface")
   Pins(0)=(PinName="TestObject",PinType=(PinCategory="object"),Direction="EGPD_Input")
   Pins(1)=(PinName="InterfaceClass",PinType=(PinCategory="class",PinSubCategoryClass=/Script/Engine.BlueprintInterface"),Direction="EGPD_Input",DefaultObject=/Script/GameFramework.BPI_Interactable)
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="bool"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_Branch Name="Branch_ValidateInterface"
   Pins(0)=(PinName="Condition",PinType=(PinCategory="bool"),Direction="EGPD_Input")
   Pins(1)=(PinName="True",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(2)=(PinName="False",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: `GetActorEyesViewPoint` → forward vector × `Trace_Distance` + camera position = End → `LineTraceSingleByChannel` (Trace Complex = True) → break HitResult, HitActor → `DoesImplementInterface(BPI_Interactable)` → Branch: True fires interaction logic; False cleanly clears active target references.

**Polish**: drive UI crosshair from the interface validation bool — interpolate reticle opacity 0.4 → 1.0 and scale ×1.2 when a valid interactable is in range, telling the player they're in range before pressing anything.
