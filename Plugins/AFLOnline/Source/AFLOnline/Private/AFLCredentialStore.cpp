// Copyright C12 AI Gaming. All Rights Reserved.
#include "AFLCredentialStore.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <wincrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "Crypt32.lib")
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAFLCredentials, Log, All);

const TCHAR* FAFLCredentialStore::KeyGameRefreshToken = TEXT("game.refresh");
const TCHAR* FAFLCredentialStore::KeyDeviceId = TEXT("device.id");
const TCHAR* FAFLCredentialStore::KeyDeviceSecret = TEXT("device.secret");

bool FAFLCredentialStore::IsAvailable()
{
#if PLATFORM_WINDOWS
	return true;
#else
	return false;
#endif
}

FString FAFLCredentialStore::StorePath()
{
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("IRONICS"), TEXT("identity.bin"));
}

#if PLATFORM_WINDOWS
static bool AFLDpapiSeal(const TArray<uint8>& Plain, TArray<uint8>& OutSealed)
{
	DATA_BLOB In; In.pbData = const_cast<BYTE*>(Plain.GetData()); In.cbData = static_cast<DWORD>(Plain.Num());
	DATA_BLOB Out = {};
	if (!CryptProtectData(&In, L"IRONICS identity", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
	{
		return false;
	}
	OutSealed.SetNumUninitialized(static_cast<int32>(Out.cbData));
	FMemory::Memcpy(OutSealed.GetData(), Out.pbData, Out.cbData);
	LocalFree(Out.pbData);
	return true;
}

static bool AFLDpapiOpen(const TArray<uint8>& Sealed, TArray<uint8>& OutPlain)
{
	DATA_BLOB In; In.pbData = const_cast<BYTE*>(Sealed.GetData()); In.cbData = static_cast<DWORD>(Sealed.Num());
	DATA_BLOB Out = {};
	if (!CryptUnprotectData(&In, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
	{
		return false;
	}
	OutPlain.SetNumUninitialized(static_cast<int32>(Out.cbData));
	FMemory::Memcpy(OutPlain.GetData(), Out.pbData, Out.cbData);
	// Scrub the DPAPI buffer before releasing it -- it held the plaintext store.
	FMemory::Memzero(Out.pbData, Out.cbData);
	LocalFree(Out.pbData);
	return true;
}
#endif

bool FAFLCredentialStore::ReadAll(TMap<FString, FString>& Out)
{
	Out.Reset();
#if PLATFORM_WINDOWS
	TArray<uint8> Sealed;
	if (!FFileHelper::LoadFileToArray(Sealed, *StorePath()) || Sealed.Num() == 0)
	{
		return false;
	}
	TArray<uint8> Plain;
	if (!AFLDpapiOpen(Sealed, Plain))
	{
		UE_LOG(LogAFLCredentials, Warning, TEXT("[AFLCredentials] store could not be opened (different user or machine?) -- treating as empty."));
		return false;
	}
	FString Json;
	FFileHelper::BufferToString(Json, Plain.GetData(), Plain.Num());
	FMemory::Memzero(Plain.GetData(), Plain.Num());
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		return false;
	}
	for (const auto& Pair : Obj->Values)
	{
		FString V;
		if (Pair.Value.IsValid() && Pair.Value->TryGetString(V))
		{
			Out.Add(Pair.Key, V);
		}
	}
	return true;
#else
	return false;
#endif
}

bool FAFLCredentialStore::WriteAll(const TMap<FString, FString>& In)
{
#if PLATFORM_WINDOWS
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	for (const auto& Pair : In)
	{
		Obj->SetStringField(Pair.Key, Pair.Value);
	}
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Obj, Writer);

	const FTCHARToUTF8 Utf8(*Json);
	TArray<uint8> Plain;
	Plain.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	TArray<uint8> Sealed;
	const bool bSealed = AFLDpapiSeal(Plain, Sealed);
	FMemory::Memzero(Plain.GetData(), Plain.Num());
	if (!bSealed)
	{
		UE_LOG(LogAFLCredentials, Error, TEXT("[AFLCredentials] DPAPI seal failed -- nothing written."));
		return false;
	}
	const FString Path = StorePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree=*/true);
	if (!FFileHelper::SaveArrayToFile(Sealed, *Path))
	{
		UE_LOG(LogAFLCredentials, Error, TEXT("[AFLCredentials] could not write the store."));
		return false;
	}
	return true;
#else
	return false;
#endif
}

FString FAFLCredentialStore::Load(const TCHAR* Key)
{
	TMap<FString, FString> All;
	if (!ReadAll(All))
	{
		return FString();
	}
	const FString* V = All.Find(Key);
	return V ? *V : FString();
}

bool FAFLCredentialStore::Save(const TCHAR* Key, const FString& Value)
{
	if (!IsAvailable())
	{
		UE_LOG(LogAFLCredentials, Log, TEXT("[AFLCredentials] no sealed store on this platform -- '%s' not persisted."), Key);
		return false;
	}
	TMap<FString, FString> All;
	ReadAll(All);
	All.Add(Key, Value);
	const bool bOk = WriteAll(All);
	UE_LOG(LogAFLCredentials, Log, TEXT("[AFLCredentials] '%s' %s (%d chars)."), Key, bOk ? TEXT("stored") : TEXT("NOT stored"), Value.Len());
	return bOk;
}

void FAFLCredentialStore::Remove(const TCHAR* Key)
{
	TMap<FString, FString> All;
	if (!ReadAll(All) || !All.Contains(Key))
	{
		return;
	}
	All.Remove(Key);
	WriteAll(All);
	UE_LOG(LogAFLCredentials, Log, TEXT("[AFLCredentials] '%s' removed."), Key);
}
