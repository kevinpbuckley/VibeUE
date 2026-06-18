// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Skills/UVibeUESkillsToolset.h"
#include "Utils/VibeUEPaths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString GetSkillsDir()
	{
		return FVibeUEPaths::GetPluginContentDir() / TEXT("Skills");
	}

	/** Pull the single-line `description:` value out of a SKILL.md's YAML frontmatter. */
	FString ExtractDescription(const FString& Content)
	{
		if (!Content.StartsWith(TEXT("---")))
		{
			return FString();
		}

		// Frontmatter is delimited by the opening "---" and the next "---" on its own line.
		const int32 CloseIdx = Content.Find(TEXT("\n---"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 3);
		const FString Frontmatter = (CloseIdx != INDEX_NONE) ? Content.Mid(3, CloseIdx - 3) : Content;

		TArray<FString> Lines;
		Frontmatter.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStartAndEnd();
			if (Trimmed.StartsWith(TEXT("description:")))
			{
				return Trimmed.RightChop(12).TrimStartAndEnd().TrimQuotes();
			}
		}
		return FString();
	}

	/** Reject names that could escape the Skills directory. */
	bool IsSafeSkillName(const FString& Name)
	{
		return !Name.IsEmpty() && !Name.Contains(TEXT("..")) && !Name.Contains(TEXT("\\")) && !Name.StartsWith(TEXT("/"));
	}
}

TMap<FString, FString> UVibeUESkillsToolset::ListSkills()
{
	TMap<FString, FString> Skills;
	const FString SkillsDir = GetSkillsDir();

	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> SkillFolders;
	FileManager.FindFiles(SkillFolders, *(SkillsDir / TEXT("*")), /*Files=*/false, /*Directories=*/true);

	for (const FString& Folder : SkillFolders)
	{
		const FString SkillMd = SkillsDir / Folder / TEXT("SKILL.md");
		FString Content;
		if (FFileHelper::LoadFileToString(Content, *SkillMd))
		{
			Skills.Add(Folder, ExtractDescription(Content));
		}
	}

	return Skills;
}

TMap<FString, FString> UVibeUESkillsToolset::LoadSkills(const TArray<FString>& SkillNames)
{
	TMap<FString, FString> Loaded;
	const FString SkillsDir = GetSkillsDir();

	for (const FString& Raw : SkillNames)
	{
		const FString Name = Raw.TrimStartAndEnd();
		if (!IsSafeSkillName(Name))
		{
			Loaded.Add(Raw, FString::Printf(TEXT("Invalid skill name: %s"), *Raw));
			continue;
		}

		// "skill/section" loads a sibling sub-doc; a bare name loads the pack's SKILL.md.
		FString Skill;
		FString Section;
		FString FilePath;
		if (Name.Split(TEXT("/"), &Skill, &Section))
		{
			FilePath = SkillsDir / Skill / (Section + TEXT(".md"));
		}
		else
		{
			FilePath = SkillsDir / Name / TEXT("SKILL.md");
		}

		FString Content;
		if (FFileHelper::LoadFileToString(Content, *FilePath))
		{
			Loaded.Add(Raw, Content);
		}
		else
		{
			Loaded.Add(Raw, FString::Printf(TEXT("Skill not found: %s. Call ListSkills for available packs."), *Raw));
		}
	}

	return Loaded;
}
