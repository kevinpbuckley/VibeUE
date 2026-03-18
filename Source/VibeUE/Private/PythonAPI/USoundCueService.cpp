// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/USoundCueService.h"

#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeConcatenator.h"
#include "Sound/SoundNodeDelay.h"
#include "Sound/SoundNodeSwitch.h"
#include "Sound/SoundNodeEnveloper.h"
#include "Sound/SoundNodeDistanceCrossFade.h"
#include "Sound/SoundNodeBranch.h"
#include "Sound/SoundNodeParamCrossFade.h"
#include "Sound/SoundNodeQualityLevel.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "SoundCueGraph/SoundCueGraph.h"
#include "SoundCueGraph/SoundCueGraphNode.h"
#include "SoundCueGraph/SoundCueGraphNode_Root.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetImportTask.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoundCueService, Log, All);

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

USoundCue* USoundCueService::LoadSoundCue(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("LoadSoundCue: path is empty"));
		return nullptr;
	}

	UObject* Loaded = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Loaded)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("LoadSoundCue: failed to load '%s'"), *AssetPath);
		return nullptr;
	}

	USoundCue* SoundCue = Cast<USoundCue>(Loaded);
	if (!SoundCue)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("LoadSoundCue: '%s' is not a SoundCue"), *AssetPath);
		return nullptr;
	}

	return SoundCue;
}

USoundWave* USoundCueService::LoadSoundWave(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	UObject* Loaded = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Loaded)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("LoadSoundWave: failed to load '%s'"), *AssetPath);
		return nullptr;
	}

	USoundWave* Wave = Cast<USoundWave>(Loaded);
	if (!Wave)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("LoadSoundWave: '%s' is not a SoundWave"), *AssetPath);
		return nullptr;
	}

	return Wave;
}

TArray<USoundCueGraphNode*> USoundCueService::GetGraphNodes(USoundCue* SoundCue)
{
	TArray<USoundCueGraphNode*> Result;

	if (!SoundCue)
	{
		return Result;
	}

	UEdGraph* Graph = SoundCue->GetGraph();
	if (!Graph)
	{
		return Result;
	}

	for (UEdGraphNode* EdNode : Graph->Nodes)
	{
		// Cast<USoundCueGraphNode> returns nullptr for USoundCueGraphNode_Root
		// since Root extends USoundCueGraphNode_Base (a sibling, not a subclass of USoundCueGraphNode)
		USoundCueGraphNode* CueNode = Cast<USoundCueGraphNode>(EdNode);
		if (CueNode && CueNode->SoundNode)
		{
			Result.Add(CueNode);
		}
	}

	return Result;
}

USoundCueGraphNode* USoundCueService::GetGraphNodeAtIndex(USoundCue* SoundCue, int32 Index)
{
	TArray<USoundCueGraphNode*> Nodes = GetGraphNodes(SoundCue);
	if (!Nodes.IsValidIndex(Index))
	{
		return nullptr;
	}
	return Nodes[Index];
}

// ============================================================================
// ASSET LIFECYCLE
// ============================================================================

FSoundCueResult USoundCueService::CreateSoundCue(const FString& AssetPath, const FString& SoundWavePath)
{
	FSoundCueResult Result;

	if (AssetPath.IsEmpty())
	{
		Result.Message = TEXT("AssetPath is empty");
		return Result;
	}

	FString PackagePath = FPaths::GetPath(AssetPath);
	FString AssetName   = FPaths::GetBaseFilename(AssetPath);

	if (PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		Result.Message = FString::Printf(
			TEXT("Could not parse path '%s' — expected format: /Game/Dir/AssetName"), *AssetPath);
		return Result;
	}

	if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		Result.Message = FString::Printf(
			TEXT("Asset already exists at '%s'. Delete it first or use a different name."), *AssetPath);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, USoundCue::StaticClass(), Factory);
	if (!NewAsset)
	{
		Result.Message = FString::Printf(TEXT("AssetTools failed to create SoundCue at '%s'"), *AssetPath);
		return Result;
	}

	USoundCue* SoundCue = Cast<USoundCue>(NewAsset);
	if (!SoundCue)
	{
		Result.Message = TEXT("Created asset is not a SoundCue — unexpected factory result");
		return Result;
	}

	// Optionally wire up an initial WavePlayer node as the cue output
	if (!SoundWavePath.IsEmpty())
	{
		USoundWave* SoundWave = LoadSoundWave(SoundWavePath);
		if (SoundWave)
		{
			FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "CreateSoundCue", "Create SoundCue"));
			SoundCue->Modify();

			USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
			if (WavePlayer)
			{
				WavePlayer->SetSoundWave(SoundWave);
				if (WavePlayer->GraphNode)
				{
					WavePlayer->GraphNode->NodePosX = -250;
					WavePlayer->GraphNode->NodePosY = 0;
				}
				// Wire root → WavePlayer via pins, compile to set FirstNode
				TArray<USoundCueGraphNode_Root*> RootNodes;
				SoundCue->GetGraph()->GetNodesOfClass<USoundCueGraphNode_Root>(RootNodes);
				USoundCueGraphNode* WaveGraphNode = Cast<USoundCueGraphNode>(WavePlayer->GraphNode);
				if (RootNodes.Num() == 1 && WaveGraphNode)
				{
					RootNodes[0]->Pins[0]->BreakAllPinLinks();
					RootNodes[0]->Pins[0]->MakeLinkTo(WaveGraphNode->GetOutputPin());
				}
				SoundCue->CompileSoundNodesFromGraphNodes();
			}
			else
			{
				UE_LOG(LogSoundCueService, Warning,
					TEXT("CreateSoundCue: ConstructSoundNode<WavePlayer> returned null — cue created without initial node"));
			}
		}
		else
		{
			UE_LOG(LogSoundCueService, Warning,
				TEXT("CreateSoundCue: SoundWave not found at '%s' — cue created without initial node"), *SoundWavePath);
		}
	}

	SoundCue->MarkPackageDirty();

	Result.bSuccess  = true;
	Result.AssetPath = SoundCue->GetPathName();
	Result.Message   = TEXT("SoundCue created successfully");
	return Result;
}

