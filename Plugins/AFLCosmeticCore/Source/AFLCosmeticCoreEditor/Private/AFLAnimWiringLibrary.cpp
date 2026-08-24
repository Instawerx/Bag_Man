// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLAnimWiringLibrary.h"

#include "Animation/AnimBlueprint.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_AnimDynamics.h"
#include "AnimGraphNode_Root.h"
#include "BoneControllers/AnimNode_AnimDynamics.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogAFLAnimWiring, Log, All);

static UEdGraph* FindAnimGraph(UAnimBlueprint* ABP)
{
	TArray<UEdGraph*> Graphs;
	ABP->GetAllGraphs(Graphs);
	for (UEdGraph* G : Graphs)
	{
		// The AnimGraph is the one whose schema is the animation schema and whose name is "AnimGraph".
		if (G && G->GetFName() == TEXT("AnimGraph")) { return G; }
	}
	// Fallback: first graph carrying the animation schema.
	for (UEdGraph* G : Graphs)
	{
		if (G && G->Schema && G->Schema->IsChildOf(UAnimationGraphSchema::StaticClass())) { return G; }
	}
	return nullptr;
}

static UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Dir)
{
	if (!Node) { return nullptr; }
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Dir && UAnimationGraphSchema::IsPosePin(Pin->PinType))
		{
			return Pin;
		}
	}
	return nullptr;
}

bool UAFLAnimWiringLibrary::WireChainDynamics(UAnimBlueprint* ABP, FName BoundBone, FName ChainEnd, int32 LODThreshold)
{
	if (!ABP) { UE_LOG(LogAFLAnimWiring, Warning, TEXT("WireChainDynamics: null AnimBP.")); return false; }

	UEdGraph* Graph = FindAnimGraph(ABP);
	if (!Graph) { UE_LOG(LogAFLAnimWiring, Warning, TEXT("WireChainDynamics: no AnimGraph on %s."), *ABP->GetName()); return false; }

	// The output pose node is always present in a fresh AnimGraph.
	UAnimGraphNode_Root* Root = nullptr;
	UAnimGraphNode_AnimDynamics* Dyn = nullptr;
	for (UEdGraphNode* N : Graph->Nodes)
	{
		if (UAnimGraphNode_Root* R = Cast<UAnimGraphNode_Root>(N)) { Root = R; }
		else if (UAnimGraphNode_AnimDynamics* D = Cast<UAnimGraphNode_AnimDynamics>(N)) { Dyn = D; }
	}
	if (!Root) { UE_LOG(LogAFLAnimWiring, Warning, TEXT("WireChainDynamics: no Output Pose node.")); return false; }

	// IDEMPOTENT: reconfigure an existing node rather than stacking a second one.
	if (!Dyn)
	{
		FGraphNodeCreator<UAnimGraphNode_AnimDynamics> Creator(*Graph);
		Dyn = Creator.CreateNode();
		Dyn->NodePosX = Root->NodePosX - 400;
		Dyn->NodePosY = Root->NodePosY;
		Creator.Finalize();
	}

	// --- CONFIGURE THE NODE (component-space bone chain, jewellery-scale) ---
	FAnimNode_AnimDynamics& Node = Dyn->Node;
	Node.bChain = true;
	Node.BoundBone.BoneName = BoundBone;
	Node.ChainEnd.BoneName = ChainEnd;
	Node.SimulationSpace = AnimPhysSimSpaceType::Component;   // ruled: component space, cheaper, no teleport issues
	Node.LODThreshold = LODThreshold;                         // ruled: 1
	// Jewellery-scale sway: a soft angular spring so links settle quickly and swing small, not simulate.
	Node.bAngularSpring = true;
	Node.AngularSpringConstant = 80.0f;
	Node.GravityScale = 1.0f;
	Dyn->ReconstructNode();   // rebuild pins to match the (now chain-enabled) config

	// --- WIRE: AnimDynamics output pose -> Output Pose input ---
	UEdGraphPin* DynOut = FindPosePin(Dyn, EGPD_Output);
	UEdGraphPin* RootIn = FindPosePin(Root, EGPD_Input);
	if (!DynOut || !RootIn)
	{
		UE_LOG(LogAFLAnimWiring, Warning, TEXT("WireChainDynamics: pose pins not found (dynOut=%d rootIn=%d)."),
			DynOut != nullptr, RootIn != nullptr);
		return false;
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	// The animation schema auto-inserts a space converter if the pin spaces differ, so this handles
	// component-space AnimDynamics feeding the local-space output.
	const bool bConnected = Schema->TryCreateConnection(DynOut, RootIn);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ABP);
	FKismetEditorUtilities::CompileBlueprint(ABP);

	// VERIFY the wire is actually present after compile, not merely that TryCreateConnection returned.
	bool bLinked = false;
	for (const UEdGraphPin* LP : RootIn->LinkedTo)
	{
		if (LP) { bLinked = true; break; }
	}
	UE_LOG(LogAFLAnimWiring, Display,
		TEXT("WireChainDynamics %s: bound=%s end=%s space=Component lod=%d  connectCall=%d  wiredAfterCompile=%d"),
		*ABP->GetName(), *BoundBone.ToString(), *ChainEnd.ToString(), LODThreshold, bConnected, bLinked);
	return bLinked;
}
