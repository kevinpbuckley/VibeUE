// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMetaSoundService.h"

// MetaSound Engine
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundSource.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"

// MetaSound Frontend
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendLiteral.h"

// MetaSound Editor
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFactory.h"

// Asset creation
#include "AssetToolsModule.h"
#include "IAssetTools.h"

// UE Editor
#include "EditorAssetLibrary.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaSoundService, Log, All);

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

namespace
{
	/** Convert a string DataType + Value into a FMetasoundFrontendLiteral. */
	FMetasoundFrontendLiteral MakeLiteral(const FString& Value, const FString& DataType)
	{
		FMetasoundFrontendLiteral Lit;
		if (DataType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
		{
			Lit.Set(FCString::Atof(*Value));
		}
		else if (DataType.Equals(TEXT("Int32"), ESearchCase::IgnoreCase)
		      || DataType.Equals(TEXT("Int"), ESearchCase::IgnoreCase))
		{
			Lit.Set(FCString::Atoi(*Value));
		}
		else if (DataType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
		{
			const bool bVal = Value.TrimStartAndEnd().Equals(TEXT("true"), ESearchCase::IgnoreCase)
			               || Value.TrimStartAndEnd().Equals(TEXT("1"), ESearchCase::IgnoreCase);
			Lit.Set(bVal);
		}
		else
		{
			// String (and Audio/Trigger literals generally use String representation)
			Lit.Set(Value);
		}
		return Lit;
	}

	/** Convert EMetaSoundOutputAudioFormat to a friendly string. */
	FString FormatEnumToString(EMetaSoundOutputAudioFormat Format)
	{
		switch (Format)
		{
			case EMetaSoundOutputAudioFormat::Mono:   return TEXT("Mono");
			case EMetaSoundOutputAudioFormat::Stereo: return TEXT("Stereo");
			case EMetaSoundOutputAudioFormat::Quad:   return TEXT("Quad");
			default:                                  return TEXT("Unknown");
		}
	}

	/** Convert a friendly string to EMetaSoundOutputAudioFormat. */
	EMetaSoundOutputAudioFormat StringToFormatEnum(const FString& Str)
	{
		if (Str.Equals(TEXT("Stereo"), ESearchCase::IgnoreCase)) return EMetaSoundOutputAudioFormat::Stereo;
		if (Str.Equals(TEXT("Quad"),   ESearchCase::IgnoreCase)) return EMetaSoundOutputAudioFormat::Quad;
		return EMetaSoundOutputAudioFormat::Mono;
	}

	/**
	 * Fuzzy vertex name lookup — tries an exact FName match first, then falls back to a
	 * case-insensitive suffix match with spaces stripped.  This lets callers pass display
	 * names (e.g. "On Play") and still resolve namespaced vertex names ("UE.Source.OnPlay").
	 *
	 * Returns the matched vertex FName, or NAME_None if nothing matched.
	 */
	FName FuzzyFindVertexName(UMetaSoundBuilderBase* Builder,
	                          const FGuid& NodeGuid,
	                          const FString& PinName,
	                          bool bSearchOutputs)
	{
		// --- 1. Exact match (fast path) ---
		EMetaSoundBuilderResult TestResult;
		const FMetaSoundNodeHandle NodeHandle(NodeGuid);
		const FName ExactName(*PinName);
		if (bSearchOutputs)
			Builder->FindNodeOutputByName(NodeHandle, ExactName, TestResult);
		else
			Builder->FindNodeInputByName(NodeHandle, ExactName, TestResult);

		if (TestResult == EMetaSoundBuilderResult::Succeeded)
		{
			return ExactName;
		}

		// --- 2. Suffix match: strip spaces + lowercase both sides ---
		// e.g. "On Play" → "onplay"  matches suffix of  "ue.source.onplay"
		const FString SearchNorm = PinName.Replace(TEXT(" "), TEXT("")).ToLower();
		FName MatchedName = NAME_None;

		Builder->GetConstBuilder().IterateNodes(
			[&](const FMetasoundFrontendClass&, const FMetasoundFrontendNode& Node)
			{
				if (MatchedName != NAME_None) return;
				if (Node.GetID() != NodeGuid)  return;

				const TArray<FMetasoundFrontendVertex>& Verts =
					bSearchOutputs ? Node.Interface.Outputs : Node.Interface.Inputs;

				for (const FMetasoundFrontendVertex& V : Verts)
				{
					FString VNorm = V.Name.ToString().Replace(TEXT(" "), TEXT("")).ToLower();
					if (VNorm == SearchNorm || VNorm.EndsWith(SearchNorm))
					{
						MatchedName = V.Name;
						break;
					}
				}
			});

		return MatchedName;
	}

	/** Quick failure result factory. */
	FMetaSoundResult Fail(const FString& Msg)
	{
		FMetaSoundResult R;
		R.bSuccess = false;
		R.Message  = Msg;
		return R;
	}

	/** Quick success result factory. */
	FMetaSoundResult Succeed(const FString& AssetPath, const FString& Msg = TEXT("OK"), const FString& NodeId = TEXT(""))
	{
		FMetaSoundResult R;
		R.bSuccess  = true;
		R.Message   = Msg;
		R.AssetPath = AssetPath;
		R.NodeId    = NodeId;
		return R;
	}
}

// ============================================================================
// PRIVATE MEMBER HELPERS
// ============================================================================

bool UMetaSoundService::ParseNodeGuid(const FString& NodeIdStr, FGuid& OutGuid, FMetaSoundResult& OutResult) const
{
	if (!FGuid::Parse(NodeIdStr, OutGuid))
	{
		OutResult = Fail(FString::Printf(TEXT("Invalid NodeId: '%s'"), *NodeIdStr));
		return false;
	}
	return true;
}

FMetaSoundNodeInfo UMetaSoundService::BuildNodeInfo(UMetaSoundBuilderBase* Builder,
                                                    const FMetasoundFrontendClass& Class,
                                                    const FMetasoundFrontendNode& Node) const
{
	FMetaSoundNodeInfo Info;
	Info.NodeId    = Node.GetID().ToString(EGuidFormats::DigitsWithHyphens);
	Info.ClassName = Class.Metadata.GetClassName().ToString();

#if WITH_EDITOR
	// Template and Invalid nodes cannot be resolved in the class registry —
	// calling GetNodeTitle on them triggers an assertion in MetasoundAssetManager.
	// Use the ClassName string as the title for those node types instead.
	{
		const EMetasoundFrontendClassType T = Class.Metadata.GetType();
		if (T == EMetasoundFrontendClassType::Template || T == EMetasoundFrontendClassType::Invalid)
		{
			Info.NodeTitle = Info.ClassName;
		}
		else
		{
			Info.NodeTitle = Builder->GetConstBuilder().GetNodeTitle(Node.GetID()).ToString();
		}
	}
#else
	Info.NodeTitle = Info.ClassName;
#endif

	// Inputs
	for (const FMetasoundFrontendVertex& V : Node.Interface.Inputs)
	{
		Info.Inputs.Add(V.Name.ToString() + TEXT(":") + V.TypeName.ToString());
	}

	// Outputs
	for (const FMetasoundFrontendVertex& V : Node.Interface.Outputs)
	{
		Info.Outputs.Add(V.Name.ToString() + TEXT(":") + V.TypeName.ToString());
	}

	// Position (editor-only Style.Display.Locations)
#if WITH_EDITORONLY_DATA
	if (!Node.Style.Display.Locations.IsEmpty())
	{
		const FVector2D& Loc = Node.Style.Display.Locations.CreateConstIterator().Value();
		Info.PosX = Loc.X;
		Info.PosY = Loc.Y;
	}
#endif

	return Info;
}

UMetaSoundBuilderBase* UMetaSoundService::BeginEditing(const FString& AssetPath,
                                                       UMetaSoundSource** OutSource,
                                                       FString& OutError)
{
	UObject* Loaded = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Loaded)
	{
		OutError = FString::Printf(TEXT("BeginEditing: failed to load '%s'"), *AssetPath);
		return nullptr;
	}

	*OutSource = Cast<UMetaSoundSource>(Loaded);
	if (!*OutSource)
	{
		OutError = FString::Printf(TEXT("BeginEditing: '%s' is not a MetaSoundSource"), *AssetPath);
		return nullptr;
	}

	UMetaSoundEditorSubsystem& EditorSub = UMetaSoundEditorSubsystem::GetChecked();
	EMetaSoundBuilderResult FindResult;
	TScriptInterface<IMetaSoundDocumentInterface> DocIface(*OutSource);
	UMetaSoundBuilderBase* Builder = EditorSub.FindOrBeginBuilding(DocIface, FindResult);

	if (FindResult != EMetaSoundBuilderResult::Succeeded || !Builder)
	{
		OutError = FString::Printf(TEXT("BeginEditing: could not get builder for '%s'"), *AssetPath);
		return nullptr;
	}

	return Builder;
}

void UMetaSoundService::CommitEditing(const FString& AssetPath, UMetaSoundSource* Source)
{
	UMetaSoundEditorSubsystem::GetChecked().RegisterGraphWithFrontend(*Source);
	// Notify any open MetaSound editor window to resync its graph view.
	// Without this, the editor displays stale state until the asset is closed/reopened.
	Source->PostEditChange();
	UEditorAssetLibrary::SaveAsset(AssetPath, false);
}

// ============================================================================
// LIFECYCLE
// ============================================================================

FMetaSoundResult UMetaSoundService::CreateMetaSound(const FString& PackagePath,
                                                     const FString& AssetName,
                                                     const FString& OutputFormat)
{
	if (PackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		return Fail(TEXT("CreateMetaSound: PackagePath and AssetName must not be empty"));
	}

	// Use the editor factory path (NewObject + InitAsset + RegisterGraphWithFrontend)
	// exactly as the editor does when creating a MetaSound Source via right-click.
	// Using CreateSourceBuilder + BuildToAsset instead produces two sets of interface
	// nodes (Input/Output type from the builder + Template type from FindOrBeginBuilding),
	// resulting in orphan nodes visible in the graph editor.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	UMetaSoundSourceFactory* Factory = NewObject<UMetaSoundSourceFactory>();
	UObject* CreatedObj = AssetTools.CreateAsset(AssetName, PackagePath, UMetaSoundSource::StaticClass(), Factory);
	UMetaSoundSource* NewSource = Cast<UMetaSoundSource>(CreatedObj);
	if (!NewSource)
	{
		return Fail(TEXT("CreateMetaSound: factory failed to create UMetaSoundSource"));
	}

	// Set the requested output format. The factory initialises with the class default
	// (Mono); if a different format is requested, update it and re-register so the
	// audio output interface nodes reflect the correct channel count.
	const EMetaSoundOutputAudioFormat DesiredFormat = StringToFormatEnum(OutputFormat);
	if (NewSource->OutputFormat != DesiredFormat)
	{
		NewSource->OutputFormat = DesiredFormat;
		UMetaSoundEditorSubsystem::GetChecked().RegisterGraphWithFrontend(*NewSource);
		UEditorAssetLibrary::SaveAsset(PackagePath / AssetName, false);
	}

	// Auto-remove Template/Invalid nodes that FindOrBeginBuilding injects on first open.
	// These orphan binding nodes are never useful to callers and confuse AI models.
	{
		const FString FullPath = PackagePath / AssetName;
		FString LoadError;
		UMetaSoundBuilderBase* Builder = BeginEditing(FullPath, &NewSource, LoadError);
		if (Builder)
		{
			TArray<FGuid> ToRemove;
			Builder->GetConstBuilder().IterateNodes(
				[&](const FMetasoundFrontendClass& Class, const FMetasoundFrontendNode& Node)
				{
					const EMetasoundFrontendClassType T = Class.Metadata.GetType();
					if (T == EMetasoundFrontendClassType::Template
					 || T == EMetasoundFrontendClassType::Invalid)
					{
						ToRemove.Add(Node.GetID());
					}
				});

			if (ToRemove.Num() > 0)
			{
				EMetaSoundBuilderResult R;
				for (const FGuid& NodeId : ToRemove)
				{
					Builder->RemoveNode(FMetaSoundNodeHandle(NodeId), R);
					UE_LOG(LogMetaSoundService, Log,
					       TEXT("CreateMetaSound: removed orphan Template node '%s'"),
					       *NodeId.ToString(EGuidFormats::DigitsWithHyphens));
				}
				CommitEditing(FullPath, NewSource);
			}
		}
	}

	const FString FullPath = PackagePath / AssetName;
	UE_LOG(LogMetaSoundService, Log, TEXT("CreateMetaSound: created '%s'"), *FullPath);
	return Succeed(FullPath, TEXT("Created"));
}

FMetaSoundResult UMetaSoundService::DeleteMetaSound(const FString& AssetPath)
{
	if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		return Fail(FString::Printf(TEXT("DeleteMetaSound: asset not found: '%s'"), *AssetPath));
	}