FSoundCueInfo USoundCueService::GetSoundCueInfo(const FString& AssetPath)
{
	FSoundCueInfo Info;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return Info;
	}

	Info.AssetPath        = SoundCue->GetPathName();
	Info.Duration         = SoundCue->GetDuration();
	Info.VolumeMultiplier = SoundCue->VolumeMultiplier;
	Info.PitchMultiplier  = SoundCue->PitchMultiplier;

	if (SoundCue->SoundClassObject)
	{
		Info.SoundClassName = SoundCue->SoundClassObject->GetName();
	}

	if (SoundCue->AttenuationSettings)
	{
		Info.AttenuationPath = SoundCue->AttenuationSettings->GetPathName();
	}

	TArray<USoundCueGraphNode*> Nodes = GetGraphNodes(SoundCue);
	Info.NodeCount = Nodes.Num();

	if (SoundCue->FirstNode)
	{
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (Nodes[i]->SoundNode == SoundCue->FirstNode)
			{
				Info.RootNodeIndex = i;
				break;
			}
		}
	}

	return Info;
}

bool USoundCueService::SaveSoundCue(const FString& AssetPath)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}
	return UEditorAssetLibrary::SaveAsset(AssetPath, false);
}

FSoundCueResult USoundCueService::DuplicateSoundCue(const FString& SourcePath, const FString& DestPath)
{
	FSoundCueResult Result;

	if (SourcePath.IsEmpty() || DestPath.IsEmpty())
	{
		Result.Message = TEXT("SourcePath and DestPath must not be empty");
		return Result;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
	{
		Result.Message = FString::Printf(TEXT("Source asset does not exist: '%s'"), *SourcePath);
		return Result;
	}

	if (UEditorAssetLibrary::DoesAssetExist(DestPath))
	{
		Result.Message = FString::Printf(TEXT("Destination already exists: '%s'. Delete it first."), *DestPath);
		return Result;
	}

	UObject* Duplicate = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
	if (!Duplicate)
	{
		Result.Message = FString::Printf(TEXT("DuplicateAsset failed: '%s' → '%s'"), *SourcePath, *DestPath);
		return Result;
	}

	UEditorAssetLibrary::SaveAsset(DestPath, false);

	Result.bSuccess  = true;
	Result.AssetPath = Duplicate->GetPathName();
	Result.Message   = FString::Printf(TEXT("Duplicated '%s' → '%s'"), *SourcePath, *DestPath);
	return Result;
}

bool USoundCueService::DeleteSoundCue(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("DeleteSoundCue: path is empty"));
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("DeleteSoundCue: asset does not exist at '%s'"), *AssetPath);
		return false;
	}

	return UEditorAssetLibrary::DeleteAsset(AssetPath);
}

// ============================================================================
// NODE INSPECTION
// ============================================================================

TArray<FSoundCueNodeInfo> USoundCueService::ListNodes(const FString& AssetPath)
{
	TArray<FSoundCueNodeInfo> Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return Result;
	}

	TArray<USoundCueGraphNode*> Nodes = GetGraphNodes(SoundCue);

	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		USoundCueGraphNode* GraphNode = Nodes[i];
		USoundNode*         SoundNode = GraphNode->SoundNode;

		FSoundCueNodeInfo Info;
		Info.NodeIndex   = i;
		Info.NodeClass   = SoundNode->GetClass()->GetName();
		Info.NodeTitle   = SoundNode->GetTitle().ToString();
		Info.PosX        = (float)GraphNode->NodePosX;
		Info.PosY        = (float)GraphNode->NodePosY;
		Info.bIsRootNode = (SoundNode == SoundCue->FirstNode);

		// Resolve ChildNodes (inputs feeding INTO this node) to their list indices
		for (USoundNode* Child : SoundNode->ChildNodes)
		{
			int32 ChildIdx = -1;
			if (Child)
			{
				for (int32 j = 0; j < Nodes.Num(); ++j)
				{
					if (Nodes[j]->SoundNode == Child)
					{
						ChildIdx = j;
						break;
					}
				}
			}
			Info.ChildIndices.Add(ChildIdx);
		}

		Result.Add(Info);
	}

	return Result;
}

// ============================================================================
// NODE CREATION
// ============================================================================

FSoundCueResult USoundCueService::AddWavePlayerNode(
	const FString& AssetPath, const FString& SoundWavePath, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddWavePlayer", "Add WavePlayer Node"));
	SoundCue->Modify();

	USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
	if (!WavePlayer)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeWavePlayer> returned null");
		return Result;
	}

	if (WavePlayer->GraphNode)
	{
		WavePlayer->GraphNode->NodePosX = (int32)PosX;
		WavePlayer->GraphNode->NodePosY = (int32)PosY;
	}

	if (!SoundWavePath.IsEmpty())
	{
		USoundWave* SoundWave = LoadSoundWave(SoundWavePath);
		if (SoundWave)
		{
			WavePlayer->SetSoundWave(SoundWave);
		}
		else
		{
			UE_LOG(LogSoundCueService, Warning,
				TEXT("AddWavePlayerNode: SoundWave not found at '%s' — node created without wave"), *SoundWavePath);
		}
	}

	SoundCue->MarkPackageDirty();

	// Report the new node's index
	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == WavePlayer)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("WavePlayer node created at index %d"), NewIndex);
	return Result;
}

