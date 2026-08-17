// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cook/AFLCookedAssetRegistry.h"

#include "AFLCore.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"

#if WITH_EDITOR
#include "AssetRegistry/IAssetRegistry.h"
#endif

const FAFLCookedAssetDecl*& AFLGetCookedAssetHead()
{
	// Constant-initialised: this is a null pointer written into the image, not a dynamic
	// initialiser. It is therefore valid before any FAFLCookedAssetDecl constructor runs,
	// in any translation unit, in any module. See the header for why that matters.
	static const FAFLCookedAssetDecl* Head = nullptr;
	return Head;
}

namespace
{
	/** Runtime-composed paths, enrolled via RegisterDynamic. Path -> context. */
	TMap<FString, FString>& GetDynamicRegistrations()
	{
		static TMap<FString, FString> Registrations;
		return Registrations;
	}

	FCriticalSection& GetDynamicLock()
	{
		static FCriticalSection Lock;
		return Lock;
	}

	/** "/Game/A/B.B_C" -> "/Game/A/B". Empty when the path is malformed. */
	FString ToPackageName(const FString& InObjectPath)
	{
		const FSoftObjectPath SoftPath(InObjectPath);
		return SoftPath.GetLongPackageName();
	}

	/** "/Game/A/B" -> "/Game/A", the directory an always-cook entry would name. */
	FString ToPackageDir(const FString& InPackageName)
	{
		int32 SlashIndex = INDEX_NONE;
		return InPackageName.FindLastChar(TEXT('/'), SlashIndex)
			? InPackageName.Left(SlashIndex)
			: InPackageName;
	}

	FString RemedyFor(const FString& InPackageName)
	{
		return FString::Printf(
			TEXT("add to Config/DefaultGame.ini under [/Script/UnrealEd.ProjectPackagingSettings]: ")
			TEXT("+DirectoriesToAlwaysCook=(Path=\"%s\")"),
			*ToPackageDir(InPackageName));
	}

#if WITH_EDITOR
	/** Parse +DirectoriesToAlwaysCook=(Path="...") entries out of the packaging settings. */
	const TArray<FString>& GetAlwaysCookDirs()
	{
		static bool bCached = false;
		static TArray<FString> Dirs;
		if (bCached)
		{
			return Dirs;
		}
		bCached = true;

		TArray<FString> RawEntries;
		GConfig->GetArray(
			TEXT("/Script/UnrealEd.ProjectPackagingSettings"),
			TEXT("DirectoriesToAlwaysCook"),
			RawEntries,
			GGameIni);

		for (const FString& Entry : RawEntries)
		{
			FString ParsedPath;
			if (FParse::Value(*Entry, TEXT("Path="), ParsedPath))
			{
				ParsedPath.RemoveFromEnd(TEXT("/"));
				Dirs.Add(MoveTemp(ParsedPath));
			}
		}
		return Dirs;
	}

	bool IsCoveredByAlwaysCook(const FString& InPackageName)
	{
		const FString PackageDir = ToPackageDir(InPackageName);
		for (const FString& Dir : GetAlwaysCookDirs())
		{
			// Always-cook entries are recursive, so a parent directory covers this package.
			if (PackageDir == Dir || PackageDir.StartsWith(Dir + TEXT("/")))
			{
				return true;
			}
		}
		return false;
	}
#endif // WITH_EDITOR

	/**
	 * @param OutReason  filled with a human-readable failure reason when this returns false.
	 * @return           false when the asset would not survive a cook.
	 */
	bool ValidateOne(const FString& InObjectPath, FString& OutReason)
	{
		const FString PackageName = ToPackageName(InObjectPath);
		if (PackageName.IsEmpty())
		{
			OutReason = TEXT("not a well-formed object path");
			return false;
		}

#if WITH_EDITOR
		// Existence is worthless here: every asset is on disk in the editor, which is exactly
		// why PIE never caught this defect. Check whether the asset would COOK instead.
		if (IsCoveredByAlwaysCook(PackageName))
		{
			return true;
		}

		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry || AssetRegistry->IsLoadingAssets())
		{
			// Mid-scan the referencer set is incomplete; reporting now would raise false
			// errors and teach everyone to ignore this log. The build-time lint covers it.
			OutReason.Reset();
			return true;
		}

