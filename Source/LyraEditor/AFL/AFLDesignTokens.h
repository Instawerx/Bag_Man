// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FAFLDesignTokens -- the token set lifted VERBATIM out of the approved design pages.
 *
 * ⚠ THE PAGES ARE THE SOURCE OF TRUTH, NOT THIS STRUCT. Every field here is filled by parsing the CSS custom
 * properties in Docs/design/IRONICS_*.html. Nothing is typed in by hand, because a hand-copied hex is a value
 * that silently stops matching the design the first time the design moves -- and the whole reason this
 * compiler exists is that the front end must BE the approved pages rather than resemble them.
 *
 * If a token is missing from the page, it is missing here too. The compiler reports it rather than inventing
 * a plausible default: a made-up colour that looks right is the most expensive kind of wrong, because nobody
 * goes looking for it.
 */
struct FAFLDesignTokens
{
	/** Raw `--name` -> value, exactly as written in the page's `:root{}` block. */
	TMap<FString, FString> Raw;

	/** Where these came from, for the report and for the generated assets' provenance comment. */
	FString SourceFile;

	/** Every `--name` the page declared but this compiler does not consume. Reported, never silently dropped. */
	TArray<FString> Unconsumed;

	/** Resolve a token to a colour. Handles #RGB / #RRGGBB / #RRGGBBAA and rgba(r,g,b,a). */
	bool TryGetColor(const FString& TokenName, FLinearColor& OutColor) const;

	/** Resolve a token to a scalar, tolerating a `px` suffix. */
	bool TryGetScalar(const FString& TokenName, float& OutValue) const;

	/** Resolve a token to its raw string (font stacks, gradients -- things with no numeric form). */
	bool TryGetString(const FString& TokenName, FString& OutValue) const;

	/** First present token wins. The pages disagree slightly on names (`--electric` vs `--house-electric`). */
	bool TryGetColorAny(const TArray<FString>& TokenNames, FLinearColor& OutColor) const;
	bool TryGetScalarAny(const TArray<FString>& TokenNames, float& OutValue) const;
};

/**
 * Parses the `:root{ --name:value; }` blocks out of a design page.
 *
 * Deliberately a CSS-custom-property reader and nothing more -- not a CSS engine. The pages declare their
 * whole vocabulary as custom properties precisely so it can be lifted mechanically; anything requiring real
 * cascade resolution is a signal the page has drifted from that contract, and is reported rather than
 * guessed at.
 */
class FAFLDesignTokenParser
{
public:
	/** Reads the file and fills Out. Returns false (with OutError) if the file is unreadable or has no :root. */
	static bool ParseFile(const FString& AbsoluteHtmlPath, FAFLDesignTokens& Out, FString& OutError);

	/** Same, for content already in memory (used by the tests). */
	static bool ParseString(const FString& Css, FAFLDesignTokens& Out, FString& OutError);
};
