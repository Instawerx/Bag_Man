// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Online/AFLCareerSubsystem.h"

#include "AFLW_CareerRank.generated.h"

class UCommonTextBlock;

/**
 * UAFLW_CareerRank -- the Career hub's RANK tab. The two OpenSkill ladders (R64/R65).
 *
 * ══ TWO SLOTS, HARDCODED, AND THAT IS CORRECT HERE ════════════════════════════════════════════════════
 *
 * Everywhere else in this front end a fixed set got a typed enum and a routing table. This one gets two
 * named slots because R64 does not say "some ladders" -- it says TWO, one per RULESET, and R82 spells out
 * why currency cannot multiply them: "a player has two ratings, not four". A third ladder would be a
 * ruling, not a data change, and it should require touching this file.
 *
 * ⚠ UNPLACED IS NOT ZERO. `FAFLCareerLadder::Rating` is INDEX_NONE for a ruleset the player has never
 * touched, and this screen must say so in words rather than printing a 0 next to the word RANK. The
 * server preserves the distinction as `null`, the subsystem preserves it as INDEX_NONE, and this is the
 * last place it can be thrown away.
 *
 * ⚠ NO VOLUME HERE. R10 keeps cumulative volume and skill rating apart -- "volume rewards attendance,
 * rating measures strength" -- so matches played appears as CONTEXT for a rating, never as a score.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_CareerRank : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Rating as a player should read it. Static so both ladders and any future surface word it once. */
	UFUNCTION(BlueprintPure, Category = "AFL|Career")
	static FText FormatRating(const FAFLCareerLadder& Ladder);

	/** The line under the rating: matches played, or the invitation when there are none. */
	UFUNCTION(BlueprintPure, Category = "AFL|Career")
	static FText FormatContext(const FAFLCareerLadder& Ladder);

protected:
	virtual void NativeOnActivated() override;

	/** Blueprint hook for the ladder treatment once real data lands. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Career", meta = (DisplayName = "On Career Loaded"))
	void BP_OnCareerLoaded(const FAFLCareer& Career);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonTextBlock> BR_Rating;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonTextBlock> BR_Context;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonTextBlock> MP_Rating;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonTextBlock> MP_Context;

	/** Shown instead of ratings when the fetch failed. Never a zero standing in for an unknown. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonTextBlock> StatusLine;

private:
	void RequestCareer();
	void Apply();

	FAFLCareer CachedCareer;
};
