// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UGameplayTagService.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameplayTagService, Log, All);

// =================================================================
// Tag value lookup (editor-scripting escape hatch)
// =================================================================

FGameplayTag UGameplayTagService::RequestTag(const FString& TagName)
{
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), /*ErrorIfNotFound=*/false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogGameplayTagService, Warning, TEXT("RequestTag: '%s' is not a registered gameplay tag (returning invalid tag)"), *TagName);
	}
	return Tag;
}

FGameplayTagContainer UGameplayTagService::RequestTagContainer(const TArray<FString>& TagNames)
{
	FGameplayTagContainer Container;
	for (const FString& TagName : TagNames)
	{
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*TagName), /*ErrorIfNotFound=*/false);
		if (Tag.IsValid())
		{
			Container.AddTag(Tag);
		}
		else
		{
			UE_LOG(LogGameplayTagService, Warning, TEXT("RequestTagContainer: skipping unregistered tag '%s'"), *TagName);
		}
	}
	return Container;
}
