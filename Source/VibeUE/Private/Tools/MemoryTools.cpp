// Copyright Buckley Builds LLC 2026 All Rights Reserved.

/**
 * Memory Tool - Persistent, cross-session memory for the VibeUE in-editor chat.
 *
 * Mirrors Anthropic's client-side "memory" tool interface so the model can use
 * the commands it is already trained on (view / create / str_replace / insert /
 * delete / rename). Files are stored on disk under the project's Saved folder
 * (Project/Saved/VibeUE/Memory), so memories persist across editor sessions.
 *
 * This tool is registered as INTERNAL-ONLY (bInternalOnly = true) and is therefore
 * NOT exposed via MCP to external clients (e.g. VS Code Copilot).
 *
 * Model-facing paths use the "/memories" convention (e.g. "/memories/notes.md");
 * they are resolved against the memory root and validated to prevent directory
 * traversal outside that root.
 *
 * Writes are gated by prompt policy (see vibeue.instructions.md): the model must
 * only create/edit/delete memory when the user explicitly asks it to.
 */

#include "Core/ToolRegistry.h"
#include "Utils/VibeUEPaths.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMemoryTools, Log, All);
DEFINE_LOG_CATEGORY(LogMemoryTools);

namespace MemoryToolHelpers
{
	/** Maximum number of lines a file may have before view refuses to read it (matches Claude's contract). */
	static constexpr int32 MaxFileLines = 999999;

	/** Format a byte count as a short human-readable size (e.g. "512B", "1.5K", "2.0M"). */
	static FString HumanSize(int64 Bytes)
	{
		if (Bytes < 1024)
		{
			return FString::Printf(TEXT("%lldB"), Bytes);
		}
		const double KB = static_cast<double>(Bytes) / 1024.0;
		if (KB < 1024.0)
		{
			return FString::Printf(TEXT("%.1fK"), KB);
		}
		const double MB = KB / 1024.0;
		return FString::Printf(TEXT("%.1fM"), MB);
	}

	/** Canonical absolute path of the memory root (directory is created if missing). */
	static FString GetRoot()
	{
		return FPaths::ConvertRelativePathToFull(FVibeUEPaths::GetMemoryDir());
	}

	/**
	 * Resolve a model-supplied path to an absolute path inside the memory root.
	 * Accepts "/memories", "/memories/foo.md", "memories/foo.md" or a bare relative
	 * path. Collapses "." / ".." and rejects anything that escapes the root.
	 */
	static bool ResolvePath(const FString& InPath, FString& OutAbsPath, FString& OutError)
	{
		const FString Root = GetRoot();

		FString Rel = InPath;
		Rel.ReplaceInline(TEXT("\\"), TEXT("/"));
		Rel.TrimStartAndEndInline();

		// Strip the "/memories" convention prefix so paths map onto our root.
		if (Rel.Equals(TEXT("/memories"), ESearchCase::IgnoreCase) || Rel.Equals(TEXT("memories"), ESearchCase::IgnoreCase))
		{
			Rel.Empty();
		}
		else if (Rel.StartsWith(TEXT("/memories/"), ESearchCase::IgnoreCase))
		{
			Rel = Rel.RightChop(10); // len("/memories/")
		}
		else if (Rel.StartsWith(TEXT("memories/"), ESearchCase::IgnoreCase))
		{
			Rel = Rel.RightChop(9); // len("memories/")
		}

		// Treat what remains as relative to the root.
		while (Rel.StartsWith(TEXT("/")))
		{
			Rel = Rel.RightChop(1);
		}

		FString Candidate = Rel.IsEmpty() ? Root : (Root / Rel);
		Candidate = FPaths::ConvertRelativePathToFull(Candidate); // collapses ".." segments

		// Path-traversal guard: candidate must be the root itself or live under it.
		FString RootWithSlash = Root;
		if (!RootWithSlash.EndsWith(TEXT("/")))
		{
			RootWithSlash += TEXT("/");
		}
		if (!Candidate.Equals(Root, ESearchCase::IgnoreCase) && !Candidate.StartsWith(RootWithSlash, ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("Error: The path %s is outside the memory directory and is not allowed."), *InPath);
			return false;
		}

		OutAbsPath = Candidate;
		return true;
	}

	/** Convert an absolute path back to a "/memories/..." display path for the model. */
	static FString ToDisplayPath(const FString& AbsPath)
	{
		const FString Root = GetRoot();
		FString Rel = AbsPath;
		FPaths::MakePathRelativeTo(Rel, *(Root / TEXT("")));
		Rel.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (Rel.IsEmpty() || Rel == TEXT("."))
		{
			return TEXT("/memories");
		}
		return TEXT("/memories/") + Rel;
	}