FSoundCueResult USoundCueService::AddRandomNode(const FString& AssetPath, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddRandom", "Add Random Node"));
	SoundCue->Modify();

	USoundNodeRandom* RandomNode = SoundCue->ConstructSoundNode<USoundNodeRandom>();
	if (!RandomNode)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeRandom> returned null");
		return Result;
	}

	if (RandomNode->GraphNode)
	{
		RandomNode->GraphNode->NodePosX = (int32)PosX;
		RandomNode->GraphNode->NodePosY = (int32)PosY;
	}

	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == RandomNode)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("Random node created at index %d"), NewIndex);
	return Result;
}

FSoundCueResult USoundCueService::AddMixerNode(const FString& AssetPath, int32 NumInputs, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	int32 ClampedInputs = FMath::Clamp(NumInputs, 1, 32);

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddMixer", "Add Mixer Node"));
	SoundCue->Modify();

	USoundNodeMixer* MixerNode = SoundCue->ConstructSoundNode<USoundNodeMixer>();
	if (!MixerNode)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeMixer> returned null");
		return Result;
	}

	if (MixerNode->GraphNode)
	{
		MixerNode->GraphNode->NodePosX = (int32)PosX;
		MixerNode->GraphNode->NodePosY = (int32)PosY;
	}

	// Add graph input pins first (CreateInputPin keeps pins and ChildNodes in sync via
	// CompileSoundNodesFromGraphNodes). Direct SetNum on ChildNodes without matching pin
	// additions causes LinkGraphNodesFromSoundNodes to assert (pins != ChildNodes).
	MixerNode->InputVolume.SetNum(ClampedInputs);
	USoundCueGraphNode* MixerGraphNode = Cast<USoundCueGraphNode>(MixerNode->GraphNode);
	if (MixerGraphNode)
	{
		for (int32 i = 0; i < ClampedInputs; i++)
		{
			MixerGraphNode->CreateInputPin();
		}
	}
	SoundCue->CompileSoundNodesFromGraphNodes();
	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == MixerNode)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("Mixer node created at index %d (%d inputs)"), NewIndex, ClampedInputs);
	return Result;
}

FSoundCueResult USoundCueService::AddModulatorNode(const FString& AssetPath, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddModulator", "Add Modulator Node"));
	SoundCue->Modify();

	USoundNodeModulator* ModNode = SoundCue->ConstructSoundNode<USoundNodeModulator>();
	if (!ModNode)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeModulator> returned null");
		return Result;
	}

	if (ModNode->GraphNode)
	{
		ModNode->GraphNode->NodePosX = (int32)PosX;
		ModNode->GraphNode->NodePosY = (int32)PosY;
	}

	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == ModNode)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("Modulator node created at index %d"), NewIndex);
	return Result;
}

FSoundCueResult USoundCueService::AddAttenuationNode(const FString& AssetPath, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddAttenuation", "Add Attenuation Node"));
	SoundCue->Modify();

	USoundNodeAttenuation* AttNode = SoundCue->ConstructSoundNode<USoundNodeAttenuation>();
	if (!AttNode)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeAttenuation> returned null");
		return Result;
	}

	if (AttNode->GraphNode)
	{
		AttNode->GraphNode->NodePosX = (int32)PosX;
		AttNode->GraphNode->NodePosY = (int32)PosY;
	}

	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == AttNode)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("Attenuation node created at index %d"), NewIndex);
	return Result;
}

FSoundCueResult USoundCueService::AddLoopingNode(const FString& AssetPath, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddLooping", "Add Looping Node"));
	SoundCue->Modify();

	USoundNodeLooping* LoopNode = SoundCue->ConstructSoundNode<USoundNodeLooping>();
	if (!LoopNode)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeLooping> returned null");
		return Result;
	}

	if (LoopNode->GraphNode)
	{
		LoopNode->GraphNode->NodePosX = (int32)PosX;
		LoopNode->GraphNode->NodePosY = (int32)PosY;
	}

	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == LoopNode)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("Looping node created at index %d"), NewIndex);
	return Result;
}

FSoundCueResult USoundCueService::AddConcatenatorNode(const FString& AssetPath, int32 NumInputs, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	int32 ClampedInputs = FMath::Clamp(NumInputs, 2, 32);

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddConcatenator", "Add Concatenator Node"));
	SoundCue->Modify();

	USoundNodeConcatenator* ConcatNode = SoundCue->ConstructSoundNode<USoundNodeConcatenator>();
	if (!ConcatNode)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeConcatenator> returned null");
		return Result;
	}

	if (ConcatNode->GraphNode)
	{
		ConcatNode->GraphNode->NodePosX = (int32)PosX;
		ConcatNode->GraphNode->NodePosY = (int32)PosY;
	}

	// Add graph input pins first — same pattern as Mixer (see AddMixerNode comment)
	ConcatNode->InputVolume.SetNum(ClampedInputs);
	USoundCueGraphNode* ConcatGraphNode = Cast<USoundCueGraphNode>(ConcatNode->GraphNode);
	if (ConcatGraphNode)
	{
		for (int32 i = 0; i < ClampedInputs; i++)
		{
			ConcatGraphNode->CreateInputPin();
		}
	}
	SoundCue->CompileSoundNodesFromGraphNodes();
	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == ConcatNode)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("Concatenator node created at index %d (%d inputs)"), NewIndex, ClampedInputs);
	return Result;
}

