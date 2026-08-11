// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBlackboardService.h"

#include "BehaviorTree/BlackboardData.h"

namespace VibeBT
{
	TArray<FString> ListAssetsOfClass(const UClass* Class, const FString& DirectoryPath);
}

TArray<FString> UBlackboardService::ListBlackboards(const FString& DirectoryPath)
{
	return VibeBT::ListAssetsOfClass(UBlackboardData::StaticClass(), DirectoryPath);
}
