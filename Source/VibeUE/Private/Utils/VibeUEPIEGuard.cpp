// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEPIEGuard.h"
#include "Utils/VibeUEToolError.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

namespace VibeUE
{
	bool IsPlaySessionRunning()
	{
		return GEditor && (GEditor->PlayWorld != nullptr || GEditor->IsPlaySessionInProgress());
	}

	FString MakePIEWriteRefusal(const FString& Operation)
	{
		return FString::Printf(
			TEXT("PIE_ACTIVE: %s refused: a Play-In-Editor session is running, and asset writes are unsafe during PIE ")
			TEXT("(EditorAssetLibrary.load_asset returns None then, so a write can miss the asset or re-create it with ")
			TEXT("default settings). Stop PIE first (EditorToolset.EditorAppToolset.StopPIE), then call again. ")
			TEXT("Read-only calls keep working during PIE."),
			*Operation);
	}

	bool RefuseAssetWriteDuringPIE(const TCHAR* Operation)
	{
		if (!IsPlaySessionRunning())
		{
			return false;
		}
		const FString Message = MakePIEWriteRefusal(Operation);
		// "PIE_ACTIVE" + ": " + the text: RaiseToolError prefixes the tool name it is given.
		RaiseToolError(TEXT("PIE_ACTIVE"), Message.RightChop(FCString::Strlen(TEXT("PIE_ACTIVE: "))));
		return true;
	}

	UObject* LoadAssetAnyState(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		// "/Game/A/B" -> "/Game/A/B.B" so FSoftObjectPath can resolve it
		FString ObjectPath = AssetPath;
		if (!ObjectPath.Contains(TEXT(".")))
		{
			ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
		}
		// UEditorAssetLibrary::LoadAsset refuses to work while PIE is active (#671); resolving through
		// FSoftObjectPath works in any editor state.
		const FSoftObjectPath SoftPath(ObjectPath);
		if (UObject* InMemory = SoftPath.ResolveObject())
		{
			return InMemory;
		}
		// Not loaded: load it when its package exists on disk (a missing asset returns null without a load warning,
		// as UEditorAssetLibrary::DoesAssetExist + LoadAsset did).
		if (!FPackageName::DoesPackageExist(SoftPath.GetLongPackageName()))
		{
			return nullptr;
		}
		return SoftPath.TryLoad();
	}
}