FSoundCueResult USoundCueService::AddDelayNode(const FString& AssetPath, float PosX, float PosY)
{
	FSoundCueResult Result;

	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		Result.Message = FString::Printf(TEXT("Could not load SoundCue '%s'"), *AssetPath);
		return Result;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddDelay", "Add Delay Node"));
	SoundCue->Modify();

	USoundNodeDelay* DelayNode = SoundCue->ConstructSoundNode<USoundNodeDelay>();
	if (!DelayNode)
	{
		Result.Message = TEXT("ConstructSoundNode<USoundNodeDelay> returned null");
		return Result;
	}

	if (DelayNode->GraphNode)
	{
		DelayNode->GraphNode->NodePosX = (int32)PosX;
		DelayNode->GraphNode->NodePosY = (int32)PosY;
	}

	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> AllNodes = GetGraphNodes(SoundCue);
	int32 NewIndex = INDEX_NONE;
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		if (AllNodes[i]->SoundNode == DelayNode)
		{
			NewIndex = i;
			break;
		}
	}

	Result.bSuccess  = true;
	Result.AssetPath = AssetPath;
	Result.Message   = FString::Printf(TEXT("Delay node created at index %d"), NewIndex);
	return Result;
}

// Shared helper: add N input pins to a freshly-constructed graph node, then compile.
// Used by all multi-input nodes (Switch, DistanceCrossFade, ParamCrossFade).
static void AddPinsAndCompile(USoundCue* SoundCue, USoundCueGraphNode* GraphNode, int32 NumPins)
{
	for (int32 i = 0; i < NumPins; i++)
	{
		GraphNode->CreateInputPin();
	}
	SoundCue->CompileSoundNodesFromGraphNodes();
}

FSoundCueResult USoundCueService::AddSwitchNode(
	const FString& AssetPath, int32 NumInputs, float PosX, float PosY)
{
	FSoundCueResult Result;
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { Result.Message = FString::Printf(TEXT("Could not load '%s'"), *AssetPath); return Result; }

	int32 Clamped = FMath::Max(1, NumInputs);
	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddSwitch", "Add Switch Node"));
	SoundCue->Modify();

	USoundNodeSwitch* Node = SoundCue->ConstructSoundNode<USoundNodeSwitch>();
	if (!Node) { Result.Message = TEXT("ConstructSoundNode<USoundNodeSwitch> returned null"); return Result; }
	if (Node->GraphNode) { Node->GraphNode->NodePosX = (int32)PosX; Node->GraphNode->NodePosY = (int32)PosY; }

	AddPinsAndCompile(SoundCue, Cast<USoundCueGraphNode>(Node->GraphNode), Clamped);
	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> All = GetGraphNodes(SoundCue);
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < All.Num(); i++) { if (All[i]->SoundNode == Node) { Idx = i; break; } }

	Result.bSuccess = true; Result.AssetPath = AssetPath;
	Result.Message = FString::Printf(TEXT("Switch node created at index %d (%d inputs)"), Idx, Clamped);
	return Result;
}

FSoundCueResult USoundCueService::AddEnveloperNode(const FString& AssetPath, float PosX, float PosY)
{
	FSoundCueResult Result;
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { Result.Message = FString::Printf(TEXT("Could not load '%s'"), *AssetPath); return Result; }

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddEnveloper", "Add Enveloper Node"));
	SoundCue->Modify();

	USoundNodeEnveloper* Node = SoundCue->ConstructSoundNode<USoundNodeEnveloper>();
	if (!Node) { Result.Message = TEXT("ConstructSoundNode<USoundNodeEnveloper> returned null"); return Result; }
	if (Node->GraphNode) { Node->GraphNode->NodePosX = (int32)PosX; Node->GraphNode->NodePosY = (int32)PosY; }

	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> All = GetGraphNodes(SoundCue);
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < All.Num(); i++) { if (All[i]->SoundNode == Node) { Idx = i; break; } }

	Result.bSuccess = true; Result.AssetPath = AssetPath;
	Result.Message = FString::Printf(TEXT("Enveloper node created at index %d"), Idx);
	return Result;
}

FSoundCueResult USoundCueService::AddDistanceCrossFadeNode(
	const FString& AssetPath, int32 NumInputs, float PosX, float PosY)
{
	FSoundCueResult Result;
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { Result.Message = FString::Printf(TEXT("Could not load '%s'"), *AssetPath); return Result; }

	int32 Clamped = FMath::Max(2, NumInputs);
	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddDistXFade", "Add Distance CrossFade Node"));
	SoundCue->Modify();

	USoundNodeDistanceCrossFade* Node = SoundCue->ConstructSoundNode<USoundNodeDistanceCrossFade>();
	if (!Node) { Result.Message = TEXT("ConstructSoundNode<USoundNodeDistanceCrossFade> returned null"); return Result; }
	if (Node->GraphNode) { Node->GraphNode->NodePosX = (int32)PosX; Node->GraphNode->NodePosY = (int32)PosY; }

	// CrossFadeInput array is kept in sync by SetChildNodes override
	AddPinsAndCompile(SoundCue, Cast<USoundCueGraphNode>(Node->GraphNode), Clamped);
	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> All = GetGraphNodes(SoundCue);
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < All.Num(); i++) { if (All[i]->SoundNode == Node) { Idx = i; break; } }

	Result.bSuccess = true; Result.AssetPath = AssetPath;
	Result.Message = FString::Printf(TEXT("DistanceCrossFade node created at index %d (%d inputs)"), Idx, Clamped);
	return Result;
}

