// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/AgentSkill.h"
#include "UPerformanceTriageSkill.generated.h"

/**
 * Native AgentSkill that teaches the AI assistant HOW to use VibeUE's performance tools — the strategy
 * layer on top of the per-tool descriptions on UPerformanceService. Ships with the plugin: because
 * ToolsetRegistry discovers skills by scanning UAgentSkill subclasses (native classes included) and the
 * default allow/block lists are empty, this is auto-registered on plugin load with zero project setup. It
 * shows up in ListSkills and the assistant reads its Instructions via GetSkills.
 *
 * Description/Instructions are set on the CDO in the constructor, which is exactly what the read path
 * (UAgentSkillToolset::ListSkills / GetSkills) inspects.
 */
UCLASS()
class UPerformanceTriageSkill : public UAgentSkill
{
	GENERATED_BODY()

public:
	UPerformanceTriageSkill();
};
