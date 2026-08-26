// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEEnvironment.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Utils/VibeUEPaths.h"

namespace
{
	TSharedPtr<FJsonObject> LoadEnvironmentJson(const FString& Path)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { return nullptr; }
		TSharedPtr<FJsonObject> Object;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) ? Object : nullptr;
	}

	TSharedRef<FJsonObject> PathDiagnostic(const FString& Path)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("path"), FPaths::ConvertRelativePathToFull(Path));
		Object->SetBoolField(TEXT("available"), FPaths::FileExists(Path) || FPaths::DirectoryExists(Path));
		return Object;
	}

	bool HasBuildInputsNewerThan(const FString& Directory, const FDateTime& BuildTime)
	{
		if (!FPaths::DirectoryExists(Directory)) { return false; }
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*.*"), true, false, false);
		for (const FString& File : Files)
		{
			const FString Extension = FPaths::GetExtension(File).ToLower();
			if ((Extension == TEXT("cpp") || Extension == TEXT("h") || Extension == TEXT("cs") ||
				Extension == TEXT("uplugin") || Extension == TEXT("uproject")) &&
				IFileManager::Get().GetTimeStamp(*File) > BuildTime)
			{
				return true;
			}
		}
		return false;
	}
}

TSharedRef<FJsonObject> FVibeUEEnvironment::BuildObject()
{
	const FString ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString EngineRoot = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
#if PLATFORM_WINDOWS
	const FString BuildScript = FPaths::Combine(EngineRoot, TEXT("Engine/Build/BatchFiles/Build.bat"));
	const FString EditorExe = FPaths::Combine(EngineRoot, TEXT("Engine/Binaries/Win64/UnrealEditor.exe"));
	const FString UatPath = FPaths::Combine(EngineRoot, TEXT("Engine/Build/BatchFiles/RunUAT.bat"));
#elif PLATFORM_MAC
	const FString BuildScript = FPaths::Combine(EngineRoot, TEXT("Engine/Build/BatchFiles/Mac/Build.sh"));
	const FString EditorExe = FPaths::Combine(EngineRoot, TEXT("Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"));
	const FString UatPath = FPaths::Combine(EngineRoot, TEXT("Engine/Build/BatchFiles/RunUAT.sh"));
#else
	const FString BuildScript = FPaths::Combine(EngineRoot, TEXT("Engine/Build/BatchFiles/Linux/Build.sh"));
	const FString EditorExe = FPaths::Combine(EngineRoot, TEXT("Engine/Binaries/Linux/UnrealEditor"));
	const FString UatPath = FPaths::Combine(EngineRoot, TEXT("Engine/Build/BatchFiles/RunUAT.sh"));
#endif
	const FString UbtPath = FPaths::Combine(EngineRoot, TEXT("Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll"));

	FString Association;
	if (const TSharedPtr<FJsonObject> Project = LoadEnvironmentJson(ProjectFile))
	{
		Project->TryGetStringField(TEXT("EngineAssociation"), Association);
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("vibeue.environment.v1"));
	Root->SetStringField(TEXT("projectFile"), ProjectFile);
	Root->SetStringField(TEXT("projectRoot"), ProjectRoot);
	Root->SetStringField(TEXT("projectName"), FApp::GetProjectName());
	Root->SetStringField(TEXT("editorTarget"), FString(FApp::GetProjectName()) + TEXT("Editor"));
	Root->SetStringField(TEXT("configuration"), LexToString(FApp::GetBuildConfiguration()));
	Root->SetStringField(TEXT("engineRoot"), EngineRoot);
	Root->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
	Root->SetStringField(TEXT("engineAssociation"), Association);
	Root->SetObjectField(TEXT("engineSource"), PathDiagnostic(FPaths::Combine(EngineRoot, TEXT("Engine/Source"))));
	Root->SetObjectField(TEXT("editorExecutable"), PathDiagnostic(EditorExe));
	Root->SetObjectField(TEXT("ubt"), PathDiagnostic(UbtPath));
	Root->SetObjectField(TEXT("uat"), PathDiagnostic(UatPath));
	Root->SetObjectField(TEXT("engineBuildScript"), PathDiagnostic(BuildScript));
	Root->SetObjectField(TEXT("vibeueBuildScript"), PathDiagnostic(FPaths::Combine(FVibeUEPaths::GetPluginDir(),
#if PLATFORM_WINDOWS
		TEXT("BuildAndLaunchGame.ps1")
#else
		TEXT("BuildAndLaunchGame.sh")
#endif
	)));
	Root->SetStringField(TEXT("hostPlatform"), FPlatformProperties::PlatformName());
	Root->SetStringField(TEXT("architecture"), TEXT("64-bit"));
	Root->SetNumberField(TEXT("editorPid"), FPlatformProcess::GetCurrentProcessId());
	Root->SetStringField(TEXT("vibeueVersion"), FVibeUEPaths::GetPluginVersionName());
	Root->SetBoolField(TEXT("toolsetRegistryAvailable"), UToolsetRegistry::IsAvailable());

	TSharedRef<FJsonObject> Compiler = MakeShared<FJsonObject>();
	FString CompilerRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("VCToolsInstallDir"));