FSoundCueResult USoundCueService::AddBranchNode(
	const FString& AssetPath, float PosX, float PosY, const FString& BoolParameterName)
{
	FSoundCueResult Result;
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { Result.Message = FString::Printf(TEXT("Could not load '%s'"), *AssetPath); return Result; }

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddBranch", "Add Branch Node"));
	SoundCue->Modify();

	USoundNodeBranch* Node = SoundCue->ConstructSoundNode<USoundNodeBranch>();
	if (!Node) { Result.Message = TEXT("ConstructSoundNode<USoundNodeBranch> returned null"); return Result; }
	if (Node->GraphNode) { Node->GraphNode->NodePosX = (int32)PosX; Node->GraphNode->NodePosY = (int32)PosY; }

	if (!BoolParameterName.IsEmpty())
	{
		Node->BoolParameterName = FName(*BoolParameterName);
	}

	// Branch always has exactly 3 slots: True(0), False(1), Unset(2)
	AddPinsAndCompile(SoundCue, Cast<USoundCueGraphNode>(Node->GraphNode), 3);
	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> All = GetGraphNodes(SoundCue);
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < All.Num(); i++) { if (All[i]->SoundNode == Node) { Idx = i; break; } }

	Result.bSuccess = true; Result.AssetPath = AssetPath;
	Result.Message = FString::Printf(TEXT("Branch node created at index %d (True=0, False=1, Unset=2)"), Idx);
	return Result;
}

FSoundCueResult USoundCueService::AddParamCrossFadeNode(
	const FString& AssetPath, int32 NumInputs, float PosX, float PosY)
{
	FSoundCueResult Result;
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { Result.Message = FString::Printf(TEXT("Could not load '%s'"), *AssetPath); return Result; }

	int32 Clamped = FMath::Max(2, NumInputs);
	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddParamXFade", "Add Param CrossFade Node"));
	SoundCue->Modify();

	USoundNodeParamCrossFade* Node = SoundCue->ConstructSoundNode<USoundNodeParamCrossFade>();
	if (!Node) { Result.Message = TEXT("ConstructSoundNode<USoundNodeParamCrossFade> returned null"); return Result; }
	if (Node->GraphNode) { Node->GraphNode->NodePosX = (int32)PosX; Node->GraphNode->NodePosY = (int32)PosY; }

	AddPinsAndCompile(SoundCue, Cast<USoundCueGraphNode>(Node->GraphNode), Clamped);
	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> All = GetGraphNodes(SoundCue);
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < All.Num(); i++) { if (All[i]->SoundNode == Node) { Idx = i; break; } }

	Result.bSuccess = true; Result.AssetPath = AssetPath;
	Result.Message = FString::Printf(TEXT("ParamCrossFade node created at index %d (%d inputs)"), Idx, Clamped);
	return Result;
}

FSoundCueResult USoundCueService::AddQualityLevelNode(const FString& AssetPath, float PosX, float PosY)
{
	FSoundCueResult Result;
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { Result.Message = FString::Printf(TEXT("Could not load '%s'"), *AssetPath); return Result; }

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "AddQualityLevel", "Add Quality Level Node"));
	SoundCue->Modify();

	USoundNodeQualityLevel* Node = SoundCue->ConstructSoundNode<USoundNodeQualityLevel>();
	if (!Node) { Result.Message = TEXT("ConstructSoundNode<USoundNodeQualityLevel> returned null"); return Result; }
	if (Node->GraphNode) { Node->GraphNode->NodePosX = (int32)PosX; Node->GraphNode->NodePosY = (int32)PosY; }

	// Number of inputs equals number of project quality levels
	int32 NumLevels = Node->GetMinChildNodes();
	if (NumLevels > 0)
	{
		AddPinsAndCompile(SoundCue, Cast<USoundCueGraphNode>(Node->GraphNode), NumLevels);
	}
	SoundCue->MarkPackageDirty();

	TArray<USoundCueGraphNode*> All = GetGraphNodes(SoundCue);
	int32 Idx = INDEX_NONE;
	for (int32 i = 0; i < All.Num(); i++) { if (All[i]->SoundNode == Node) { Idx = i; break; } }

	Result.bSuccess = true; Result.AssetPath = AssetPath;
	Result.Message = FString::Printf(TEXT("QualityLevel node created at index %d (%d quality levels)"), Idx, NumLevels);
	return Result;
}

bool USoundCueService::MoveNode(const FString& AssetPath, int32 NodeIndex, float PosX, float PosY)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { return false; }

	USoundCueGraphNode* GraphNode = GetGraphNodeAtIndex(SoundCue, NodeIndex);
	if (!GraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("MoveNode: NodeIndex %d out of range"), NodeIndex);
		return false;
	}

	GraphNode->NodePosX = (int32)PosX;
	GraphNode->NodePosY = (int32)PosY;
	SoundCue->MarkPackageDirty();
	return true;
}

// ============================================================================
// NODE CONNECTIONS
// ============================================================================

