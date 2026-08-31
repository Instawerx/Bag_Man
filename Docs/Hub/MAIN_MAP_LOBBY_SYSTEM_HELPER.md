# MAIN MAP LOBBY SYSTEM HELPER DOC — OPERATOR-PROVIDED BUILD AUTHORITY (imported 2026-08-31)

> Imported verbatim from the operator's docx. This doc GOVERNS the Outpost lobby
> system build; in-repo SSOTs support and implement it. Where an in-repo doc
> conflicts, THIS doc + dated operator rulings win.

Here is the official Architecture and Implementation Specification document. It outlines the integration of your new persistent social hub into your existing PlayFab and AWS GameLift codebase.
TECHNICAL ARCHITECTURE SPECIFICATION
System: Persistent Social & Game Hub Integration (1,000+ Players)Target Map: Military Mega Base Pack (Optimized Demo Map)Stack: Unreal Engine (C++/Blueprints), AWS GameLift, PlayFab, EOS OSS

1. HIGH-LEVEL NETWORK ARCHITECTURE
To achieve a 1,000+ player capacity without crashing server performance, this architecture implements a strict Asymmetric Network Topology.
All spatial movement, zone safety rules, and final purchase validation are Server-Authoritative (via AWS GameLift). Conversely, all shopping visual states, attachment previews, cosmetics, and mirror renderings are Client-Local (0% server overhead).
   [ AWS GameLift Dedicated Server ] <--- (Auth State, Fast Positioning, Matchmaking Tickets)
                ▲
                │  (Server RPC: Commit Final Purchase / Equip)
                │
   [ Local Client Viewport ] ─── (PlayFab SDK) ───> [ PlayFab Backend Cloud ]
        │          │                                 (Currency & Inventory Auth)
        ▼          ▼
  (Local Preview) (Optimized Mirrors)

2. COMPONENT DECOUPLING & SERVER OPTIMIZATION (C++)
Standard Unreal Engine replication mechanics (ACharacterMovementComponent) cannot process 1,000 concurrent entities. The Hub Level requires a stripped, lightweight network footprint.
2.1 Character Replication Bypassing
Class: Create a specialized AHubCharacter : ACharacter.
Movement Configuration: Disable standard movement replication components on the server for the Hub map. Use an asynchronous, low-frequency positioning update loop.
Network Relevance: Set strict NetCullDistanceSquared thresholds. Spatial data from players at the Deployment Zone must never replicate to clients inside the PX Store.
2.2 Bandwidth Optimization Struct
Use the following network-quantized C++ struct to broadcast character positioning at a throttled frame rate (10Hz–15Hz):
cpp
// USTRUCT optimized for minimum hub replication bandwidth
USTRUCT(BlueprintType)
FHubLowFrequencyTransform
{
    GENERATED_BODY()

    UPROPERTY()
    FVector_NetQuantize10 Position; // Automatically truncates floating point precision

    UPROPERTY()
    uint8 YawCompressed; // Compresses 360-degree rotation into a single byte (0-255)
};
Use code with caution.

3. ZONE-BASED SAFETY ENGINE (GEOPLAY TAGS)
The Hub must prevent players from shooting or killing others outside designated areas (e.g., the Shooting Range). This is governed natively via Server-Authoritative Gameplay Tags.
       [ AHubZoneVolume (Trigger Box) ]
                       │
       ┌───────────────┴───────────────┐
       ▼                               ▼
[ OnOverlapBegin ]              [ OnOverlapEnd ]
Add Gameplay Tag:               Remove Gameplay Tag:
"Capability.Weapon.CanFire"      "Capability.Weapon.CanFire"
3.1 Verification Logic Blueprint
Every interactive action on the weapon wheel or input component must validate against the player's runtime Gameplay Tag Container on the server:
cpp
// Core safety check inside character input or weapon component
bool UWeaponComponent::CanExecuteFireCycle() const
{
    AHubCharacter* HubChar = Cast<AHubCharacter>(GetOwner());
    if (!HubChar) return false;

    // Enforce safety zone tag checking
    return HubChar->GetGameplayTags().HasTag(
        FGameplayTag::RequestGameplayTag(TEXT("Capability.Weapon.CanFire"))
    );
}
Use code with caution.

