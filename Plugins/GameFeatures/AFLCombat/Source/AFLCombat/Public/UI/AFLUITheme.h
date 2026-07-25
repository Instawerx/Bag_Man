// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * AFLUITheme -- the SINGLE SOURCE for AFL UI chrome colours.
 *
 * WHY THIS EXISTS: these six tokens were `static const FLinearColor` literals buried inside
 * AFLW_FrontEndMarket.cpp. The weapon wheel needs the same values, and copying them into a second .cpp
 * is exactly the drift that produced the "6 edges render blue" saga on the cosmetic side -- two places
 * holding what should be one value, silently diverging. Extracted BEFORE the wheel copies anything.
 *
 * SCOPE -- read this before adding a token:
 *   These are UI CHROME (frame, rail, tabs, currency). They are NOT player-selectable identities and must
 *   NOT go in UAFLColorIdentityRegistry. The split is deliberate:
 *     - CHROME  (here)                  -> the app's own furniture; same for every player.
 *     - IDENTITY (the colour registry)  -> what a player picked / a brand owns; resolved BY TAG.
 *   A widget tinting a per-ITEM colour must still resolve it through the registry
 *   (UAFLCosmeticCatalogSubsystem::GetEntryPrimaryColor / ResolveColorIdentity), never from here.
 *
 * NOTE on Volts blue: #1E5AFF is the VOLTS CURRENCY colour. It is deliberately NOT in the identity
 * registry, and it is a DIFFERENT blue from Cosmetic.Identity.NeonBlue (#00ADFF, the free base colour)
 * and from Cosmetic.Identity.IRONICS (#5090FF, the house/ground-zero default). Three distinct blues with
 * three distinct owners -- do not conflate them.
 */
namespace AFLUITheme
{
	/** Volts currency neon (primary economy token). #1E5AFF */
	static const FLinearColor VoltsNeon(0.1176f, 0.3529f, 1.0f, 1.0f);

	/** Watts currency neon (secondary economy token). #FF2D9E */
	static const FLinearColor WattsNeon(1.0f, 0.1765f, 0.6196f, 1.0f);

	/** Active tab / selected label. */
	static const FLinearColor CyanActive(0.15f, 0.95f, 1.0f, 1.0f);

	/** Inactive tab / de-emphasised label. */
	static const FLinearColor DimInactive(0.62f, 0.68f, 0.80f, 1.0f);

	/** Active tab background fill (intentionally low alpha). */
	static const FLinearColor TabFill(0.05f, 0.55f, 0.72f, 0.30f);

	/** Panel / rail glass base -- #05080F gloss-black at 0.93. Matches the product cards. */
	static const FLinearColor GlossBlackRail(0.0196f, 0.0314f, 0.0588f, 0.93f);
}