bool USoundCueService::RemoveNode(const FString& AssetPath, int32 NodeIndex)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	USoundCueGraphNode* GraphNode = GetGraphNodeAtIndex(SoundCue, NodeIndex);
	if (!GraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("RemoveNode: NodeIndex %d out of range"), NodeIndex);
		return false;
	}

	USoundNode* SoundNode = GraphNode->SoundNode;

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "RemoveNode", "Remove SoundCue Node"));
	SoundCue->Modify();

	// Break all pin connections on this graph node
	for (UEdGraphPin* Pin : GraphNode->Pins)
	{
		Pin->BreakAllPinLinks();
	}

	// Remove from graph and AllNodes
	FBlueprintEditorUtils::RemoveNode(nullptr, GraphNode, true);
	SoundCue->AllNodes.Remove(SoundNode);

	// Rebuild ChildNodes and FirstNode from remaining graph state
	SoundCue->CompileSoundNodesFromGraphNodes();
	SoundCue->MarkPackageDirty();

	UE_LOG(LogSoundCueService, Verbose, TEXT("RemoveNode: removed [%d] %s"),
		NodeIndex, *SoundNode->GetClass()->GetName());

	return true;
}

bool USoundCueService::DisconnectNode(const FString& AssetPath, int32 NodeIndex, int32 InputSlot)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	USoundCueGraphNode* GraphNode = GetGraphNodeAtIndex(SoundCue, NodeIndex);
	if (!GraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("DisconnectNode: NodeIndex %d out of range"), NodeIndex);
		return false;
	}

	TArray<UEdGraphPin*> InputPins;
	GraphNode->GetInputPins(InputPins);

	if (!InputPins.IsValidIndex(InputSlot))
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("DisconnectNode: InputSlot %d out of range (node has %d input pins)"),
			InputSlot, InputPins.Num());
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "DisconnectNode", "Disconnect SoundCue Node"));
	SoundCue->Modify();

	InputPins[InputSlot]->BreakAllPinLinks();
	SoundCue->CompileSoundNodesFromGraphNodes();
	SoundCue->MarkPackageDirty();

	return true;
}

bool USoundCueService::ConnectNodes(
	const FString& AssetPath, int32 ParentIndex, int32 ChildIndex, int32 InputSlot)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	if (ParentIndex == ChildIndex)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("ConnectNodes: ParentIndex and ChildIndex are the same (%d) — self-connection not allowed"), ParentIndex);
		return false;
	}

	if (InputSlot < 0)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("ConnectNodes: InputSlot must be >= 0 (got %d)"), InputSlot);
		return false;
	}

	TArray<USoundCueGraphNode*> Nodes = GetGraphNodes(SoundCue);

	if (!Nodes.IsValidIndex(ParentIndex))
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("ConnectNodes: ParentIndex %d is out of range (node count: %d)"), ParentIndex, Nodes.Num());
		return false;
	}

	if (!Nodes.IsValidIndex(ChildIndex))
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("ConnectNodes: ChildIndex %d is out of range (node count: %d)"), ChildIndex, Nodes.Num());
		return false;
	}

	USoundNode* ParentNode = Nodes[ParentIndex]->SoundNode;
	USoundNode* ChildNode  = Nodes[ChildIndex]->SoundNode;

	// Validate InputSlot against the parent node's declared maximum
	int32 MaxChildren = ParentNode->GetMaxChildNodes();
	if (MaxChildren != USoundNode::MAX_ALLOWED_CHILD_NODES && InputSlot >= MaxChildren)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("ConnectNodes: InputSlot %d exceeds max children (%d) for '%s'"),
			InputSlot, MaxChildren, *ParentNode->GetClass()->GetName());
		return false;
	}

	USoundCueGraphNode* ParentGraphNode = Cast<USoundCueGraphNode>(ParentNode->GraphNode);
	USoundCueGraphNode* ChildGraphNode  = Cast<USoundCueGraphNode>(ChildNode->GraphNode);
	if (!ParentGraphNode || !ChildGraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("ConnectNodes: graph node is null on parent or child"));
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "ConnectNodes", "Connect SoundCue Nodes"));
	SoundCue->Modify();

	// Ensure the parent graph node has enough input pins to cover InputSlot.
	// We use CreateInputPin() (not ChildNodes.SetNum) so pins and ChildNodes stay in sync.
	TArray<UEdGraphPin*> InputPins;
	ParentGraphNode->GetInputPins(InputPins);
	while (InputPins.Num() <= InputSlot)
	{
		ParentGraphNode->CreateInputPin();
		ParentGraphNode->GetInputPins(InputPins);
	}

	// Make the visual connection, then compile ChildNodes from the graph.
	// This keeps the graph as the single source of truth (same direction as the editor).
	InputPins[InputSlot]->BreakAllPinLinks();
	InputPins[InputSlot]->MakeLinkTo(ChildGraphNode->GetOutputPin());
	SoundCue->CompileSoundNodesFromGraphNodes();
	SoundCue->MarkPackageDirty();

	UE_LOG(LogSoundCueService, Verbose,
		TEXT("ConnectNodes: [%d]%s <- [%d]%s (slot %d)"),
		ParentIndex, *ParentNode->GetClass()->GetName(),
		ChildIndex,  *ChildNode->GetClass()->GetName(),
		InputSlot);

	return true;
}