4. UNIVERSAL LOCAL RETAIL FRAMEWORK (PX, LABS, BARRACKS)
To ensure hundreds of players can view, spin, and test assets simultaneously without replicating those massive asset swaps to the server, all item checking utilizes a Client-Side Preview Anchor system.
4.1 Interactive Asset Preview Pattern
The Display: Items on shelves (Scarlett Rifle, masks, jewelry, robots) are static meshes inside an APXDisplayRack actor.
The View State: Interacting with a rack smoothly interpolates the player's camera to a fixed AHubPreviewAnchor position using SetViewTargetWithBlend.
Local Mesh Injection: The client framework spawns a localized, non-replicated 3D asset instance (weapon mesh or stationary robot asset) directly in front of the camera or socketed onto the local player's character.
Interaction Handling: Mouse-drag inputs are captured locally to rotate the preview asset model on its local pivot matrix.
cpp
// Triggers the local preview pipeline when a user inspects a display rack
void AHubCharacter::InitiateLocalAssetPreview(AHubPreviewAnchor* TargetAnchor, FName CatalogItemID)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !IsLocallyControlled()) return;

    // Smoothly blend client camera viewport to shop display layout anchor
    PC->SetViewTargetWithBlend(TargetAnchor, 0.4f, VTBlend_Cubic);

    // Spawn non-replicated preview asset model (Robot, Gun, Mask) locally on client
    TargetAnchor->SpawnClientOnlyPreviewInstance(CatalogItemID);

    // Push local UI layer overlay (Buy / Put Back options)
    CurrentHUD->DisplayRetailCheckoutOverlay(CatalogItemID, TargetAnchor);
}
Use code with caution.
4.2 PlayFab Transaction Workflow
When a player transitions from a preview to a formal purchase, the inventory transaction bypasses the GameLift server entirely to protect compute resources:
Client Request: Client triggers PlayFabClientAPI::PurchaseItem using existing virtual/staked currency mappings.
Success Hook: PlayFab updates the cloud database and returns a validation payload to the client.
World State Commit: The client destroys the local preview mesh. If equipping the item permanently, the client sends a single Server RPC (Server_EquipPermanentCosmetic(ItemID)). The server verifies ownership against PlayFab cloud data once, updates the persistent character blueprint mesh, and handles basic network replication to nearby players.

5. CROWD-OPTIMIZED REAL-TIME MIRRORS
Traditional scene capture components will cause critical rendering bottlenecks if 1,000 players attempt to view their characters in a shared retail environment. Mirrors must use strict visibility and distance-based execution gates.
5.1 Mirror Optimization Implementation
Default State: USceneCaptureComponent2D is explicitly deactivated (bCaptureEveryFrame = false).
Proximity Gating: Use a localized box collision trigger in front of physical dressing mirrors.
Show-Only Filtering: Configure the capture frame to completely ignore the outside world map and the other 999 online player meshes, focusing execution entirely on the local client character model.
cpp
// Activates rendering pipelines exclusively for the local player standing at a display mirror
void AHubMirror::OnMirrorTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, 
                                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                                        bool bFromSweep, const FHitResult& SweepResult)
{
    AHubCharacter* ViewingChar = Cast<AHubCharacter>(OtherActor);
    if (ViewingChar && ViewingChar->IsLocallyControlled())
    {
        // Awake the scene capture rendering engine for this client viewport only
        SceneCaptureComponent->bCaptureEveryFrame = true;
        
        // Performance Maximization: Force the mirror to render ONLY the local character + previews
        SceneCaptureComponent->ShowOnlyActorComponents(ViewingChar);
    }
}
Use code with caution.

6. SOCIAL CLUBS & SUB-LOBBY ISOLATION (EOS OSS)
The Social Clubs allow players to establish private, invite-only, or friend-centric groups within the larger server instance. This architecture maps EOS Lobbies directly underneath the GameLift Game Server Connection.
6.1 Session Management Workflow
The Layer Split: Players maintain their active dedicated server network connection to the GameLift Military Base level.
Party Instantiation: When renting or activating a private room loop inside a Social Club, the player invokes IOnlineLobby::CreateLobby via the EOS Online Subsystem.
Privacy Flags: Set permission thresholds using native EOS variables:
For public/friend queries: EOnlineSessionPermissionLevel::FriendsOnly
For isolated rooms: EOnlineSessionPermissionLevel::InviteOnly
Visibility Masking: When friends congregate inside an active EOS Lobby party room, a local C++ gameplay tag manager toggles the rendering visibility profile of any player character not registered inside that specific EOS party data cluster, creating a private instanced environment without loading into a new map.