	/** Recursive total size of a directory's files in bytes. */
	static int64 GetDirSize(const FString& AbsDir)
	{
		int64 Total = 0;
		IFileManager& FM = IFileManager::Get();
		TArray<FString> Files;
		FM.FindFilesRecursive(Files, *AbsDir, TEXT("*"), true, false);
		for (const FString& F : Files)
		{
			const int64 Size = FM.FileSize(*F);
			if (Size > 0)
			{
				Total += Size;
			}
		}
		return Total;
	}

	/** Build a "view" directory listing up to 2 levels deep, mirroring Claude's format. */
	static FString ViewDirectory(const FString& AbsDir)
	{
		IFileManager& FM = IFileManager::Get();
		const FString DisplayRoot = ToDisplayPath(AbsDir);

		FString Out = FString::Printf(
			TEXT("Here're the files and directories up to 2 levels deep in %s, excluding hidden items and node_modules:\n"),
			*DisplayRoot);
		Out += FString::Printf(TEXT("%s\t%s\n"), *HumanSize(GetDirSize(AbsDir)), *DisplayRoot);

		TFunction<void(const FString&, int32)> Walk = [&](const FString& Dir, int32 Depth)
		{
			if (Depth > 2)
			{
				return;
			}

			TArray<FString> Dirs;
			TArray<FString> Files;
			FM.FindFiles(Dirs, *(Dir / TEXT("*")), false, true);  // directories only
			FM.FindFiles(Files, *(Dir / TEXT("*")), true, false); // files only
			Dirs.Sort();
			Files.Sort();

			for (const FString& D : Dirs)
			{
				if (D.StartsWith(TEXT(".")) || D.Equals(TEXT("node_modules"), ESearchCase::IgnoreCase))
				{
					continue;
				}
				const FString AbsSub = Dir / D;
				Out += FString::Printf(TEXT("%s\t%s\n"), *HumanSize(GetDirSize(AbsSub)), *ToDisplayPath(AbsSub));
				Walk(AbsSub, Depth + 1);
			}
			for (const FString& F : Files)
			{
				if (F.StartsWith(TEXT(".")))
				{
					continue;
				}
				const FString AbsFile = Dir / F;
				Out += FString::Printf(TEXT("%s\t%s\n"), *HumanSize(FM.FileSize(*AbsFile)), *ToDisplayPath(AbsFile));
			}
		};
		Walk(AbsDir, 1);

		return Out;
	}

	/** Render file content with 6-char right-aligned, 1-indexed line numbers and an optional range. */
	static FString RenderWithLineNumbers(const TArray<FString>& Lines, int32 StartLine, int32 EndLine)
	{
		const int32 Start = StartLine > 0 ? StartLine : 1;
		const int32 End = (EndLine > 0) ? FMath::Min(EndLine, Lines.Num()) : Lines.Num();

		FString Out;
		for (int32 i = Start; i <= End; ++i)
		{
			Out += FString::Printf(TEXT("%6d\t%s\n"), i, *Lines[i - 1]);
		}
		return Out;
	}

	/** Count case-sensitive occurrences of Needle in Haystack, recording each match's 1-based line number. */
	static int32 CountOccurrences(const FString& Haystack, const FString& Needle, TArray<int32>& OutLines)
	{
		if (Needle.IsEmpty())
		{
			return 0;
		}

		int32 Count = 0;
		int32 From = 0;
		while (true)
		{
			const int32 Idx = Haystack.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (Idx == INDEX_NONE)
			{
				break;
			}
			int32 Line = 1;
			for (int32 i = 0; i < Idx; ++i)
			{
				if (Haystack[i] == TEXT('\n'))
				{
					++Line;
				}
			}
			OutLines.Add(Line);
			++Count;
			From = Idx + Needle.Len();
		}
		return Count;
	}

	/** A short line-numbered window of `Content` centred on the 1-based `CenterLine`. */
	static FString SnippetAround(const FString& Content, int32 CenterLine, int32 Context = 4)
	{
		TArray<FString> Lines;
		Content.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);
		const int32 Start = FMath::Max(1, CenterLine - Context);
		const int32 End = FMath::Min(Lines.Num(), CenterLine + Context);
		return RenderWithLineNumbers(Lines, Start, End);
	}

	/** Ensure the parent directory of AbsFile exists. */
	static void EnsureParentDir(const FString& AbsFile)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString ParentDir = FPaths::GetPath(AbsFile);
		if (!ParentDir.IsEmpty() && !PlatformFile.DirectoryExists(*ParentDir))
		{
			PlatformFile.CreateDirectoryTree(*ParentDir);
		}
	}
}

