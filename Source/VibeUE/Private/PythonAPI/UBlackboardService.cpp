// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBlackboardService.h"

#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeServiceInternal.h"

TArray<FString> UBlackboardService::ListBlackboards(const FString& DirectoryPath)
{
	return VibeBT::ListAssetsOfClass(UBlackboardData::StaticClass(), DirectoryPath);
}
