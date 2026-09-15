// Copyright C12 AI Gaming. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * FAFLCredentialStore -- the ONE place a player-held secret is persisted on this machine
 * (Identity Program I-2/I-3): the portal refresh token behind STAY SIGNED IN, and the guest
 * device credential behind PLAY NOW.
 *
 * Windows: the whole store is sealed with DPAPI (CryptProtectData, current user, UI forbidden) --
 * readable only by this Windows account on this machine, never a plain file. Other platforms: no
 * persistence (Load returns nothing, Save is a logged no-op) rather than a plaintext fallback; the
 * player simply signs in again. Values are never logged, only their presence and length.
 *
 * File: <UserSettingsDir>/IRONICS/identity.bin -- outside the install so a reinstall keeps the
 * sign-in, and outside Saved/ so a "clear saved data" never silently signs the player out.
 */
class AFLONLINE_API FAFLCredentialStore
{
public:
	static const TCHAR* KeyGameRefreshToken;  // portal refresh token (game audience)
	static const TCHAR* KeyDeviceId;          // guest device id (public half)
	static const TCHAR* KeyDeviceSecret;      // guest device secret (proof)

	/** Read one value. Empty when absent, unreadable, or not supported on this platform. */
	static FString Load(const TCHAR* Key);
	/** Write (or overwrite) one value. Returns false when persistence is unavailable. */
	static bool Save(const TCHAR* Key, const FString& Value);
	/** Remove one value. Idempotent. */
	static void Remove(const TCHAR* Key);
	/** True when this platform can seal secrets at rest (Windows DPAPI). */
	static bool IsAvailable();

private:
	static FString StorePath();
	static bool ReadAll(TMap<FString, FString>& Out);
	static bool WriteAll(const TMap<FString, FString>& In);
};