7. DEPLOYMENT ZONE MATCHMAKING GATEWAY
The Deployment Zone functions as a physical pipeline to pass players from the social GameLift Hub straight into your existing League and Staked matchmaking servers.
7.1 Matchmaking Transition Mechanics
Trigger Event: A player walks into the Deployment Zone volume.
Solo Queue Pass: The game pushes the existing matchmaking UI overlay. On confirmation, the client gracefully calls Disconnect() from the GameLift Hub and enters your existing matchmaker queue pipeline.
EOS Party Queue Pass: If the player is the leader of an active EOS Sub-Lobby party:
The leader initiates the Matchmaking Ticket cycle via your existing backend service.
The ticket ID data is written to the EOS Lobby Attributes.
All connected clients in that EOS Lobby listen for attribute changes via C++ delegates, read the ticket payload, and register into your existing matchmaking pool as a single structured group.

8. MAP CLEANUP & DEVELOPMENT TASKLIST
To prepare the Military Mega Base Pack Demo Map asset for deployment as a 1,000-player executable, your engineering team must complete the following optimization scrub:
Actor Sanitation: Delete all pre-packaged demo AI blueprints, ticking target actors, interactive asset sequences, and physics props placed by the original environment artist.
HISM Conversion: Pass all static mesh prop clusters (crates, structural beams, wall decorations, gun lockers) through Unreal's Merge Actors tool to convert them into Hierarchical Instanced Static Meshes (HISMs) to drop draw call overhead.
NavMesh Restriction: Shrink and constrain the core Navigation Bounds Volume to encompass only active walkable surfaces between the PX Store, Labs, Barracks, Deployment Zone, and Social Clubs.
Collision Optimization: Ensure all retail display shelving profiles are assigned to a clean, lightweight custom Object Channel (e.g., ECC_GameActions) for optimized, non-blocking client-side line tracing.
### Secondary document detailing the PlayFab CloudScript/Lambda webhook payload required to safely transfer a player's character customization state directly from the Labs UI back onto their GameLift Server Avatar upon checkout
Here is the secondary technical document detailing the PlayFab backend integration payload and execution loop.
INTERFACE SPECIFICATION: PLAYFAB TO GAMELIFT COSMETIC SYNC
System: Character Labs Customization & Checkout PipelineArchitecture: PlayFab CloudScript (JavaScript/TypeScript) or AWS Lambda (C#/.NET or Node.js) WebhookData Flow: Client UI ➔ PlayFab Cloud Inventory Auth ➔ AWS GameLift Server RPC

1. THE CHECKOUT FLOW PIPELINE
To prevent players from hacking or injecting unowned cosmetics into the 1,000-player GameLift server, the GameLift server must never trust cosmetic data sent directly from a game client. The server must only load data that has been validated by PlayFab.
[ Labs UI: Click Checkout ] 
             │
             ▼
[ PlayFab CloudScript / Lambda ] ──( 1. Validates Owned Inventory & Deducts Currency )
             │
             ├───────────────────( 2. Writes Verified State to PlayFab User Data )
             │
             ▼
[ GameLift Server Webhook ] ─────( 3. Server Receives Push Token & Securely Updates Avatar )

2. WEBHOOK PAYLOAD SPECIFICATION
When a player clicks "Purchase & Equip" inside the Labs UI, the client sends the desired setup to your PlayFab CloudScript/Lambda function.
2.1 Request Payload (Client to PlayFab)
The client sends the unique item IDs they are trying to permanently equip.
json
{
  "PlayFabId": "A1B2C3D4E5F6G7H8",
  "CharacterId": "HubAvatar_01",
  "CatalogVersion": "Main_Catalog_2026",
  "ItemsToPurchase": [
    "mask_tactical_ballistic_03",
    "jewelry_ring_gold_skull"
  ],
  "ItemsToEquip": {
    "Slot_Mask": "mask_tactical_ballistic_03",
    "Slot_Jewelry_Finger": "jewelry_ring_gold_skull",
    "Slot_Weapon_Skin_Scarlett": "skin_scarlett_rifle_tiger"
  }
}
Use code with caution.
2.2 Server-to-Server Push Payload (PlayFab to AWS GameLift API)
Once PlayFab verifies the player owns the items (or processes the virtual currency transaction successfully), the CloudScript/Lambda function securely posts this verified payload directly to your GameLift Server Management endpoint or matchmaking coordinator.
json
{
  "EventName": "PlayerCosmeticCheckoutConfirmed",
  "Timestamp": "2026-08-24T22:45:12Z",
  "Data": {
    "PlayFabId": "A1B2C3D4E5F6G7H8",
    "GameLiftPlayerSessionId": "psess-11112222-3333-4444-5555-666677778888",
    "ActiveGameSessionId": "arn:aws:gamelift:us-west-2::gamesession/fleet-1234/gsess-abcd",
    "ValidatedEquipment": {
      "Slot_Mask": "mask_tactical_ballistic_03",
      "Slot_Jewelry_Finger": "jewelry_ring_gold_skull",
      "Slot_Weapon_Skin_Scarlett": "skin_scarlett_rifle_tiger"
    }
  }
}
Use code with caution.

3. BACKEND CLOUDSCRIPT / LAMBDA SOURCE CODE (NODE.JS)
This serverless function processes the transaction, updates the user's persistent cloud profile, and forwards the secure authorization token down to the running GameLift instance.
javascript
// PlayFab CloudScript / AWS Lambda Handler for Labs Checkout Validation
const PlayFabServer = require("playfab-sdk/Scripts/PlayFab/PlayFabServer");

exports.handler = async (event) => {
    const requestData = JSON.parse(event.body);
    const playFabId = requestData.PlayFabId;
    const itemsToEquip = requestData.ItemsToEquip;

    try {
        // 1. Authoritative Server Check: Verify inventory ownership via PlayFab Server API
        const userInventory = await getUserInventory(playFabId);
        
        for (const [slot, itemId] of Object.entries(itemsToEquip)) {
            const hasOwnership = userInventory.some(item => item.ItemId === itemId);
            if (!hasOwnership) {
                return {
                    statusCode: 400,
                    body: JSON.stringify({ error: `Security Violation: Item ${itemId} not owned by player.` })
                };
            }
        }

        // 2. Commit the new persistent equipment state to PlayFab User Read-Only Data
        await updateUserReadOnlyData(playFabId, {
            "CurrentEquippedHubCosmetics": JSON.stringify(itemsToEquip)
        });

        // 3. Route the validated data payload directly to the running AWS GameLift Session
        const gameLiftSuccess = await forwardToGameLiftServer(
            requestData.ActiveGameSessionId, 
            playFabId, 
            itemsToEquip
        );

        if (!gameLiftSuccess) {
            throw new Error("Failed to communicate state with active GameLift dedicated server instance.");
        }

        return {
            statusCode: 200,
            body: JSON.stringify({ success: true, message: "Inventory verified and applied to GameLift avatar." })
        };

    } catch (error) {
        return {
            statusCode: 500,
            body: JSON.stringify({ error: error.message })
        };
    }
};

// Helper function to fetch inventory serverless-side
function getUserInventory(playFabId) {
    return new Promise((resolve, reject) => {
        PlayFabServer.GetUserInventory({ PlayFabId: playFabId }, (error, result) => {
            if (error) reject(error);
            else resolve(result.data.Inventory);
        });
    });
}

// Helper function to update read-only properties securely
function updateUserReadOnlyData(playFabId, dataPayload) {
    return new Promise((resolve, reject) => {
        PlayFabServer.UpdateUserReadOnlyData({ PlayFabId: playFabId, Data: dataPayload }, (error, result) => {
            if (error) reject(error);
            else resolve(result);
        });
    });
}
Use code with caution.

4. GAMELIFT SERVER-SIDE INGESTION (C++)
Once the GameLift server receives the confirmation payload via its local AWS SDK network socket listener, it uses the player's unique PlayFabId or PlayerSessionId to locate their in-game character avatar and swap the assets safely.
cpp
// Executed on the GameLift Dedicated Server when PlayFab pushes a validated checkout event
void AHubGameMode::OnCosmeticCheckoutVerified(FString PlayFabId, TMap<FString, FString> ValidatedEquipment)
{
    // 1. Locate the player state using the validated PlayFabId index mapping
    AHubPlayerState* TargetPlayerState = GetPlayerStateByPlayFabId(PlayFabId);
    if (!TargetPlayerState) return;

    AHubCharacter* TargetCharacter = Cast<AHubCharacter>(TargetPlayerState->GetPawn());
    if (!TargetCharacter) return;

    // 2. Distribute the new verified cosmetic payload down to the character class instance
    TargetCharacter->ApplyVerifiedCosmetics(ValidatedEquipment);

    // 3. Trigger a single network replication pulse to notify nearby hub clients of visual update
    TargetCharacter->ForceReplicationUpdate();
}

// Inside AHubCharacter Class: Applying the skeletal asset updates
void AHubCharacter::ApplyVerifiedCosmetics(const TMap<FString, FString>& ValidatedEquipment)
{
    if (GetNetMode() == NM_DedicatedServer || IsLocallyControlled())
    {
        // Extract weapon skin data or accessory asset IDs safely
        if (ValidatedEquipment.Contains(TEXT("Slot_Mask")))
        {
            FString MaskItemID = ValidatedEquipment[TEXT("Slot_Mask")];
            USkeletalMesh* NewMaskMesh = LoadCosmeticMeshFromCatalog(MaskItemID);
            
            // Swap out structural mesh component
            MaskMeshComponent->SetSkeletalMesh(NewMaskMesh);
        }
        
        // Process downstream asset slots (Jewelry, Weapon Wheel Holster skins, etc.)
    }
}
Use code with caution.

5. RECOVERY & FALLBACK MATRIX
Because network drops can happen during transactions, implement this fallback mechanism in your C++ initialization loops:
On Map Join: When a player first logs into the 1,000-player Military Base map, the GameLift game mode must execute a client-read via the PlayFab SDK to download the stored "CurrentEquippedHubCosmetics" JSON block.
State Recovery: This ensures that if a player experiences a hard disconnect directly after purchasing an item in the Labs, their updated character model will load correctly on re-entry without losing data.

### Operational Cost Assessment: 1,000+ Concurrent User (CCU) Military Base Hub
Hosting a 1,000-player persistent Hub using this decoupled architecture will cost between $130 and $430 per month on Amazon GameLift, while the EOS Sub-Lobby and friend layers operate completely free. [1, 2]
Because the heavy shopping, previews, loadouts, and mirror rendering are handled entirely client-side, the GameLift server only processes lightweight movement and player presence. [1]

📊 Monthly Cost Breakdown by Service Layer
Infrastructure Layer
Service / Provider
Monthly Cost (Baseline)
Cost Mechanics & Notes
1,000-CCU Hub Servers
Amazon GameLift Servers (Compute)
$128 – $426 / mo
16-vCPU instance (c6g.4xlarge / c7g.4xlarge Graviton Linux)
Network Egress (Bandwidth)
AWS GameLift Gen-6+ Egress
$0.00 (Free)
AWS provides free outbound bandwidth on Gen-6+ instances
Social & Sub-Lobbies
Epic Online Services (EOS)
$0.00 (Free)
Unlimited P2P lobbies, presence, invites, and sub-lobby sessions
Cloud Customization & Store
Microsoft PlayFab (Standard/Pay-as-you-go)
$0.00 – $50 / mo
Core API calls for store purchases are included in the standard tier
Serverless Webhook Sync
AWS Lambda / PlayFab CloudScript
<$5.00 / mo
Pay-per-checkout invocation execution time (<100ms per purchase)
Estimated Total Cost
—
~$130 – $480 / month
Full operation for 1,000 concurrent players 24/7

⚙️ GameLift Compute Cost Deep-Dive (1,000 CCU)
Because standard character tick rates are stripped down from 60Hz to 10–15Hz with aggressive network culling, a single 16-vCPU compute instance (such as the ARM-based c6g.4xlarge / c7g.4xlarge running Amazon Linux 2023) can comfortably host the entire 1,000-player Military Base instance. [1, 2]
  Monthly Compute Cost for 1 Dedicated 1,000-Player Hub (730 hrs/mo):
  ├── On-Demand Pricing ($0.584/hr) ──────────────► $426.32 / month
  ├── 1-Year Savings Plan (~50% Discount) ────────► $213.16 / month
  └── FleetIQ Spot Instances (~70% Discount) ─────► $127.75 / month

💡 How to Achieve Maximum Cost Efficiency
Compile Dedicated Servers for ARM / Linux (Graviton): Always deploy game server binaries on Amazon Linux 2023 using AWS Graviton instances (c6g / c7g). Windows Server instances carry an OS licensing surcharge that nearly doubles hourly compute rates. [1, 2]
Leverage Free Egress on Gen-6+ Fleets: Ensure all instance types in the GameLift fleet are 6th-generation or newer (c6g, c6i, c7g). AWS does not charge outbound data transfer fees for GameLift server traffic on these instances, eliminating what would otherwise be hundreds of dollars in bandwidth costs. [1, 2]
Use GameLift FleetIQ Spot Allocation: Configure GameLift Queues with a Spot-first strategy (70–80% Spot, with On-Demand fallback) via Amazon GameLift FleetIQ. This lowers instance compute costs to ~$0.17/hour without risking mid-session lobby drops. [, 2, 3, 4, 5]
Scale-to-Zero During Low-Traffic Windows: Use GameLift Target-Tracking Autoscaling policies. When concurrency drops overnight in specific regions, the fleet automatically scales down to a minimal warm pool or drops empty hub instances. [1, 2]
Keep Retail Interactions Client-Side: Keep all 3D asset rotations, mirror rendering, and cosmetic attachment previews strictly local on the client. Pushing these interactions onto the dedicated server would necessitate 4–6x more instances to manage the same 1,000-player load.
