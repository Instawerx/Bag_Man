// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCosmeticCatalog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCosmeticCatalog)

void UAFLCosmeticCatalog::BuildLookup()
{
	EntryById.Reset();
	EntryById.Reserve(Entries.Num());
	for (const FAFLCatalogEntry& Entry : Entries)
	{
		if (Entry.CosmeticId != NAME_None)
		{
			// Last-wins on a duplicate id; the manifest should not have dupes (author discipline).
			EntryById.Add(Entry.CosmeticId, &Entry);
		}
	}
}

const FAFLCatalogEntry* UAFLCosmeticCatalog::FindEntry(FName CosmeticId) const
{
	if (CosmeticId == NAME_None)
	{
		return nullptr;
	}

	// The accelerator is built on load/PostLoad. Guard the cold case (e.g. an entry queried before
	// PostLoad in some editor flow): if the map is empty but we have entries, fall back to a linear scan
	// so a read never silently misses a present id. (const method -> can't lazily build the member map.)
	if (const FAFLCatalogEntry* const* Found = EntryById.Find(CosmeticId))
	{
		return *Found;
	}

	if (EntryById.Num() == 0 && Entries.Num() > 0)
	{
		for (const FAFLCatalogEntry& Entry : Entries)
		{
			if (Entry.CosmeticId == CosmeticId)
			{
				return &Entry;
			}
		}
	}

	return nullptr;
}

void UAFLCosmeticCatalog::PostLoad()
{
	Super::PostLoad();
	BuildLookup();
}
