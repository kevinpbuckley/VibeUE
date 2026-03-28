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

FGameplayTagInfo UGameplayTagService::BuildTagInfo(const FString& TagName)
{
	FGameplayTagInfo Info;
	Info.TagName = TagName;

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);

	if (Tag.IsValid())
	{
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

TArray<FGameplayTagInfo> UGameplayTagService::ListTags(const FString& Filter)
{
	TArray<FGameplayTagInfo> Result;

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	FGameplayTagContainer AllTags;
	Manager.RequestAllGameplayTags(AllTags, false);

	for (const FGameplayTag& Tag : AllTags)
	{
		FString TagName = Tag.GetTagName().ToString();

		// Apply prefix filter
		if (!Filter.IsEmpty() && !TagName.StartsWith(Filter))
		{
			continue;
		}

		Result.Add(BuildTagInfo(TagName));
	}

	// Sort alphabetically
	Result.Sort([](const FGameplayTagInfo& A, const FGameplayTagInfo& B)
	{
		return A.TagName < B.TagName;
	});

	return Result;
}

bool UGameplayTagService::HasTag(const FString& TagName)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);
	return Tag.IsValid();
}

bool UGameplayTagService::GetTagInfo(const FString& TagName, FGameplayTagInfo& OutInfo)
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

TArray<FGameplayTagInfo> UGameplayTagService::GetChildren(const FString& ParentTag)
{
	TArray<FGameplayTagInfo> Result;

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
	Result.Sort([](const FGameplayTagInfo& A, const FGameplayTagInfo& B)
	{
		return A.TagName < B.TagName;
	});

	return Result;
}

// =================================================================
// Add Operations
// =================================================================

#if WITH_EDITOR

FGameplayTagResult UGameplayTagService::AddTag(const FString& TagName, const FString& Comment, const FString& TagSource)
{
	FGameplayTagResult Result;

	if (TagName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("Tag name cannot be empty");
		return Result;
	}

	// Check if tag already exists
	if (HasTag(TagName))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Tag '%s' already exists"), *TagName);
		return Result;
	}

	// Use the GameplayTags editor module to properly add the tag
	IGameplayTagsEditorModule& EditorModule = IGameplayTagsEditorModule::Get();

	bool bAdded = EditorModule.AddNewGameplayTagToINI(TagName, Comment, FName(*TagSource));

	if (bAdded)
	{
		Result.bSuccess = true;
		Result.TagsModified.Add(TagName);
		UE_LOG(LogGameplayTagService, Log, TEXT("Added gameplay tag: %s (source: %s)"), *TagName, *TagSource);
	}
	else
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to add tag '%s' - editor module rejected the operation"), *TagName);
		UE_LOG(LogGameplayTagService, Error, TEXT("Failed to add gameplay tag: %s"), *TagName);
	}

	return Result;
}

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

// =================================================================
// Modify Operations
// =================================================================

FGameplayTagResult UGameplayTagService::RemoveTag(const FString& TagName)
{
	FGameplayTagResult Result;

	if (TagName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("Tag name cannot be empty");
		return Result;
	}

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);

	if (!Tag.IsValid())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Tag '%s' does not exist"), *TagName);
		return Result;
	}

	TSharedPtr<FGameplayTagNode> Node = Manager.FindTagNode(Tag);
	if (!Node.IsValid())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Tag node not found for '%s'"), *TagName);
		return Result;
	}

	IGameplayTagsEditorModule& EditorModule = IGameplayTagsEditorModule::Get();
	bool bRemoved = EditorModule.DeleteTagFromINI(Node);

	if (bRemoved)
	{
		Result.bSuccess = true;
		Result.TagsModified.Add(TagName);
		UE_LOG(LogGameplayTagService, Log, TEXT("Removed gameplay tag: %s"), *TagName);
	}
	else
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to remove tag '%s' - it may be referenced by assets or be a native tag"), *TagName);
		UE_LOG(LogGameplayTagService, Error, TEXT("Failed to remove gameplay tag: %s"), *TagName);
	}

	return Result;
}

FGameplayTagResult UGameplayTagService::RenameTag(const FString& OldTagName, const FString& NewTagName)
{
	FGameplayTagResult Result;

	if (OldTagName.IsEmpty() || NewTagName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("Both old and new tag names must be provided");
		return Result;
	}

	if (!HasTag(OldTagName))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Tag '%s' does not exist"), *OldTagName);
		return Result;
	}

	if (HasTag(NewTagName))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Target tag '%s' already exists"), *NewTagName);
		return Result;
	}

	IGameplayTagsEditorModule& EditorModule = IGameplayTagsEditorModule::Get();
	bool bRenamed = EditorModule.RenameTagInINI(OldTagName, NewTagName);

	if (bRenamed)
	{
		Result.bSuccess = true;
		Result.TagsModified.Add(OldTagName);
		Result.TagsModified.Add(NewTagName);
		UE_LOG(LogGameplayTagService, Log, TEXT("Renamed gameplay tag: %s -> %s"), *OldTagName, *NewTagName);
	}
	else
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to rename tag '%s' to '%s'"), *OldTagName, *NewTagName);
		UE_LOG(LogGameplayTagService, Error, TEXT("Failed to rename gameplay tag: %s -> %s"), *OldTagName, *NewTagName);
	}

	return Result;
}

#endif // WITH_EDITOR
