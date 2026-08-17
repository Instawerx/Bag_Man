// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"   // TSoftObjectPtr / TSoftClassPtr, returned by the helpers below

/**
 * AFL COOKED-ASSET REGISTRY -- runtime half of the guard against string-referenced assets
 * being silently dropped from a packaged build.
 *
 * THE DEFECT. The cooker packages what something REFERENCES. An FSoftObjectPath or
 * TSoftClassPtr built from a C++ string literal is not a reference the cooker can follow, so
 * the asset is never packaged. The failure CANNOT REPRODUCE IN PIE -- every asset is on disk
 * there, so LoadSynchronous always succeeds -- and in a cooked build it surfaces only as a
 * LogStreaming Warning buried in thousands of lines. It shipped three times. FBIK, doctrine on
 * every character, had never once been in a packaged build.
 *
 * WHERE THE REAL GATE IS. Tools/AFL_Lint/cook_soft_refs.py, run per-PR. It scans source for
 * these literals and fails the build when one is not covered by DirectoriesToAlwaysCook, in
 * seconds, with no engine and no cook. Prefer it: a runtime check still requires somebody to
 * produce a cooked build and read its log, which is precisely the step that failed twice.
 *
 * WHAT THIS ADDS THAT A SOURCE SCAN CANNOT.
 *   - Paths composed at runtime (FString::Printf), which no static scan can resolve.
 *   - Cook drift between targets. The lint checks INTENT (config); this checks REALITY on the
 *     artifact that actually shipped, per target -- client and dedicated server cook differently
 *     and the known bug reached clients only.
 *
 * TIMING, HONESTLY. AFLCore is a GameFeature plugin (ExplicitlyLoaded, initial state
 * Registered), so its module starts on GameFeature load -- experience-load time, not engine
 * init. That is the earliest point reachable from both offending modules, since AFLMovement
 * does not depend on AFLGameCore and AFLCore is their only common base. It is still far earlier
 * than either observed failure: site 1 fired at match end, minutes in; site 2 at character
 * BeginPlay. Registration itself is not activation-tied -- in a monolithic cooked build every
 * declaration below is linked in before main(), so the sweep sees the complete set.
 *
 * DECLARATION IS REGISTRATION. AFL_COOKED_ASSET declares the path and enrols it in one line;
 * there is no second step to forget.
 *
 *     AFL_COOKED_ASSET(GProModFBIK, TEXT("/Game/BagMan/ProMod/ABP_ProMod_FBIK_PP.ABP_ProMod_FBIK_PP_C"));
 *     ...
 *     PostProcessABP = GProModFBIK.ToSoftClassPtr<UAnimInstance>();
 */

struct FAFLCookedAssetDecl;

/**
 * Head of the intrusive declaration list.
 *
 * Returned by function rather than exported as a data symbol so the storage is
 * constant-initialised (a null pointer, zero-initialised before ANY dynamic initialisation runs
 * anywhere in the program). That is what makes registration immune to the static
 * initialisation order fiasco: a declaration in AFLMovement can link itself in before AFLCore's
 * own dynamic initialisers have run, and still find a valid head.
 */
AFLCORE_API const FAFLCookedAssetDecl*& AFLGetCookedAssetHead();

/**
 * One declared asset path. Deliberately a POD holding raw pointers to string literals:
 * constructed during static initialisation, where FString, FName and FMemory are not yet
 * guaranteed usable. No allocation, no engine types, no work beyond a pointer swap.
 */
struct FAFLCookedAssetDecl
{
	/** Full object path exactly as written, e.g. "/Game/A/B.B_C". Points at a string literal. */
	const TCHAR* Path = nullptr;

	/** Declaration site, for an actionable log line. __FILE__ is ANSI. */
	const ANSICHAR* File = nullptr;
	int32 Line = 0;

	const FAFLCookedAssetDecl* Next = nullptr;

	FAFLCookedAssetDecl(const TCHAR* InPath, const ANSICHAR* InFile, int32 InLine)
		: Path(InPath)
		, File(InFile)
		, Line(InLine)
	{
		// Static initialisation is single-threaded, so an unguarded push is safe here.
		const FAFLCookedAssetDecl*& Head = AFLGetCookedAssetHead();
		Next = Head;
		Head = this;
	}

	/** Safe from any runtime context; NOT safe to call during static initialisation. */
	FSoftObjectPath ToSoftObjectPath() const { return FSoftObjectPath(Path); }

	template <typename T>
	TSoftClassPtr<T> ToSoftClassPtr() const { return TSoftClassPtr<T>(ToSoftObjectPath()); }

	template <typename T>
	TSoftObjectPtr<T> ToSoftObjectPtr() const { return TSoftObjectPtr<T>(ToSoftObjectPath()); }
};

/**
 * Declare a cooked-asset path AND enrol it for validation, in one statement.
 *
 * USE AT FILE SCOPE. A function-scope static is initialised on first execution, not at load, so
 * a declaration inside a function that has not run yet would be invisible to the startup sweep --
 * which is the entire point of enrolling it. File scope registers before main() in a cooked
 * (monolithic) build and at module load in the editor.
 *
 * GIVE EACH ONE A NAME UNIQUE WITHIN ITS MODULE. The declaration has internal linkage, but unity
 * builds merge translation units, so two files in the same module using the same Name collide at
 * compile time rather than at link time.
 */
#define AFL_COOKED_ASSET(Name, PathLiteral) \
	static const FAFLCookedAssetDecl Name(PathLiteral, __FILE__, __LINE__)

/** Validation entry points. */
struct AFLCORE_API FAFLCookedAssetRegistry
{
	/**
	 * Resolve every declared path and log an ERROR naming the asset and the
	 * DirectoriesToAlwaysCook remedy for each one that would not survive a cook.
	 *
	 * Cooked builds  -- asks the loader whether the package exists, without loading it. This is
	 *                   exactly the question the shipped bug answered "no" to.
	 * Editor builds  -- existence is meaningless (everything is on disk, which is why PIE never
	 *                   caught this). Checks cook COVERAGE instead: the path must be under a
	 *                   DirectoriesToAlwaysCook entry, or have at least one referencer in the
	 *                   asset registry. Zero referencers and no coverage is the FBIK case
	 *                   precisely, and is reported as an error in-editor.
	 *
	 * @param Reason  short context for the log line, e.g. TEXT("AFLCore startup").
	 * @return        number of declarations that failed validation.
	 */
	static int32 ValidateAll(const TCHAR* Reason);

	/**
	 * Enrol a path composed at runtime, which no source scan can resolve. Cheap and idempotent;
	 * safe to call on a hot path only in the sense that duplicates are ignored.
	 *
	 * @param InPath    the fully-composed object path.
	 * @param InContext where it came from, for the log line.
	 */
	static void RegisterDynamic(const FString& InPath, const FString& InContext);

	/** Count of statically declared paths. */
	static int32 NumDeclared();
};