	if (!UEditorAssetLibrary::DeleteAsset(AssetPath))
	{
		return Fail(FString::Printf(TEXT("DeleteMetaSound: failed to delete '%s'"), *AssetPath));
	}

	return Succeed(AssetPath, TEXT("Deleted"));
}

FMetaSoundInfo UMetaSoundService::GetMetaSoundInfo(const FString& AssetPath)
{
	FMetaSoundInfo Info;
	Info.AssetPath = AssetPath;

	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		UE_LOG(LogMetaSoundService, Warning, TEXT("GetMetaSoundInfo: %s"), *LoadError);
		return Info;
	}

	Info.AssetName    = Source->GetName();
	Info.OutputFormat = FormatEnumToString(Source->OutputFormat);

	// Graph inputs/outputs
	EMetaSoundBuilderResult R;
	for (const FName& Name : Builder->GetGraphInputNames(R))
	{
		Info.GraphInputs.Add(Name.ToString());
	}
	for (const FName& Name : Builder->GetGraphOutputNames(R))
	{
		Info.GraphOutputs.Add(Name.ToString());
	}

	// Count nodes by iterating the document builder (skip Template/Invalid internal nodes)
	int32 Count = 0;
	Builder->GetConstBuilder().IterateNodes(
		[&Count](const FMetasoundFrontendClass& Class, const FMetasoundFrontendNode&)
		{
			const EMetasoundFrontendClassType T = Class.Metadata.GetType();
			if (T != EMetasoundFrontendClassType::Template && T != EMetasoundFrontendClassType::Invalid)
			{
				++Count;
			}
		});
	Info.NodeCount = Count;

	return Info;
}