// ============================================================================
// memory - persistent cross-session memory (view/create/str_replace/insert/delete/rename)
// ============================================================================

namespace MemoryToolHelpers
{

static FString ExecuteMemoryCommand(const TMap<FString, FString>& Params)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FString Command = Params.FindRef(TEXT("command"));
	if (Command.IsEmpty())
	{
		Command = Params.FindRef(TEXT("Command"));
	}
	Command = Command.ToLower().TrimStartAndEnd();

	if (Command.IsEmpty())
	{
		return TEXT("Error: 'command' is required. Must be one of: view, create, str_replace, insert, delete, rename.");
	}

	// ---- view --------------------------------------------------------------
	if (Command == TEXT("view"))
	{
		const FString InPath = Params.FindRef(TEXT("path"));
		FString AbsPath, Err;
		if (!ResolvePath(InPath, AbsPath, Err))
		{
			return Err;
		}

		if (PlatformFile.DirectoryExists(*AbsPath))
		{
			return ViewDirectory(AbsPath);
		}

		if (!PlatformFile.FileExists(*AbsPath))
		{
			return FString::Printf(TEXT("The path %s does not exist. Please provide a valid path."), *ToDisplayPath(AbsPath));
		}

		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *AbsPath))
		{
			return FString::Printf(TEXT("Error: Failed to read %s."), *ToDisplayPath(AbsPath));
		}

		TArray<FString> Lines;
		Content.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);
		if (Lines.Num() > MaxFileLines)
		{
			return FString::Printf(TEXT("File %s exceeds maximum line limit of 999,999 lines."), *ToDisplayPath(AbsPath));
		}

		// Optional view_range as a "[start, end]" JSON-ish array.
		int32 RangeStart = 0;
		int32 RangeEnd = 0;
		FString RangeStr = Params.FindRef(TEXT("view_range"));
		if (!RangeStr.IsEmpty())
		{
			RangeStr.ReplaceInline(TEXT("["), TEXT(""));
			RangeStr.ReplaceInline(TEXT("]"), TEXT(""));
			TArray<FString> Parts;
			RangeStr.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() >= 1) { RangeStart = FCString::Atoi(*Parts[0].TrimStartAndEnd()); }
			if (Parts.Num() >= 2) { RangeEnd = FCString::Atoi(*Parts[1].TrimStartAndEnd()); }
		}

		FString Out = FString::Printf(TEXT("Here's the content of %s with line numbers:\n"), *ToDisplayPath(AbsPath));
		Out += RenderWithLineNumbers(Lines, RangeStart, RangeEnd);
		return Out;
	}

	// ---- create ------------------------------------------------------------
	if (Command == TEXT("create"))
	{
		const FString InPath = Params.FindRef(TEXT("path"));
		FString AbsPath, Err;
		if (!ResolvePath(InPath, AbsPath, Err))
		{
			return Err;
		}

		if (PlatformFile.FileExists(*AbsPath))
		{
			return FString::Printf(TEXT("Error: File %s already exists"), *ToDisplayPath(AbsPath));
		}

		const FString FileText = Params.FindRef(TEXT("file_text"));
		EnsureParentDir(AbsPath);
		if (!FFileHelper::SaveStringToFile(FileText, *AbsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return FString::Printf(TEXT("Error: Failed to create %s."), *ToDisplayPath(AbsPath));
		}

		UE_LOG(LogMemoryTools, Log, TEXT("Created memory file: %s"), *AbsPath);
		return FString::Printf(TEXT("File created successfully at: %s"), *ToDisplayPath(AbsPath));
	}

	// ---- str_replace -------------------------------------------------------
	if (Command == TEXT("str_replace"))
	{
		const FString InPath = Params.FindRef(TEXT("path"));
		FString AbsPath, Err;
		if (!ResolvePath(InPath, AbsPath, Err))
		{
			return Err;
		}

		if (PlatformFile.DirectoryExists(*AbsPath) || !PlatformFile.FileExists(*AbsPath))
		{
			return FString::Printf(TEXT("Error: The path %s does not exist. Please provide a valid path."), *ToDisplayPath(AbsPath));
		}

		const FString OldStr = Params.FindRef(TEXT("old_str"));
		const FString NewStr = Params.FindRef(TEXT("new_str"));

		FString Content;
		FFileHelper::LoadFileToString(Content, *AbsPath);

		TArray<int32> MatchLines;
		const int32 Occurrences = CountOccurrences(Content, OldStr, MatchLines);
		if (Occurrences == 0)
		{
			return FString::Printf(TEXT("No replacement was performed, old_str `%s` did not appear verbatim in %s."), *OldStr, *ToDisplayPath(AbsPath));
		}
		if (Occurrences > 1)
		{
			TArray<FString> LineStrs;
			for (int32 L : MatchLines) { LineStrs.Add(FString::FromInt(L)); }
			return FString::Printf(TEXT("No replacement was performed. Multiple occurrences of old_str `%s` in lines: %s. Please ensure it is unique"),
				*OldStr, *FString::Join(LineStrs, TEXT(", ")));
		}

		const int32 EditLine = MatchLines.Num() > 0 ? MatchLines[0] : 1;
		Content.ReplaceInline(*OldStr, *NewStr, ESearchCase::CaseSensitive);
		if (!FFileHelper::SaveStringToFile(Content, *AbsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return FString::Printf(TEXT("Error: Failed to write %s."), *ToDisplayPath(AbsPath));
		}

		UE_LOG(LogMemoryTools, Log, TEXT("Edited memory file (str_replace): %s"), *AbsPath);
		FString Out = TEXT("The memory file has been edited.\n");
		Out += SnippetAround(Content, EditLine);
		return Out;
	}

	// ---- insert ------------------------------------------------------------
	if (Command == TEXT("insert"))
	{
		const FString InPath = Params.FindRef(TEXT("path"));
		FString AbsPath, Err;
		if (!ResolvePath(InPath, AbsPath, Err))
		{
			return Err;
		}

		if (PlatformFile.DirectoryExists(*AbsPath) || !PlatformFile.FileExists(*AbsPath))
		{
			return FString::Printf(TEXT("Error: The path %s does not exist"), *ToDisplayPath(AbsPath));
		}

		FString InsertLineStr = Params.FindRef(TEXT("insert_line"));
		const int32 InsertLine = FCString::Atoi(*InsertLineStr);
		const FString InsertText = Params.FindRef(TEXT("insert_text"));

		FString Content;
		FFileHelper::LoadFileToString(Content, *AbsPath);
		TArray<FString> Lines;
		Content.ParseIntoArrayLines(Lines, /*CullEmpty=*/false);

		if (InsertLine < 0 || InsertLine > Lines.Num())
		{
			return FString::Printf(TEXT("Error: Invalid `insert_line` parameter: %d. It should be within the range of lines of the file: [0, %d]"),
				InsertLine, Lines.Num());
		}

		// Allow multi-line insert_text; strip a single trailing newline so we don't double up.
		FString ToInsert = InsertText;
		ToInsert.RemoveFromEnd(TEXT("\n"));
		TArray<FString> InsertLines;
		ToInsert.ParseIntoArrayLines(InsertLines, /*CullEmpty=*/false);

		// Insert AFTER `InsertLine` lines (0 = beginning of file).
		for (int32 i = 0; i < InsertLines.Num(); ++i)
		{
			Lines.Insert(InsertLines[i], InsertLine + i);
		}

		const FString NewContent = FString::Join(Lines, TEXT("\n"));
		if (!FFileHelper::SaveStringToFile(NewContent, *AbsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return FString::Printf(TEXT("Error: Failed to write %s."), *ToDisplayPath(AbsPath));
		}

		UE_LOG(LogMemoryTools, Log, TEXT("Edited memory file (insert): %s"), *AbsPath);
		return FString::Printf(TEXT("The file %s has been edited."), *ToDisplayPath(AbsPath));
	}

	// ---- delete ------------------------------------------------------------
	if (Command == TEXT("delete"))
	{
		const FString InPath = Params.FindRef(TEXT("path"));
		FString AbsPath, Err;
		if (!ResolvePath(InPath, AbsPath, Err))
		{
			return Err;
		}

		const bool bIsDir = PlatformFile.DirectoryExists(*AbsPath);
		const bool bIsFile = PlatformFile.FileExists(*AbsPath);
		if (!bIsDir && !bIsFile)
		{
			return FString::Printf(TEXT("Error: The path %s does not exist"), *ToDisplayPath(AbsPath));
		}

		// Guard against deleting the memory root itself.
		if (AbsPath.Equals(GetRoot(), ESearchCase::IgnoreCase))
		{
			return TEXT("Error: Cannot delete the memory root directory.");
		}

		bool bOk = false;
		if (bIsDir)
		{
			bOk = IFileManager::Get().DeleteDirectory(*AbsPath, false, true);
		}
		else
		{
			bOk = IFileManager::Get().Delete(*AbsPath, false, true);
		}
		if (!bOk)
		{
			return FString::Printf(TEXT("Error: Failed to delete %s."), *ToDisplayPath(AbsPath));
		}

		UE_LOG(LogMemoryTools, Log, TEXT("Deleted memory path: %s"), *AbsPath);
		return FString::Printf(TEXT("Successfully deleted %s"), *ToDisplayPath(AbsPath));
	}

	// ---- rename ------------------------------------------------------------
	if (Command == TEXT("rename"))
	{
		const FString InOld = Params.FindRef(TEXT("old_path"));
		const FString InNew = Params.FindRef(TEXT("new_path"));

		FString AbsOld, AbsNew, Err;
		if (!ResolvePath(InOld, AbsOld, Err))
		{
			return Err;
		}
		if (!ResolvePath(InNew, AbsNew, Err))
		{
			return Err;
		}

		const bool bSrcExists = PlatformFile.FileExists(*AbsOld) || PlatformFile.DirectoryExists(*AbsOld);
		if (!bSrcExists)
		{
			return FString::Printf(TEXT("Error: The path %s does not exist"), *ToDisplayPath(AbsOld));
		}
		if (PlatformFile.FileExists(*AbsNew) || PlatformFile.DirectoryExists(*AbsNew))
		{
			return FString::Printf(TEXT("Error: The destination %s already exists"), *ToDisplayPath(AbsNew));
		}

		EnsureParentDir(AbsNew);
		if (!IFileManager::Get().Move(*AbsNew, *AbsOld, true, true))
		{
			return FString::Printf(TEXT("Error: Failed to rename %s to %s."), *ToDisplayPath(AbsOld), *ToDisplayPath(AbsNew));
		}

		UE_LOG(LogMemoryTools, Log, TEXT("Renamed memory path: %s -> %s"), *AbsOld, *AbsNew);
		return FString::Printf(TEXT("Successfully renamed %s to %s"), *ToDisplayPath(AbsOld), *ToDisplayPath(AbsNew));
	}

	return FString::Printf(TEXT("Error: Unknown command '%s'. Must be one of: view, create, str_replace, insert, delete, rename."), *Command);
}

} // namespace MemoryToolHelpers

// Manual registration (internal-only): mirrors Anthropic's memory tool command surface.
static FToolAutoRegistrar AutoRegister_memory(
	[]() {
		FToolRegistration Reg;
		Reg.Name = TEXT("memory");
		Reg.Description = TEXT(
			"Persistent cross-session memory for this project, stored on disk under the project's Saved folder. "
			"Use it to recall context the user asked you to remember in earlier sessions. "
			"Commands: 'view' (list a directory or read a file), 'create' (new file), 'str_replace' (edit), "
			"'insert' (insert at a line), 'delete' (remove a file/dir), 'rename' (move a file/dir). "
			"Paths use the /memories root, e.g. '/memories/notes.md'. "
			"IMPORTANT: only create, edit, rename, or delete memory when the user explicitly asks you to remember "
			"or forget something. Reading (view) is always allowed.");
		Reg.Category = TEXT("Memory");
		Reg.Parameters = TArray<FToolParameter>({
			FToolParameter(TEXT("command"), TEXT("Operation: view, create, str_replace, insert, delete, or rename."), TEXT("string"), true),
			FToolParameter(TEXT("path"), TEXT("Path under the memory root, e.g. '/memories/notes.md'. Used by view, create, str_replace, insert, delete."), TEXT("string"), false),
			FToolParameter(TEXT("file_text"), TEXT("Full contents for the 'create' command."), TEXT("string"), false),
			FToolParameter(TEXT("old_str"), TEXT("Exact text to find for 'str_replace' (must appear exactly once)."), TEXT("string"), false),
			FToolParameter(TEXT("new_str"), TEXT("Replacement text for 'str_replace'."), TEXT("string"), false),
			FToolParameter(TEXT("insert_line"), TEXT("Line number to insert AFTER for 'insert' (0 = beginning of file)."), TEXT("int"), false),
			FToolParameter(TEXT("insert_text"), TEXT("Text to insert for the 'insert' command."), TEXT("string"), false),
			FToolParameter(TEXT("view_range"), TEXT("Optional [start, end] line range for 'view' on a file."), TEXT("array"), false),
			FToolParameter(TEXT("old_path"), TEXT("Source path for the 'rename' command."), TEXT("string"), false),
			FToolParameter(TEXT("new_path"), TEXT("Destination path for the 'rename' command."), TEXT("string"), false)
		});
		Reg.ExecuteFunc = [](const TMap<FString, FString>& Params) -> FString
		{
			return MemoryToolHelpers::ExecuteMemoryCommand(Params);
		};
		Reg.bInternalOnly = true;
		return Reg;
	}()
);
