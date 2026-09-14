// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace VibeUEPerformanceAnalysis
{
	FString AnalyseTrace(const FString& TraceFile);
	FString AnalyseLogs(const FString& LogFile);
	FString AnalyseBoth(const FString& TraceFile, const FString& LogFile);
	FString MakeStandaloneSessionStem(const FString& RequestedName);
}