FMetaSoundResult UMetaSoundService::SaveMetaSound(const FString& AssetPath)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, TEXT("Saved"));
}

// ============================================================================
// NODE DISCOVERY
// ============================================================================

TArray<FMetaSoundNodeClassInfo> UMetaSoundService::ListAvailableNodes(const FString& SearchFilter)
{
	TArray<FMetaSoundNodeClassInfo> Result;

#if WITH_EDITORONLY_DATA
	TArray<FMetasoundFrontendClass> AllClasses =
		Metasound::Frontend::ISearchEngine::Get().FindAllClasses(false);

	AllClasses.Sort([](const FMetasoundFrontendClass& A, const FMetasoundFrontendClass& B)
	{
		return A.Metadata.GetClassName().ToString() < B.Metadata.GetClassName().ToString();
	});

	for (const FMetasoundFrontendClass& Class : AllClasses)
	{
		// Only expose External (DSP) and Graph (asset-reference) node classes
		const EMetasoundFrontendClassType ClassType = Class.Metadata.GetType();
		if (ClassType != EMetasoundFrontendClassType::External &&
		    ClassType != EMetasoundFrontendClassType::Graph)
		{
			continue;
		}

		FMetaSoundNodeClassInfo Info;
		const FMetasoundFrontendClassName& CN = Class.Metadata.GetClassName();
		Info.Namespace    = CN.Namespace.ToString();
		Info.Name         = CN.Name.ToString();
		Info.Variant      = CN.Variant.ToString();
		Info.FullClassName = CN.ToString();
		Info.MajorVersion = Class.Metadata.GetVersion().Major;
		Info.MinorVersion = Class.Metadata.GetVersion().Minor;

#if WITH_EDITOR
		Info.DisplayName  = Class.Metadata.GetDisplayName().ToString();
		Info.Description  = Class.Metadata.GetDescription().ToString();
#endif

		const FMetasoundFrontendClassInterface& Iface = Class.GetDefaultInterface();
		for (const FMetasoundFrontendClassInput& In : Iface.Inputs)
		{
			Info.Inputs.Add(In.Name.ToString() + TEXT(":") + In.TypeName.ToString());
		}
		for (const FMetasoundFrontendClassOutput& Out : Iface.Outputs)
		{
			Info.Outputs.Add(Out.Name.ToString() + TEXT(":") + Out.TypeName.ToString());
		}

		// Apply search filter (case-insensitive substring on FullClassName + DisplayName)
		if (!SearchFilter.IsEmpty())
		{
			const bool bMatchFull    = Info.FullClassName.Contains(SearchFilter, ESearchCase::IgnoreCase);
			const bool bMatchDisplay = Info.DisplayName.Contains(SearchFilter, ESearchCase::IgnoreCase);
			if (!bMatchFull && !bMatchDisplay)
			{
				continue;
			}
		}

		Result.Add(MoveTemp(Info));
	}
#endif

	return Result;
}

