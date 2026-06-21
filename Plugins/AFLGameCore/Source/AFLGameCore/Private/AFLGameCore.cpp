// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLGameCore.h"

#include "Modules/ModuleManager.h"

// A plain runtime module with no startup logic -- it exists only to be loaded at Default phase so its
// game-framework classes (AAFLGameMode) are registered before any map's world init needs them.
IMPLEMENT_MODULE(FDefaultModuleImpl, AFLGameCore);