#if PLATFORM_WINDOWS
	Compiler->SetStringField(TEXT("kind"), TEXT("msvc"));
	Compiler->SetStringField(TEXT("path"), CompilerRoot);
	Compiler->SetBoolField(TEXT("available"), !CompilerRoot.IsEmpty() && FPaths::DirectoryExists(CompilerRoot));
	if (CompilerRoot.IsEmpty()) { Compiler->SetStringField(TEXT("diagnostic"), TEXT("VCToolsInstallDir is not present in the editor environment; UBT remains the authoritative toolchain probe.")); }
#else
	Compiler->SetStringField(TEXT("kind"), TEXT("platform-default"));
	Compiler->SetStringField(TEXT("path"), TEXT(""));
	Compiler->SetBoolField(TEXT("available"), true);
#endif
	Root->SetObjectField(TEXT("compiler"), Compiler);

	TArray<TSharedPtr<FJsonValue>> Plugins;
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Plugin->GetName());
		Item->SetStringField(TEXT("version"), Plugin->GetDescriptor().VersionName);
		Plugins.Add(MakeShared<FJsonValueObject>(Item));
	}
	Plugins.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("name")) < B->AsObject()->GetStringField(TEXT("name"));
	});
	Root->SetArrayField(TEXT("enabledPlugins"), Plugins);

	const FString LastBuildPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE/last-build.json"));
	if (const TSharedPtr<FJsonObject> LastBuild = LoadEnvironmentJson(LastBuildPath))
	{
		FString Status;
		LastBuild->TryGetStringField(TEXT("status"), Status);
		const FDateTime BuildTime = IFileManager::Get().GetTimeStamp(*LastBuildPath);
		const bool bStale = Status == TEXT("succeeded") &&
			(HasBuildInputsNewerThan(FPaths::Combine(ProjectRoot, TEXT("Source")), BuildTime) ||
			 HasBuildInputsNewerThan(FPaths::Combine(FVibeUEPaths::GetPluginDir(), TEXT("Source")), BuildTime) ||
			 IFileManager::Get().GetTimeStamp(*ProjectFile) > BuildTime);
		LastBuild->SetBoolField(TEXT("isStale"), bStale);
		if (bStale)
		{
			LastBuild->SetStringField(TEXT("previousStatus"), Status);
			LastBuild->SetStringField(TEXT("status"), TEXT("stale"));
			LastBuild->SetStringField(TEXT("diagnostic"), TEXT("Build inputs changed after the last successful build."));
		}
		Root->SetObjectField(TEXT("lastBuild"), LastBuild.ToSharedRef());
	}
	else
	{
		TSharedRef<FJsonObject> NotRunBuild = MakeShared<FJsonObject>();
		NotRunBuild->SetStringField(TEXT("status"), TEXT("not-run"));
		NotRunBuild->SetBoolField(TEXT("isStale"), false);
		NotRunBuild->SetStringField(TEXT("diagnostic"), TEXT("No VibeUE build-script result has been recorded in this project."));
		Root->SetObjectField(TEXT("lastBuild"), NotRunBuild);
	}
	return Root;
}

FString FVibeUEEnvironment::BuildJson()
{
	FString Out;
	FJsonSerializer::Serialize(BuildObject(), TJsonWriterFactory<>::Create(&Out));
	return Out;
}
