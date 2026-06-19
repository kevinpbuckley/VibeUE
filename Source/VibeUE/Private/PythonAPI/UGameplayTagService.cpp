// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UGameplayTagService.h"
#include "GameplayTagsManager.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsSettings.h"
#if WITH_EDITOR
#include "GameplayTagsEditorModule.h"
#endif
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameplayTagService, Log, All);

// =================================================================
// Internal Helpers
// =================================================================

FVibeGameplayTagInfo UGameplayTagService::BuildTagInfo(const FString& TagName)
{
	FVibeGameplayTagInfo Info;
	Info.TagName = TagName;

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);

	if (Tag.IsValid())
	{
		// RequestGameplayTag applies tag redirects (registered by renames). If the resolved
		// tag differs from the requested name, surface that instead of silently describing
		// the redirect target under the old name.
		if (!Tag.GetTagName().IsEqual(FName(*TagName)))
		{
			Info.RedirectedTo = Tag.GetTagName().ToString();
		}

		TSharedPtr<FGameplayTagNode> Node = Manager.FindTagNode(Tag);
		if (Node.IsValid())
		{
			Info.bIsExplicit = Node->IsExplicitTag();
			Info.ChildCount = Node->GetChildTagNodes().Num();

			// Get source name from the tag node
			Info.Source = Node->GetFirstSourceName().ToString();

			// Get developer comment
			Info.Comment = Node->GetDevComment();
		}
	}

	return Info;
}

// =================================================================
// Query Operations
// =================================================================

bool UGameplayTagService::HasTag(const FString& TagName)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);
	return Tag.IsValid();
}

bool UGameplayTagService::GetTagInfo(const FString& TagName, FVibeGameplayTagInfo& OutInfo)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);

	if (!Tag.IsValid())
	{
		return false;
	}

	OutInfo = BuildTagInfo(TagName);
	return true;
}

TArray<FVibeGameplayTagInfo> UGameplayTagService::GetChildren(const FString& ParentTag)
{
	TArray<FVibeGameplayTagInfo> Result;

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// Find the parent tag node
	FGameplayTag Parent = Manager.RequestGameplayTag(FName(*ParentTag), false);
	if (!Parent.IsValid())
	{
		UE_LOG(LogGameplayTagService, Warning, TEXT("Parent tag '%s' not found"), *ParentTag);
		return Result;
	}

	TSharedPtr<FGameplayTagNode> ParentNode = Manager.FindTagNode(Parent);
	if (!ParentNode.IsValid())
	{
		return Result;
	}

	// Get direct children
	for (const TSharedPtr<FGameplayTagNode>& ChildNode : ParentNode->GetChildTagNodes())
	{
		if (ChildNode.IsValid())
		{
			FString ChildTagName = ChildNode->GetCompleteTagName().ToString();
			Result.Add(BuildTagInfo(ChildTagName));
		}
	}

	// Sort alphabetically
	Result.Sort([](const FVibeGameplayTagInfo& A, const FVibeGameplayTagInfo& B)
	{
		return A.TagName < B.TagName;
	});

	return Result;
}

// =================================================================
// Add Operations
// =================================================================

#if WITH_EDITOR

FGameplayTagResult UGameplayTagService::AddTags(const TArray<FString>& TagNames, const FString& Comment, const FString& TagSource)
{
	FGameplayTagResult Result;

	if (TagNames.Num() == 0)
	{
		Result.ErrorMessage = TEXT("Tag names array is empty");
		return Result;
	}

	IGameplayTagsEditorModule& EditorModule = IGameplayTagsEditorModule::Get();
	TArray<FString> Failed;

	for (const FString& TagName : TagNames)
	{
		if (TagName.IsEmpty())
		{
			Failed.Add(TEXT("(empty tag name)"));
			continue;
		}

		if (HasTag(TagName))
		{
			// Skip already-existing tags silently but record them as modified
			Result.TagsModified.Add(TagName);
			continue;
		}

		bool bAdded = EditorModule.AddNewGameplayTagToINI(TagName, Comment, FName(*TagSource));
		if (bAdded)
		{
			Result.TagsModified.Add(TagName);
			UE_LOG(LogGameplayTagService, Log, TEXT("Added gameplay tag: %s"), *TagName);
		}
		else
		{
			Failed.Add(TagName);
			UE_LOG(LogGameplayTagService, Error, TEXT("Failed to add gameplay tag: %s"), *TagName);
		}
	}

	if (Failed.Num() > 0)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to add %d tag(s): %s"), Failed.Num(), *FString::Join(Failed, TEXT(", ")));
		Result.bSuccess = Result.TagsModified.Num() > 0; // Partial success
	}
	else
	{
		Result.bSuccess = true;
	}

	return Result;
}

#endif // WITH_EDITOR
