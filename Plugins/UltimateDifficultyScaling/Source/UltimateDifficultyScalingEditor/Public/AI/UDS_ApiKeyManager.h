// Copyright 2025, BlueprintsLab, All rights reserved
#pragma once

#include "CoreMinimal.h"
#include "AI/UDS_ApiKeySlot.h"

/**
 * Isolated BYO-key store for this plugin. Persists to a per-user JSON file
 * (UserSettingsDir/UltimateDifficultyScaling/api_keys.json) so keys survive across
 * projects but are independent of the author's other plugins.
 */
class FUDS_ApiKeyManager
{
public:
	static FUDS_ApiKeyManager& Get();

	void SetSlot(int32 SlotIndex, const FUDS_ApiKeySlot& Slot);
	FUDS_ApiKeySlot GetSlot(int32 SlotIndex) const;
	void ClearSlot(int32 SlotIndex);
	TArray<FUDS_ApiKeySlot> GetAllSlots() const;

	int32 GetActiveSlotIndex() const;
	void  SetActiveSlot(int32 SlotIndex);
	FUDS_ApiKeySlot GetActiveSlot() const;

	FString GetActiveApiKey() const;
	FString GetActiveProvider() const;
	FString GetActiveModelName() const;

	/** True if any slot has a Provider + ApiKey (or a Custom slot with a base URL). Gates the AI feature. */
	bool HasAnyConfiguredSlot() const;

private:
	FUDS_ApiKeyManager();
	~FUDS_ApiKeyManager();

	FUDS_ApiKeySlot Slots[UDS_MAX_API_KEY_SLOTS];
	int32 ActiveSlotIndex = 0;
	mutable FCriticalSection SlotsLock;

	void LoadFromConfig();
	void SaveToConfig();

	static FString GetConfigFilePath();
};