// ============================================================================
// NODE MANAGEMENT
// ============================================================================

FMetaSoundResult UMetaSoundService::AddNode(const FString& AssetPath,
                                             const FString& NodeNamespace,
                                             const FString& NodeName,
                                             const FString& NodeVariant,
                                             int32 MajorVersion,
                                             float PosX,
                                             float PosY)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	const FMetasoundFrontendClassName NodeClass{
		FName(*NodeNamespace),
		FName(*NodeName),
		FName(*NodeVariant)
	};

	EMetaSoundBuilderResult R;
	FMetaSoundNodeHandle NodeHandle = Builder->AddNodeByClassName(NodeClass, R, MajorVersion);

	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("AddNode: AddNodeByClassName failed for '%s.%s:%s'"),
		                            *NodeNamespace, *NodeName, *NodeVariant));
	}

#if WITH_EDITOR
	EMetaSoundBuilderResult LocR;
	Builder->SetNodeLocation(NodeHandle, FVector2D(PosX, PosY), LocR);
#endif

	CommitEditing(AssetPath, Source);

	const FString NodeId = NodeHandle.NodeID.ToString(EGuidFormats::DigitsWithHyphens);
	return Succeed(AssetPath,
	               FString::Printf(TEXT("Added %s.%s:%s"), *NodeNamespace, *NodeName, *NodeVariant),
	               NodeId);
}

