// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDisplayPedestal.h"

#include "AFLHub.h"
#include "AFLHubSignWidget.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Components/InputComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDisplayPedestal)

AAFLDisplayPedestal::AAFLDisplayPedestal()
{
	// Shelf-sized engage trigger, the door PromptBox pattern scaled to a display fixture.
	ShelfBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ShelfBox"));
	ShelfBox->SetupAttachment(RootComponent);
	ShelfBox->SetBoxExtent(FVector(160.f, 160.f, 120.f));
	ShelfBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ShelfBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ShelfBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	PlateWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PlateWidget"));
	PlateWidget->SetupAttachment(RootComponent);
	PlateWidget->SetRelativeLocation(FVector(0.f, 0.f, 170.f));
	PlateWidget->SetWidgetSpace(EWidgetSpace::Screen);
	PlateWidget->SetDrawAtDesiredSize(true);
}

void AAFLDisplayPedestal::BeginPlay()
{
	Super::BeginPlay(); // the spawner half: registry resolve + pad visuals (the AAA display)

	// COSMETIC-ONLY below (s4 doctrine: retail visuals are client-local, 0% server overhead).
	if (GetNetMode() == NM_DedicatedServer || !PlateWidget)
	{
		return;
	}

	PlateWidget->SetWidgetClass(UAFLHubSignWidget::StaticClass());
	ShelfBox->OnComponentBeginOverlap.AddDynamic(this, &AAFLDisplayPedestal::OnShelfBeginOverlap);
	ShelfBox->OnComponentEndOverlap.AddDynamic(this, &AAFLDisplayPedestal::OnShelfEndOverlap);
	GetWorldTimerManager().SetTimer(PlateTimer, this, &AAFLDisplayPedestal::UpdatePlate, 0.5f, true);
	UpdatePlate();
}

void AAFLDisplayPedestal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlateTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AAFLDisplayPedestal::AttemptPickUpWeapon_Implementation(APawn* /*Pawn*/)
{
	// RETAIL: the pad never grants. Deliberately empty -- engagement (E at the shelf) is the verb.
}

void AAFLDisplayPedestal::OnShelfBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	bPawnAtShelf = true;

	// LAZY engage bind at the first at-shelf moment (the proven door pattern verbatim).
	if (!bEngageBound)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (PC->IsLocalController() && PC->InputComponent)
			{
				FInputKeyBinding& KB = PC->InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAFLDisplayPedestal::OnEngagePressed);
				KB.bConsumeInput = false;
				FInputKeyBinding& GB = PC->InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AAFLDisplayPedestal::OnEngagePressed);
				GB.bConsumeInput = false;
				bEngageBound = true;
			}
		}
	}
	UpdatePlate();
	UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: AT-SHELF '%s'."), *CosmeticId.ToString());
}

void AAFLDisplayPedestal::OnShelfEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	bPawnAtShelf = false;
	UpdatePlate();
}

void AAFLDisplayPedestal::OnEngagePressed()
{
	if (!bPawnAtShelf || CosmeticId.IsNone())
	{
		return;
	}
	UClass* PageClass = LoadClass<UCommonActivatableWidget>(nullptr, *ProductPageClassPath);
	if (!PageClass)
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_RETAIL: '%s' product page class '%s' did not resolve."),
			*CosmeticId.ToString(), *ProductPageClassPath);
		return;
	}
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LP)
	{
		return;
	}
	UCommonActivatableWidget* Page = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), TSubclassOf<UCommonActivatableWidget>(PageClass));
	UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: ENGAGE '%s' -> %s."), *CosmeticId.ToString(), *GetNameSafe(PageClass));

	// Hand the SKU over reflectively -- the page lives in AFLCombat; AFLHub fronts seams, never
	// links UI internals (the ProcessEvent recipe, same as the quickbar reach).
	if (Page)
	{
		if (UFunction* Fn = Page->FindFunction(FName(TEXT("FocusCosmeticId"))))
		{
			struct { FName Id; } Args{ CosmeticId };
			Page->ProcessEvent(Fn, &Args);
		}
	}
}

void AAFLDisplayPedestal::UpdatePlate()
{
	UAFLHubSignWidget* Plate = PlateWidget ? Cast<UAFLHubSignWidget>(PlateWidget->GetWidget()) : nullptr;
	if (!Plate)
	{
		return;
	}
	FText Name = FText::FromName(CosmeticId);
	FText PriceLine = FText::GetEmpty();
	bool bSellable = false;
	if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld()))
	{
		if (const FAFLCatalogEntry* Entry = Catalog->FindEntry(CosmeticId))
		{
			Name = Entry->DisplayName.IsEmpty() ? Name : Entry->DisplayName;
			PriceLine = Catalog->GetEntryPriceText(*Entry);
			bSellable = Entry->bTransactable;
		}
	}
	APawn* Pawn = GetWorld() && GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
	const float Meters = Pawn ? FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) / 100.f : 999.f;
	const EAFLHubSignTier Tier = (bPawnAtShelf || Meters < 6.f) ? EAFLHubSignTier::AtDoor : (Meters < 20.f ? EAFLHubSignTier::Mid : EAFLHubSignTier::Far);
	Plate->SetSignData(Name, PriceLine, bSellable, Tier, Meters, nullptr);
}
