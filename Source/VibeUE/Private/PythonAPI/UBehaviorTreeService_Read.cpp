// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "AIGraphTypes.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTreeServiceInternal.h"

TArray<FBTNodeClassInfo> UBehaviorTreeService::GetAvailableNodeTypes(const FString& Category)
{
	UClass* BaseClass = nullptr;
	if (Category == TEXT("Composite"))
	{
		BaseClass = UBTCompositeNode::StaticClass();
	}
	else if (Category == TEXT("Task"))
	{
		BaseClass = UBTTaskNode::StaticClass();
	}
	else if (Category == TEXT("Decorator"))
	{
		BaseClass = UBTDecorator::StaticClass();
	}
	else if (Category == TEXT("Service"))
	{
		BaseClass = UBTService::StaticClass();
	}

	TArray<FBTNodeClassInfo> Result;
	if (!BaseClass)
	{
		return Result;
	}

	const TSharedPtr<FGraphNodeClassHelper> Helper = VibeBT::GetClassHelper(BaseClass);
	if (!Helper.IsValid())
	{
		return Result;
	}

	TArray<FGraphNodeClassData> ClassData;
	Helper->GatherClasses(BaseClass, ClassData);

	Result.Reserve(ClassData.Num());
	for (FGraphNodeClassData& Data : ClassData)
	{
		FBTNodeClassInfo Info;
		Info.ClassName = Data.GetClassName();
		Info.Category = Data.GetCategory().ToString();

		// GetClass() loads the class if it isn't already resident — that's the price of
		// knowing bIsBlueprint, since only the resolved UClass exposes ClassGeneratedBy.
		UClass* ResolvedClass = Data.GetClass(/*bSilent=*/true);
		Info.bIsBlueprint = ResolvedClass && ResolvedClass->ClassGeneratedBy != nullptr;
		Info.ClassPath = ResolvedClass ? ResolvedClass->GetPathName() : Data.GetPackageName();

		Result.Add(MoveTemp(Info));
	}
	return Result;
}