FMetaSoundResult UMetaSoundService::RemoveNode(const FString& AssetPath, const FString& NodeId)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	FGuid NodeGuid;
	FMetaSoundResult ErrResult;
	if (!ParseNodeGuid(NodeId, NodeGuid, ErrResult))
	{
		return ErrResult;
	}

	EMetaSoundBuilderResult R;
	Builder->RemoveNode(FMetaSoundNodeHandle(NodeGuid), R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("RemoveNode: node '%s' not found"), *NodeId));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, TEXT("Node removed"), NodeId);
}

TArray<FMetaSoundNodeInfo> UMetaSoundService::ListNodes(const FString& AssetPath)
{
	TArray<FMetaSoundNodeInfo> Result;

	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		UE_LOG(LogMetaSoundService, Warning, TEXT("ListNodes: %s"), *LoadError);
		return Result;
	}

	Builder->GetConstBuilder().IterateNodes(
		[&](const FMetasoundFrontendClass& Class, const FMetasoundFrontendNode& Node)
		{
			Result.Add(BuildNodeInfo(Builder, Class, Node));
		});

	return Result;
}

FMetaSoundNodeInfo UMetaSoundService::GetNodePins(const FString& AssetPath, const FString& NodeId)
{
	FMetaSoundNodeInfo Empty;

	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		UE_LOG(LogMetaSoundService, Warning, TEXT("GetNodePins: %s"), *LoadError);
		return Empty;
	}

	FGuid NodeGuid;
	FMetaSoundResult ErrResult;
	if (!ParseNodeGuid(NodeId, NodeGuid, ErrResult))
	{
		return Empty;
	}

	FMetaSoundNodeInfo Found;
	bool bFoundNode = false;

	Builder->GetConstBuilder().IterateNodes(
		[&](const FMetasoundFrontendClass& Class, const FMetasoundFrontendNode& Node)
		{
			if (bFoundNode)
			{
				return;
			}
			if (Node.GetID() == NodeGuid)
			{
				Found = BuildNodeInfo(Builder, Class, Node);
				bFoundNode = true;
			}
		});

	if (!bFoundNode)
	{
		UE_LOG(LogMetaSoundService, Warning, TEXT("GetNodePins: node '%s' not found"), *NodeId);
	}

	return Found;
}

// ============================================================================
// CONNECTIONS
// ============================================================================