bool USoundCueService::SetRootNode(const FString& AssetPath, int32 NodeIndex)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	USoundCueGraphNode* GraphNode = GetGraphNodeAtIndex(SoundCue, NodeIndex);
	if (!GraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("SetRootNode: NodeIndex %d is out of range"), NodeIndex);
		return false;
	}

	// Wire root graph node → target node's output, then compile to update FirstNode.
	// Avoids calling LinkGraphNodesFromSoundNodes (which asserts pins==ChildNodes for all nodes).
	TArray<USoundCueGraphNode_Root*> RootNodes;
	SoundCue->GetGraph()->GetNodesOfClass<USoundCueGraphNode_Root>(RootNodes);
	if (RootNodes.Num() != 1)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("SetRootNode: expected 1 root node, found %d"), RootNodes.Num());
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetRootNode", "Set SoundCue Root Node"));
	SoundCue->Modify();

	RootNodes[0]->Pins[0]->BreakAllPinLinks();
	RootNodes[0]->Pins[0]->MakeLinkTo(GraphNode->GetOutputPin());
	SoundCue->CompileSoundNodesFromGraphNodes();
	SoundCue->MarkPackageDirty();

	UE_LOG(LogSoundCueService, Verbose,
		TEXT("SetRootNode: FirstNode set to [%d] %s"),
		NodeIndex, *GraphNode->SoundNode->GetClass()->GetName());

	return true;
}

// ============================================================================
// NODE PROPERTIES
// ============================================================================

bool USoundCueService::SetWavePlayerAsset(
	const FString& AssetPath, int32 NodeIndex, const FString& SoundWavePath)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	USoundCueGraphNode* GraphNode = GetGraphNodeAtIndex(SoundCue, NodeIndex);
	if (!GraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("SetWavePlayerAsset: NodeIndex %d is out of range"), NodeIndex);
		return false;
	}

	USoundNodeWavePlayer* WavePlayer = Cast<USoundNodeWavePlayer>(GraphNode->SoundNode);
	if (!WavePlayer)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("SetWavePlayerAsset: Node %d is '%s', not a WavePlayer"),
			NodeIndex, *GraphNode->SoundNode->GetClass()->GetName());
		return false;
	}

	USoundWave* SoundWave = LoadSoundWave(SoundWavePath);
	if (!SoundWave)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetWavePlayerAsset", "Set WavePlayer Asset"));
	SoundCue->Modify();
	WavePlayer->Modify();

	WavePlayer->SetSoundWave(SoundWave);
	SoundCue->MarkPackageDirty();

	return true;
}

// ============================================================================
// CUE SETTINGS
// ============================================================================

bool USoundCueService::SetVolumeMultiplier(const FString& AssetPath, float Value)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetVolume", "Set SoundCue Volume"));
	SoundCue->Modify();
	SoundCue->VolumeMultiplier = Value;
	SoundCue->MarkPackageDirty();
	return true;
}

bool USoundCueService::SetPitchMultiplier(const FString& AssetPath, float Value)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetPitch", "Set SoundCue Pitch"));
	SoundCue->Modify();
	SoundCue->PitchMultiplier = Value;
	SoundCue->MarkPackageDirty();
	return true;
}

bool USoundCueService::SetSoundClass(const FString& AssetPath, const FString& SoundClassPath)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	UObject* Loaded = UEditorAssetLibrary::LoadAsset(SoundClassPath);
	if (!Loaded)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("SetSoundClass: could not load asset at '%s'"), *SoundClassPath);
		return false;
	}

	USoundClass* SoundClass = Cast<USoundClass>(Loaded);
	if (!SoundClass)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("SetSoundClass: '%s' is not a SoundClass"), *SoundClassPath);
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetSoundClass", "Set SoundCue SoundClass"));
	SoundCue->Modify();
	SoundCue->SoundClassObject = SoundClass;
	SoundCue->MarkPackageDirty();
	return true;
}

bool USoundCueService::SetAttenuation(const FString& AssetPath, const FString& AttenuationAssetPath)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { return false; }

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetAttenuation", "Set SoundCue Attenuation"));
	SoundCue->Modify();

	if (AttenuationAssetPath.IsEmpty())
	{
		SoundCue->AttenuationSettings = nullptr;
	}
	else
	{
		UObject* Loaded = UEditorAssetLibrary::LoadAsset(AttenuationAssetPath);
		USoundAttenuation* Att = Cast<USoundAttenuation>(Loaded);
		if (!Att)
		{
			UE_LOG(LogSoundCueService, Warning,
				TEXT("SetAttenuation: '%s' is not a SoundAttenuation asset"), *AttenuationAssetPath);
			return false;
		}
		SoundCue->AttenuationSettings = Att;
	}

	SoundCue->MarkPackageDirty();
	return true;
}

FString USoundCueService::GetAttenuation(const FString& AssetPath)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { return FString(); }
	return SoundCue->AttenuationSettings
		? SoundCue->AttenuationSettings->GetPathName()
		: FString();
}

bool USoundCueService::SetConcurrency(
	const FString& AssetPath, const FString& ConcurrencyAssetPath, bool bClearExisting)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { return false; }

	// Allow empty path when only clearing existing concurrency assets
	USoundConcurrency* Concurrency = nullptr;
	if (!ConcurrencyAssetPath.IsEmpty())
	{
		UObject* Loaded = UEditorAssetLibrary::LoadAsset(ConcurrencyAssetPath);
		Concurrency = Cast<USoundConcurrency>(Loaded);
		if (!Concurrency)
		{
			UE_LOG(LogSoundCueService, Warning,
				TEXT("SetConcurrency: '%s' is not a SoundConcurrency asset"), *ConcurrencyAssetPath);
			return false;
		}
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetConcurrency", "Set SoundCue Concurrency"));
	SoundCue->Modify();

	if (bClearExisting)
	{
		SoundCue->ConcurrencySet.Empty();
	}

	if (Concurrency)
	{
		SoundCue->ConcurrencySet.Add(Concurrency);
	}

	SoundCue->MarkPackageDirty();
	return true;
}

