// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLNetTypes.h"

#include "Modules/ModuleManager.h"

// A plain runtime module with no startup logic -- it exists only to be loaded at Default
// phase so its replicated USTRUCTs are registered before GAS builds its struct cache.
IMPLEMENT_MODULE(FDefaultModuleImpl, AFLNetTypes);
