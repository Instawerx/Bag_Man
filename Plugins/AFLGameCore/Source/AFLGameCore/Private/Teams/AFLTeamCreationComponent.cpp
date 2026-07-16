// Copyright C12 AI Gaming. All Rights Reserved.

#include "Teams/AFLTeamCreationComponent.h"

#include "AFLGameCore.h"   // LogAFLGameCore

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLTeamCreationComponent)

#if WITH_SERVER_CODE
void UAFLTeamCreationComponent::ServerAssignPlayersToTeams()
{
	// FIRST INCREMENT -- ZERO BEHAVIOR CHANGE: prove the subclass is in the assignment path, then delegate to
	// stock. Overriding ONLY ServerAssignPlayersToTeams (and calling Super) preserves ALL stock behavior --
	// including the base's ServerChooseTeamForPlayer per player and the late-joiner OnPlayerInitialized hook it
	// registers. The next increment replaces this body with the IAFLTeamAssignmentProvider (LocalFill:
	// bot-fill + party-together).
	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLTeams: UAFLTeamCreationComponent in path, delegating to stock"));

	Super::ServerAssignPlayersToTeams();
}
#endif
