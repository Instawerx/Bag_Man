// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubMirror.h"

#include "AFLHub.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BoxComponent.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/WidgetComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHubMirror)

// --- UAFLHubMirrorWidget ---------------------------------------------------------------------------

TSharedRef<SWidget> UAFLHubMirrorWidget::RebuildWidget()
{
	MirrorImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MirrorImage"));
	WidgetTree->RootWidget = MirrorImage;
	return Super::RebuildWidget();
}

void UAFLHubMirrorWidget::SetMirrorTexture(UTexture* InTexture, const FVector2D& InSize)
{
	if (!MirrorImage || !InTexture)
	{
		return;
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(InTexture);
	Brush.ImageSize = InSize;
	MirrorImage->SetBrush(Brush);
}

// --- AAFLHubMirror ---------------------------------------------------------------------------------

AAFLHubMirror::AAFLHubMirror()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// The capture sits at the glass and looks OUT (+X) at whoever steps up -- the accepted cheap
	// mirror (capture-from-the-surface), not a planar-reflection projection.
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->SetRelativeLocation(FVector(6.f, 0.f, 0.f));
	Capture->FOVAngle = 55.f;
	Capture->bCaptureEveryFrame = false;   // s5 LAW: off by default; the wake box pays the cost
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = SCS_FinalColorLDR; // tonemapped -- matches what the player's screen shows
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	Surface = CreateDefaultSubobject<UWidgetComponent>(TEXT("Surface"));
	Surface->SetupAttachment(Root);
	Surface->SetWidgetSpace(EWidgetSpace::World);
	Surface->SetDrawSize(FVector2D(512.f, 768.f));
	Surface->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	Surface->SetRelativeScale3D(FVector(0.22f)); // ~112x168 cm of glass; operator scales per placement
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WakeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WakeBox"));
	WakeBox->SetupAttachment(Root);
	WakeBox->SetRelativeLocation(FVector(220.f, 0.f, 0.f));
	WakeBox->SetBoxExtent(FVector(220.f, 200.f, 150.f));
	WakeBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WakeBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	WakeBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AAFLHubMirror::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return; // pure cosmetics; the dedicated server strips render comps anyway (proven law)
	}

	MirrorRT = NewObject<UTextureRenderTarget2D>(this, TEXT("MirrorRT"));
	MirrorRT->InitAutoFormat(512, 768);
	MirrorRT->ClearColor = FLinearColor(0.01f, 0.012f, 0.02f, 1.f);
	Capture->TextureTarget = MirrorRT;

	Surface->SetWidgetClass(UAFLHubMirrorWidget::StaticClass());
	if (UAFLHubMirrorWidget* Widget = Cast<UAFLHubMirrorWidget>(Surface->GetWidget()))
	{
		Widget->SetMirrorTexture(MirrorRT, FVector2D(512.f, 768.f));
	}

	WakeBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLHubMirror::OnWakeBeginOverlap);
	WakeBox->OnComponentEndOverlap.AddDynamic(this, &AAFLHubMirror::OnWakeEndOverlap);
}

void AAFLHubMirror::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShowOnlyTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AAFLHubMirror::OnWakeBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return; // remote pawns never wake a capture (s5: the cost is paid only by the player at the glass)
	}
	WatchedPawn = Pawn;
	RefreshShowOnly();
	Capture->bCaptureEveryFrame = true;
	GetWorldTimerManager().SetTimer(ShowOnlyTimer, this, &AAFLHubMirror::RefreshShowOnly, 0.5f, true);
	UE_LOG(LogAFLHub, Log, TEXT("AFL_MIRROR: awake for '%s'."), *Pawn->GetName());
}

void AAFLHubMirror::OnWakeEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || Pawn != WatchedPawn.Get())
	{
		return;
	}
	WatchedPawn = nullptr;
	Capture->bCaptureEveryFrame = false;
	GetWorldTimerManager().ClearTimer(ShowOnlyTimer);
	UE_LOG(LogAFLHub, Log, TEXT("AFL_MIRROR: asleep."));
}

void AAFLHubMirror::RefreshShowOnly()
{
	APawn* Pawn = WatchedPawn.Get();
	if (!Pawn)
	{
		Capture->bCaptureEveryFrame = false;
		GetWorldTimerManager().ClearTimer(ShowOnlyTimer);
		return;
	}
	// The widget component creates its UUserWidget lazily -- re-push the RT here (2 Hz, awake-only,
	// idempotent) so the glass is fed even when BeginPlay ran before the widget existed.
	if (UAFLHubMirrorWidget* Widget = Cast<UAFLHubMirrorWidget>(Surface ? Surface->GetWidget() : nullptr))
	{
		Widget->SetMirrorTexture(MirrorRT, FVector2D(512.f, 768.f));
	}

	// You + everything hanging off you: character-part actors, the held (try-on) weapon, accessories.
	// Refreshed at 2 Hz while awake because try-on SWAPS those actors under the mirror's nose.
	Capture->ShowOnlyActors.Reset();
	Capture->ShowOnlyActors.Add(Pawn);
	TArray<AActor*> Attached;
	Pawn->GetAttachedActors(Attached, /*bResetArray*/ true, /*bRecursivelyIncludeAttachedActors*/ true);
	for (AActor* A : Attached)
	{
		Capture->ShowOnlyActors.Add(A);
	}
}