TArray<FString> USoundCueService::GetConcurrency(const FString& AssetPath)
{
	TArray<FString> Result;
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue) { return Result; }
	for (const TObjectPtr<USoundConcurrency>& C : SoundCue->ConcurrencySet)
	{
		if (C) { Result.Add(C->GetPathName()); }
	}
	return Result;
}

FString USoundCueService::GetNodeProperty(
	const FString& AssetPath, int32 NodeIndex, const FString& PropertyName)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return FString();
	}

	USoundCueGraphNode* GraphNode = GetGraphNodeAtIndex(SoundCue, NodeIndex);
	if (!GraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("GetNodeProperty: NodeIndex %d out of range"), NodeIndex);
		return FString();
	}

	USoundNode* SoundNode = GraphNode->SoundNode;
	FProperty* Prop = SoundNode->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("GetNodeProperty: property '%s' not found on %s"),
			*PropertyName, *SoundNode->GetClass()->GetName());
		return FString();
	}

	FString ExportedValue;
	Prop->ExportText_InContainer(0, ExportedValue, SoundNode, SoundNode, SoundNode, PPF_None);
	return ExportedValue;
}

bool USoundCueService::SetNodeProperty(
	const FString& AssetPath, int32 NodeIndex,
	const FString& PropertyName, const FString& Value)
{
	USoundCue* SoundCue = LoadSoundCue(AssetPath);
	if (!SoundCue)
	{
		return false;
	}

	USoundCueGraphNode* GraphNode = GetGraphNodeAtIndex(SoundCue, NodeIndex);
	if (!GraphNode)
	{
		UE_LOG(LogSoundCueService, Warning, TEXT("SetNodeProperty: NodeIndex %d out of range"), NodeIndex);
		return false;
	}

	USoundNode* SoundNode = GraphNode->SoundNode;
	FProperty* Prop = SoundNode->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("SetNodeProperty: property '%s' not found on %s"),
			*PropertyName, *SoundNode->GetClass()->GetName());
		return false;
	}

	FScopedTransaction Transaction(NSLOCTEXT("SoundCueService", "SetNodeProperty", "Set SoundNode Property"));
	SoundCue->Modify();
	SoundNode->Modify();

	const TCHAR* ImportResult = Prop->ImportText_InContainer(*Value, SoundNode, SoundNode, PPF_None);
	if (!ImportResult)
	{
		UE_LOG(LogSoundCueService, Warning,
			TEXT("SetNodeProperty: failed to set '%s' = '%s' on %s"),
			*PropertyName, *Value, *SoundNode->GetClass()->GetName());
		return false;
	}

	SoundNode->PostEditChange();
	SoundCue->MarkPackageDirty();
	return true;
}

// ============================================================================
// SOUNDWAVE UTILITIES
// ============================================================================

FSoundWaveInfo USoundCueService::GetSoundWaveInfo(const FString& SoundWavePath)
{
	FSoundWaveInfo Info;

	USoundWave* Wave = LoadSoundWave(SoundWavePath);
	if (!Wave)
	{
		return Info;
	}

	Info.AssetPath   = Wave->GetPathName();
	Info.Duration    = Wave->GetDuration();
	Info.SampleRate  = Wave->GetSampleRateForCurrentPlatform();
	Info.NumChannels = Wave->NumChannels;
	Info.bLooping    = Wave->bLooping;
	Info.bStreaming  = Wave->IsStreaming(nullptr);

	return Info;
}

FSoundCueResult USoundCueService::ImportSoundWave(const FString& FilePath, const FString& AssetPath)
{
	FSoundCueResult Result;

	if (FilePath.IsEmpty() || AssetPath.IsEmpty())
	{
		Result.Message = TEXT("FilePath and AssetPath must not be empty");
		return Result;
	}

	if (!FPaths::FileExists(FilePath))
	{
		Result.Message = FString::Printf(TEXT("File not found on disk: '%s'"), *FilePath);
		return Result;
	}

	FString PackagePath = FPaths::GetPath(AssetPath);
	FString AssetName   = FPaths::GetBaseFilename(AssetPath);

	if (PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		Result.Message = FString::Printf(
			TEXT("Could not parse asset path '%s' — expected /Game/Dir/Name"), *AssetPath);
		return Result;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename         = FilePath;
	Task->DestinationPath  = PackagePath;
	Task->DestinationName  = AssetName;
	Task->bReplaceExisting = true;
	Task->bAutomated       = true;
	Task->bSave            = true;

	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task);
	AssetTools.ImportAssetTasks(Tasks);

	// Verify the asset was actually created
	USoundWave* Wave = LoadSoundWave(AssetPath);
	if (!Wave)
	{
		Result.Message = FString::Printf(
			TEXT("Import completed but asset not found at '%s' — check file format"), *AssetPath);
		return Result;
	}

	Result.bSuccess  = true;
	Result.AssetPath = Wave->GetPathName();
	Result.Message   = FString::Printf(TEXT("Imported '%s' → '%s'"), *FilePath, *AssetPath);
	return Result;
}