		TArray<FName> Referencers;
		AssetRegistry->GetReferencers(FName(*PackageName), Referencers);
		if (Referencers.Num() == 0)
		{
			OutReason = TEXT("no always-cook coverage AND no asset in the project references it, "
			                 "so nothing will pull it into the build");
			return false;
		}
		return true;
#else
		// Cooked build: ask the loader whether the package is actually here, without loading
		// it. This is the precise question the shipped bug answered "no" to.
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			OutReason = TEXT("package is not in this build (not on disk or in the loader)");
			return false;
		}
		return true;
#endif
	}
}

int32 FAFLCookedAssetRegistry::ValidateAll(const TCHAR* Reason)
{
	int32 Checked = 0;
	int32 Failed = 0;

	auto Report = [&Failed](const FString& ObjectPath, const FString& Site)
	{
		FString FailureReason;
		if (ValidateOne(ObjectPath, FailureReason))
		{
			return;
		}
		++Failed;

		const FString PackageName = ToPackageName(ObjectPath);
		UE_LOG(LogAFLCore, Error,
			TEXT("AFL_COOK: '%s' will not be in the packaged build -- %s. Declared at %s. %s"),
			*ObjectPath,
			*FailureReason,
			*Site,
			PackageName.IsEmpty() ? TEXT("Fix the path.") : *RemedyFor(PackageName));
	};

	for (const FAFLCookedAssetDecl* Decl = AFLGetCookedAssetHead(); Decl; Decl = Decl->Next)
	{
		++Checked;
		Report(FString(Decl->Path), FString::Printf(TEXT("%hs:%d"), Decl->File, Decl->Line));
	}

	{
		FScopeLock Lock(&GetDynamicLock());
		for (const TPair<FString, FString>& Pair : GetDynamicRegistrations())
		{
			++Checked;
			Report(Pair.Key, Pair.Value);
		}
	}

	if (Failed > 0)
	{
		UE_LOG(LogAFLCore, Error,
			TEXT("AFL_COOK: %d of %d string-referenced asset(s) would be MISSING from a packaged "
			     "build (%s). Each one fails silently at use time and cannot reproduce in PIE. "
			     "Run: python Tools/AFL_Lint/cook_soft_refs.py --root ."),
			Failed, Checked, Reason);

#if !UE_BUILD_SHIPPING
		// Development cooked clients are what acceptance actually runs, and an Error alone has
		// already proven easy to miss in a log of thousands of lines. One ensure per sweep --
		// not per asset -- so it is impossible to overlook without being spam.
		ensureAlwaysMsgf(false,
			TEXT("AFL_COOK: %d string-referenced asset(s) missing from this build. See the "
			     "LogAFLCore errors above for each path and its DirectoriesToAlwaysCook remedy."),
			Failed);
#endif
	}
	else
	{
		UE_LOG(LogAFLCore, Log,
			TEXT("AFL_COOK: %d string-referenced asset(s) validated, all present (%s)."),
			Checked, Reason);
	}

	return Failed;
}

void FAFLCookedAssetRegistry::RegisterDynamic(const FString& InPath, const FString& InContext)
{
	if (InPath.IsEmpty())
	{
		return;
	}

	FScopeLock Lock(&GetDynamicLock());
	if (!GetDynamicRegistrations().Contains(InPath))
	{
		GetDynamicRegistrations().Add(InPath, InContext);
	}
}

int32 FAFLCookedAssetRegistry::NumDeclared()
{
	int32 Count = 0;
	for (const FAFLCookedAssetDecl* Decl = AFLGetCookedAssetHead(); Decl; Decl = Decl->Next)
	{
		++Count;
	}
	return Count;
}

// On-demand sweep, so a cooked build can be interrogated without hunting the startup log.
// Fits the existing headless convention: UnrealEditor-Cmd ... -ExecCmds="afl.CookedAssets.Validate".
static FAutoConsoleCommand GAFLValidateCookedAssetsCmd(
	TEXT("afl.CookedAssets.Validate"),
	TEXT("Resolve every string-referenced asset path declared via AFL_COOKED_ASSET and report "
	     "any that would be absent from a packaged build."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		const int32 Failed = FAFLCookedAssetRegistry::ValidateAll(TEXT("console"));
		UE_LOG(LogAFLCore, Display, TEXT("AFL_COOK: console sweep complete, %d failure(s)."), Failed);
	}));
