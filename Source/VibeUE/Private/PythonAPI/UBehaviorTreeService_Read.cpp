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

		// IsBlueprint() is a free flag check (AssetName.Len() > 0 — set at construction from
		// asset-registry data). Deliberately NOT using GetClass()->ClassGeneratedBy here: that
		// forces a LoadPackage + FullyLoad per Blueprint entry, on every listing call, whose
		// result is then discarded — dozens of blocking loads to answer a yes/no question.
		Info.bIsBlueprint = Data.IsBlueprint();

		if (Info.bIsBlueprint)
		{
			// Full object path built from asset-registry strings alone, so listing performs
			// no loads at all: "<package>.<GeneratedClassName>", e.g.
			// "/Game/AI/BTT_Foo.BTT_Foo_C".
			Info.ClassPath = Data.GetPackageName() + TEXT(".") + Info.ClassName;
		}
		else
		{
			// Native entries were resolved from a TObjectIterator at graph-build time, so the
			// UClass is already resident — GetClass() here is a pointer read, never a load.
			UClass* ResolvedClass = Data.GetClass(/*bSilent=*/true);
			Info.ClassPath = ResolvedClass
				? ResolvedClass->GetPathName()
				: Data.GetPackageName() + TEXT(".") + Info.ClassName;
		}

		Result.Add(MoveTemp(Info));
	}
	return Result;
}
