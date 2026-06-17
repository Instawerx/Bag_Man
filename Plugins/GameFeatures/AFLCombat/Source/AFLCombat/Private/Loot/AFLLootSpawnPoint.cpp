// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootSpawnPoint.h"

#include "Components/SceneComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootSpawnPoint)

AAFLLootSpawnPoint::AAFLLootSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Bare transform root -- the marker IS a location; the director reads GetActorTransform() server-side.
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	// Editor visibility only (billboards don't render in-game) -- a sprite so the designer can place/select it.
	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(SceneRoot);
	static ConstructorHelpers::FObjectFinder<UTexture2D> SpriteFinder(TEXT("/Engine/EditorResources/S_TargetPoint"));
	if (SpriteFinder.Succeeded())
	{
		Billboard->SetSprite(SpriteFinder.Object);
	}
#endif
}