FMetaSoundResult UMetaSoundService::ConnectNodes(const FString& AssetPath,
                                                  const FString& FromNodeId,
                                                  const FString& OutputName,
                                                  const FString& ToNodeId,
                                                  const FString& InputName)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	FGuid FromGuid, ToGuid;
	FMetaSoundResult ErrResult;
	if (!ParseNodeGuid(FromNodeId, FromGuid, ErrResult)) return ErrResult;
	if (!ParseNodeGuid(ToNodeId,   ToGuid,   ErrResult)) return ErrResult;

	EMetaSoundBuilderResult R;

	const FName ResolvedOutputName = FuzzyFindVertexName(Builder, FromGuid, OutputName, true);
	if (ResolvedOutputName == NAME_None)
	{
		return Fail(FString::Printf(TEXT("ConnectNodes: output pin '%s' not found on node '%s'"),
		                            *OutputName, *FromNodeId));
	}
	if (ResolvedOutputName.ToString() != OutputName)
	{
		UE_LOG(LogMetaSoundService, Log, TEXT("ConnectNodes: fuzzy output match '%s' → '%s'"),
		       *OutputName, *ResolvedOutputName.ToString());
	}
	const FMetaSoundBuilderNodeOutputHandle OutHandle =
		Builder->FindNodeOutputByName(FMetaSoundNodeHandle(FromGuid), ResolvedOutputName, R);

	const FName ResolvedInputName = FuzzyFindVertexName(Builder, ToGuid, InputName, false);
	if (ResolvedInputName == NAME_None)
	{
		return Fail(FString::Printf(TEXT("ConnectNodes: input pin '%s' not found on node '%s'"),
		                            *InputName, *ToNodeId));
	}
	if (ResolvedInputName.ToString() != InputName)
	{
		UE_LOG(LogMetaSoundService, Log, TEXT("ConnectNodes: fuzzy input match '%s' → '%s'"),
		       *InputName, *ResolvedInputName.ToString());
	}
	const FMetaSoundBuilderNodeInputHandle InHandle =
		Builder->FindNodeInputByName(FMetaSoundNodeHandle(ToGuid), ResolvedInputName, R);

	Builder->ConnectNodes(OutHandle, InHandle, R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("ConnectNodes: connection failed (type mismatch or already connected)")));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Connected %s.%s -> %s.%s"),
	                                           *FromNodeId, *OutputName, *ToNodeId, *InputName));
}

FMetaSoundResult UMetaSoundService::DisconnectPin(const FString& AssetPath,
                                                   const FString& NodeId,
                                                   const FString& InputName)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	FGuid NodeGuid;
	FMetaSoundResult ErrResult;
	if (!ParseNodeGuid(NodeId, NodeGuid, ErrResult)) return ErrResult;

	EMetaSoundBuilderResult R;
	const FName ResolvedInputName = FuzzyFindVertexName(Builder, NodeGuid, InputName, false);
	if (ResolvedInputName == NAME_None)
	{
		return Fail(FString::Printf(TEXT("DisconnectPin: input pin '%s' not found on node '%s'"),
		                            *InputName, *NodeId));
	}
	const FMetaSoundBuilderNodeInputHandle InHandle =
		Builder->FindNodeInputByName(FMetaSoundNodeHandle(NodeGuid), ResolvedInputName, R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("DisconnectPin: input pin '%s' not found on node '%s'"),
		                            *InputName, *NodeId));
	}

	Builder->DisconnectNodeInput(InHandle, R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("DisconnectPin: pin '%s' was not connected"), *InputName));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Disconnected %s.%s"), *NodeId, *InputName));
}

// ============================================================================
// GRAPH I/O
// ============================================================================

FMetaSoundResult UMetaSoundService::AddGraphInput(const FString& AssetPath,
                                                   const FString& InputName,
                                                   const FString& DataType,
                                                   const FString& DefaultValue)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	const FMetasoundFrontendLiteral DefaultLit = MakeLiteral(DefaultValue, DataType);

	EMetaSoundBuilderResult R;
	Builder->AddGraphInputNode(FName(*InputName), FName(*DataType), DefaultLit, R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("AddGraphInput: failed to add '%s' (%s)"), *InputName, *DataType));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Added graph input '%s:%s'"), *InputName, *DataType));
}

