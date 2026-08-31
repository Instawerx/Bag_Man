// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"

#include "AFLHubDestinationVolume.generated.h"

class UBoxComponent;
class UWidgetComponent;
class UTexture2D;
class UCommonActivatableWidget;

/** What a hub door DOES when its backend is proven. Spec s4.3: unproven backends read Disabled. */
UENUM(BlueprintType)
enum class EAFLHubDestinationAction : uint8
{
	Disabled    UMETA(DisplayName = "Disabled (backend unproven)"),
	OpenScreen  UMETA(DisplayName = "Open Screen"),
	Travel      UMETA(DisplayName = "Travel To Map"),
	JoinClub    UMETA(DisplayName = "Join Club"),
};

USTRUCT(BlueprintType)
struct FAFLHubDestinationRow
{
	GENERATED_BODY()

	/** Row key -- the placed volume's DestinationId points here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	FName DestinationId;

	/** Player-facing door name ("PX STORE", "ROBO LABS"...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	FText DisplayName;

	/** At-door plate subtitle ("Weapons - Masks - Jewellery - Robots"). Ratified sign mock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	FText Subtitle;

	/** Destination glyph (T_AFL_HubGlyph_*) -- FAR diamond + AT-DOOR badge. White strokes, tinted live. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	TSoftObjectPtr<UTexture2D> Glyph;

	/** Disabled until the backend ticket lands (s4.3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	EAFLHubDestinationAction Action = EAFLHubDestinationAction::Disabled;

	/** Payload for the proven action -- widget path (OpenScreen) or map id (Travel). Inert while Disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	FString ActionPayload;
};

/**
 * UAFLHubDestinationsData -- the one table every hub door reads (DA_AFL_HubDestinations).
 * Adding/enabling a door is a DATA edit; the volumes never hardcode names or actions.
 */
UCLASS(BlueprintType)
class AFLHUB_API UAFLHubDestinationsData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub", meta = (TitleProperty = "DestinationId"))
	TArray<FAFLHubDestinationRow> Rows;

	const FAFLHubDestinationRow* FindRow(FName DestinationId) const;
};

/**
 * AAFLHubDestinationVolume  (AFL-3405 / spec s4.3 -- doors at real doorways)
 *
 * COSMETIC-ONLY wayfinding sign at a ratified doorway, per the operator-approved mock (canvas
 * 0542c547): a screen-space UAFLHubSignWidget (camera-facing, draws over structures) switching
 * between three distance tiers on a low-rate timer -- FAR beacon (diamond + name + pillar), MID
 * plate (name + distance), AT-DOOR full plate (name + subtitle + status band, driven by the box
 * overlap). Row data (name/subtitle/action) resolves from DA_AFL_HubDestinations by DestinationId.
 * No GE, no server logic -- the ACTION half belongs to the later per-backend tickets; a Disabled
 * row reads OFFLINE. Sibling of AAFLHubZoneVolume (same box shape) but deliberately NOT a tag
 * dispenser -- zones own tags, doors own signs.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLHubDestinationVolume : public AActor
{
	GENERATED_BODY()

public:
	AAFLHubDestinationVolume();

	/** Server-side row re-resolution for the Travel backend (UAFLHubTravelComponent): the client
	 *  names a row, the server reads THIS placed volume's resolved action + payload. */
	bool GetTravelContract(FName& OutId, EAFLHubDestinationAction& OutAction, FString& OutPayload) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnDoorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDoorEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** True only for the pawn whose screen this sign should react to. */
	static bool IsLocalPlayerPawn(const AActor* Actor);

	/** Low-rate distance-tier decision (ratified mock: far beacon / mid plate / at-door). */
	void UpdateSignTier();

	/** Interact keypress (E / gamepad face-left; non-consuming) -- acts only AT-DOOR on an enabled row. */
	void OnInteractPressed();

	/** ESC / gamepad B: closes OUR pushed screen (uniform exit for handler-less screens). */
	void OnExitPressed();

	/** The enabled-row action: OpenScreen pushes ActionPayload's widget class to UI.Layer.Menu (the
	 *  proven takeover mount). Travel/JoinClub land with their own backend tickets. */
	void ExecuteDoorAction();

	/** The approach trigger (root). Extent is the designer knob on the placed instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	TObjectPtr<UBoxComponent> PromptBox;

	/** The ratified holo sign (screen-space widget: camera-facing + visible over structures). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	TObjectPtr<UWidgetComponent> SignWidget;

	/** Which DA_AFL_HubDestinations row this door is. The ONLY per-door datum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	FName DestinationId;

	/** The shared destinations table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Hub")
	TObjectPtr<UAFLHubDestinationsData> Destinations;

	/** Resolved at BeginPlay so the tier timer stays trivial. */
	EAFLHubDestinationAction ResolvedAction = EAFLHubDestinationAction::Disabled;
	FText ResolvedName;
	FText ResolvedSubtitle;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> ResolvedGlyph;
	bool bPawnInVolume = false;
	bool bInteractBound = false;
	/** The takeover we pushed -- gates re-entry while it lives (E behind the UI double-pushed). */
	TWeakObjectPtr<UCommonActivatableWidget> PushedScreen;
	FTimerHandle TierTimer;
	FString ResolvedPayload;
};