FMetaSoundResult UMetaSoundService::AddGraphOutput(const FString& AssetPath,
                                                    const FString& OutputName,
                                                    const FString& DataType)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	const FMetasoundFrontendLiteral EmptyLit;
	EMetaSoundBuilderResult R;
	Builder->AddGraphOutputNode(FName(*OutputName), FName(*DataType), EmptyLit, R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("AddGraphOutput: failed to add '%s' (%s)"), *OutputName, *DataType));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Added graph output '%s:%s'"), *OutputName, *DataType));
}

FMetaSoundResult UMetaSoundService::RemoveGraphInput(const FString& AssetPath, const FString& InputName)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	EMetaSoundBuilderResult R;
	Builder->RemoveGraphInput(FName(*InputName), R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("RemoveGraphInput: input '%s' not found"), *InputName));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Removed graph input '%s'"), *InputName));
}

FMetaSoundResult UMetaSoundService::RemoveGraphOutput(const FString& AssetPath, const FString& OutputName)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	EMetaSoundBuilderResult R;
	Builder->RemoveGraphOutput(FName(*OutputName), R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("RemoveGraphOutput: output '%s' not found"), *OutputName));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Removed graph output '%s'"), *OutputName));
}

// ============================================================================
// NODE CONFIGURATION
// ============================================================================

FMetaSoundResult UMetaSoundService::SetNodeInputDefault(const FString& AssetPath,
                                                         const FString& NodeId,
                                                         const FString& InputName,
                                                         const FString& Value,
                                                         const FString& DataType)
{
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	FGuid NodeGuid;
	FMetaSoundResult ErrResult;
	if (!ParseNodeGuid(NodeId, NodeGuid, ErrResult)) return ErrResult;

	EMetaSoundBuilderResult R;
	const FName ResolvedInputName = FuzzyFindVertexName(Builder, NodeGuid, InputName, false);
	if (ResolvedInputName == NAME_None)
	{
		return Fail(FString::Printf(TEXT("SetNodeInputDefault: pin '%s' not found on node '%s'"),
		                            *InputName, *NodeId));
	}
	const FMetaSoundBuilderNodeInputHandle InHandle =
		Builder->FindNodeInputByName(FMetaSoundNodeHandle(NodeGuid), ResolvedInputName, R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("SetNodeInputDefault: pin '%s' not found on node '%s'"),
		                            *InputName, *NodeId));
	}

	FMetasoundFrontendLiteral Lit;
	if (DataType.Equals(TEXT("WaveAsset"), ESearchCase::IgnoreCase) ||
	    DataType.Equals(TEXT("Object"),    ESearchCase::IgnoreCase))
	{
		// Value is an asset path — load the UObject and create an object literal
		UObject* Asset = UEditorAssetLibrary::LoadAsset(Value);
		if (!Asset)
		{
			return Fail(FString::Printf(TEXT("SetNodeInputDefault: could not load asset '%s'"), *Value));
		}
		Lit.Set(Asset);
	}
	else
	{
		Lit = MakeLiteral(Value, DataType);
	}

	Builder->SetNodeInputDefault(InHandle, Lit, R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("SetNodeInputDefault: failed to set '%s' on '%s'"),
		                            *InputName, *NodeId));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Set %s.%s = %s"), *NodeId, *InputName, *Value));
}

FMetaSoundResult UMetaSoundService::SetNodeLocation(const FString& AssetPath,
                                                     const FString& NodeId,
                                                     float PosX,
                                                     float PosY)
{
#if WITH_EDITOR
	FString LoadError;
	UMetaSoundSource* Source = nullptr;
	UMetaSoundBuilderBase* Builder = BeginEditing(AssetPath, &Source, LoadError);
	if (!Builder)
	{
		return Fail(LoadError);
	}

	FGuid NodeGuid;
	FMetaSoundResult ErrResult;
	if (!ParseNodeGuid(NodeId, NodeGuid, ErrResult)) return ErrResult;

	EMetaSoundBuilderResult R;
	Builder->SetNodeLocation(FMetaSoundNodeHandle(NodeGuid), FVector2D(PosX, PosY), R);
	if (R != EMetaSoundBuilderResult::Succeeded)
	{
		return Fail(FString::Printf(TEXT("SetNodeLocation: node '%s' not found"), *NodeId));
	}

	CommitEditing(AssetPath, Source);
	return Succeed(AssetPath, FString::Printf(TEXT("Moved node to (%.0f, %.0f)"), PosX, PosY), NodeId);
#else
	return Fail(TEXT("SetNodeLocation: requires editor build"));
#endif
}
