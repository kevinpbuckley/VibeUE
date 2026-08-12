// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBlackboardService.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeServiceInternal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_NativeEnum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	/**
	 * Ancestor cap on every parent-chain walk. The engine blocks cycle CREATION only in
	 * PostEditChangeProperty — a details-panel path these services do not take, and one a corrupt
	 * asset never took either — so a chain with a cycle in it must terminate here by bound, not
	 * by trust. No legitimate hierarchy approaches 64 levels.
	 */
	constexpr int32 kMaxParentDepth = 64;

	/**
	 * Name -> key type class. Explicit rather than reflective, so an unrecognised type is a
	 * reported error instead of a silently created broken key.
	 */
	UClass* ResolveKeyTypeClass(const FString& TypeName)
	{
		static const TMap<FString, UClass*> Table = {
			{ TEXT("Bool"),       UBlackboardKeyType_Bool::StaticClass() },
			{ TEXT("Int"),        UBlackboardKeyType_Int::StaticClass() },
			{ TEXT("Float"),      UBlackboardKeyType_Float::StaticClass() },
			{ TEXT("String"),     UBlackboardKeyType_String::StaticClass() },
			{ TEXT("Name"),       UBlackboardKeyType_Name::StaticClass() },
			{ TEXT("Vector"),     UBlackboardKeyType_Vector::StaticClass() },
			{ TEXT("Rotator"),    UBlackboardKeyType_Rotator::StaticClass() },
			{ TEXT("Object"),     UBlackboardKeyType_Object::StaticClass() },
			{ TEXT("Class"),      UBlackboardKeyType_Class::StaticClass() },
			{ TEXT("Enum"),       UBlackboardKeyType_Enum::StaticClass() },
			{ TEXT("NativeEnum"), UBlackboardKeyType_NativeEnum::StaticClass() },
		};
		UClass* const* Found = Table.Find(TypeName);
		return Found ? *Found : nullptr;
	}

	/** Inverse of ResolveKeyTypeClass, for reporting. */
	FString KeyTypeToName(const UBlackboardKeyType* KeyType)
	{
		if (!KeyType) { return FString(); }
		FString Name = KeyType->GetClass()->GetName();
		Name.RemoveFromStart(TEXT("BlackboardKeyType_"));
		return Name;
	}

	UBlackboardData* LoadBlackboard(const FString& AssetPath)
	{
		// LOAD_NoWarn | LOAD_Quiet: "not found" is a value returned to the caller, not an incident
		// worth engine warnings in the log — same rationale as the BT service's loads.
		return LoadObject<UBlackboardData>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/**
	 * Load AssetPath for writing, refusing when the write could not be trusted to land — the same
	 * two refusals the BT service's OpenWriteGuard applies, for the same reasons:
	 *
	 *  - A Play In Editor session: live UBlackboardComponents computed their key IDs and value
	 *    offsets from Keys at init. Mutating Keys under them makes every later read through those
	 *    IDs type-confused — silently, at runtime, in the running game.
	 *  - An open editor on the asset (a Blackboard editor, or a Behavior Tree editor whose tab
	 *    hosts this blackboard): it holds its own view of the keys, would not show this change,
	 *    and overwrites it on the human's next save.
	 *
	 * Returns an empty string on success (OutBoard set), otherwise the error.
	 */
	FString BlackboardWriteGuard(const FString& AssetPath, UBlackboardData*& OutBoard)
	{
		OutBoard = nullptr;

		if (AssetPath.IsEmpty())
		{
			return TEXT("AssetPath is empty");
		}
		const FString LengthError = VibeBT::CheckNameLength(AssetPath, TEXT("AssetPath"));
		if (!LengthError.IsEmpty())
		{
			return LengthError;
		}

		UBlackboardData* Board = LoadBlackboard(AssetPath);
		if (!Board)
		{
			return FString::Printf(TEXT("Blackboard not found: %s"), *AssetPath);
		}

		if (GEditor)
		{
			const FString PlayRefusal = VibeBT::PlaySessionRefusal(AssetPath, ToRawPtr(GEditor->PlayWorld));
			if (!PlayRefusal.IsEmpty())
			{
				return FString::Printf(
					TEXT("A Play In Editor session is running; stop it and retry writing %s. Live "
						 "UBlackboardComponents hold key IDs and value offsets computed from this "
						 "asset's keys at init — mutating the keys under them type-confuses every "
						 "later read. The asset on disk is unchanged."),
					*AssetPath);
			}

			if (UAssetEditorSubsystem* AssetEditorSubsystem =
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				if (AssetEditorSubsystem->FindEditorsForAsset(Board).Num() > 0)
				{
					return FString::Printf(
						TEXT("An editor is open on %s; close it and retry. The open editor holds its "
							 "own view of the keys, would not show this change, and would overwrite "
							 "it on the next save from the editor."),
						*AssetPath);
				}
			}
		}

		OutBoard = Board;
		return FString();
	}

	/**
	 * The engine's post-edit fix-ups for a key mutation — what UBlackboardData::
	 * PostEditChangeProperty runs when a human edits Keys in the details panel, which these
	 * services bypass by writing the array directly:
	 *
	 *  - PropagateKeyChangesToDerivedBlackboardAssets(): loaded child blackboards recompute their
	 *    FirstKeyID / ParentKeys against the new chain. Without it their key IDs collide with the
	 *    parent's until the next reload — the IDs every BT selector resolution hands out.
	 *  - UpdateIfHasSynchronizedKeys(): the bHasSynchronizedKeys flag instances consult.
	 *  - OnUpdateKeys broadcast: what an open Blackboard editor listens to. None can be open on
	 *    THIS asset (the write guard refused that), but one can be open on a derived asset whose
	 *    inherited keys just changed.
	 */
	void RunKeyChangeFixups(UBlackboardData* Board)
	{
		Board->PropagateKeyChangesToDerivedBlackboardAssets();
		Board->UpdateIfHasSynchronizedKeys();
		UBlackboardData::OnUpdateKeys.Broadcast(Board);
	}

	/** Dirty the package and save it to disk. Callers Modify() BEFORE mutating, not here. */
	FString SaveBlackboard(UBlackboardData* Board)
	{
		Board->MarkPackageDirty();
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Board->GetOutermost() }, /*bOnlyDirty*/ false))
		{
			// Same discipline as CommitGraph: a mutation that failed to reach disk must not sit
			// dirty in memory, where the next successful save of the asset ships it silently.
			return FString::Printf(
				TEXT("Failed to save Blackboard %s (read-only file or a held file lock?). The edit "
					 "did not reach disk%s"),
				*Board->GetPathName(),
				*VibeBT::DiscardDirtyStateFromDisk(Board->GetOutermost()));
		}
		return FString();
	}

	/** Find an entry this asset owns directly (not inherited) by name. */
	FBlackboardEntry* FindOwnEntry(UBlackboardData* Board, const FName& KeyName)
	{
		return Board->Keys.FindByPredicate(
			[&KeyName](const FBlackboardEntry& Entry) { return Entry.EntryName == KeyName; });
	}

	/**
	 * Every key inherited from Board's parent chain, walked manually rather than through the
	 * cached UBlackboardData::ParentKeys (transient, refreshed only by UpdateParentKeys /
	 * PostLoad). Child keys shadow a same-named parent key, matching UBlackboardData::GetKey.
	 * Depth-bounded — see kMaxParentDepth.
	 */
	void CollectInheritedKeys(const UBlackboardData* Board, TArray<FBlackboardEntry>& OutInherited)
	{
		TSet<FName> Seen;
		for (const FBlackboardEntry& Entry : Board->Keys)
		{
			Seen.Add(Entry.EntryName);
		}
		int32 Depth = 0;
		for (const UBlackboardData* Ancestor = Board->Parent;
			Ancestor && Depth < kMaxParentDepth; Ancestor = Ancestor->Parent, ++Depth)
		{
			for (const FBlackboardEntry& Entry : Ancestor->Keys)
			{
				bool bAlreadySeen = false;
				Seen.Add(Entry.EntryName, &bAlreadySeen);
				if (!bAlreadySeen)
				{
					OutInherited.Add(Entry);
				}
			}
		}
	}

	/** True if KeyName exists somewhere in the parent chain (i.e. is inherited, not owned). */
	bool IsInheritedName(UBlackboardData* Board, const FName& KeyName)
	{
		int32 Depth = 0;
		for (const UBlackboardData* Ancestor = Board->Parent;
			Ancestor && Depth < kMaxParentDepth; Ancestor = Ancestor->Parent, ++Depth)
		{
			if (Ancestor->Keys.ContainsByPredicate(
				[&KeyName](const FBlackboardEntry& Entry) { return Entry.EntryName == KeyName; }))
			{
				return true;
			}
		}
		return false;
	}

	/** Whether Candidate appears in Board's own parent chain (cycle test, depth-bounded). */
	bool ParentChainContains(const UBlackboardData* Board, const UBlackboardData* Candidate)
	{
		int32 Depth = 0;
		for (const UBlackboardData* Ancestor = Board;
			Ancestor && Depth < kMaxParentDepth; Ancestor = Ancestor->Parent, ++Depth)
		{
			if (Ancestor == Candidate)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Guard + own-entry resolution shared by every per-key mutator. Returns the entry, or null
	 * with OutError set — distinguishing "inherited" (real key, wrong asset to edit it on) from
	 * "not found".
	 */
	FBlackboardEntry* ResolveOwnKeyForWrite(const FString& AssetPath, const FString& KeyName,
		UBlackboardData*& OutBoard, FString& OutError)
	{
		OutError = VibeBT::CheckNameLength(KeyName, TEXT("KeyName"));
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}
		OutError = BlackboardWriteGuard(AssetPath, OutBoard);
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}

		const FName Name(*KeyName);
		FBlackboardEntry* Entry = FindOwnEntry(OutBoard, Name);
		if (!Entry)
		{
			OutError = IsInheritedName(OutBoard, Name)
				? FString::Printf(TEXT("Key is inherited from the parent Blackboard: %s. Edit it on "
									   "the asset that owns it."), *KeyName)
				: FString::Printf(TEXT("Key not found: %s"), *KeyName);
			return nullptr;
		}
		return Entry;
	}

	/** Human-readable, unique-within-tree label for a BT node, used in the affected-reference report. */
	FString DescribeBTNode(const UObject* Node)
	{
		return Node ? Node->GetName() : FString(TEXT("<node>"));
	}

	/** If StructType/StructPtr is exactly FBlackboardKeySelector, record it when it matches KeyName. */
	void CheckSelectorMatch(const UStruct* StructType, const void* StructPtr, const FName& KeyName,
		const FString& BTPath, const FString& NodePath, const FString& QualifiedName,
		TArray<FString>& OutReferences)
	{
		if (StructType != FBlackboardKeySelector::StaticStruct())
		{
			return;
		}
		const FBlackboardKeySelector* Selector = static_cast<const FBlackboardKeySelector*>(StructPtr);
		if (Selector->SelectedKeyName == KeyName)
		{
			OutReferences.Add(FString::Printf(TEXT("%s:%s.%s"), *BTPath, *NodePath, *QualifiedName));
		}
	}

	/**
	 * Recursively find every FBlackboardKeySelector property on Object — including ones nested in
	 * other struct properties, and ones inside a TArray<FBlackboardKeySelector> (or an array of a
	 * struct that itself nests one) — whose SelectedKeyName matches KeyName.
	 */
	void FindKeySelectorReferencesInStruct(const UStruct* StructType, const void* StructPtr,
		const FName& KeyName, const FString& BTPath, const FString& NodePath, const FString& PropertyPrefix,
		TArray<FString>& OutReferences, int32 Depth = 0)
	{
		// Guard against runaway recursion through self-referential struct types.
		if (!StructType || !StructPtr || Depth > 3)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			FProperty* Property = *It;
			const FString QualifiedName = PropertyPrefix.IsEmpty()
				? Property->GetName()
				: PropertyPrefix + TEXT(".") + Property->GetName();

			if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				const void* ValuePtr = StructProp->ContainerPtrToValuePtr<void>(StructPtr);
				if (StructProp->Struct == FBlackboardKeySelector::StaticStruct())
				{
					CheckSelectorMatch(StructProp->Struct, ValuePtr, KeyName, BTPath, NodePath, QualifiedName,
						OutReferences);
				}
				else
				{
					FindKeySelectorReferencesInStruct(StructProp->Struct, ValuePtr, KeyName, BTPath, NodePath,
						QualifiedName, OutReferences, Depth + 1);
				}
			}
			else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner);
				if (!InnerStructProp)
				{
					continue;
				}

				const void* ArrayValuePtr = ArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayValuePtr);
				for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
				{
					const void* ElementPtr = ArrayHelper.GetRawPtr(ElementIndex);
					const FString ElementName = FString::Printf(TEXT("%s[%d]"), *QualifiedName, ElementIndex);

					if (InnerStructProp->Struct == FBlackboardKeySelector::StaticStruct())
					{
						CheckSelectorMatch(InnerStructProp->Struct, ElementPtr, KeyName, BTPath, NodePath,
							ElementName, OutReferences);
					}
					else
					{
						FindKeySelectorReferencesInStruct(InnerStructProp->Struct, ElementPtr, KeyName, BTPath,
							NodePath, ElementName, OutReferences, Depth + 1);
					}
				}
			}
		}
	}

	void FindKeySelectorReferences(const UObject* Node, const FName& KeyName, const FString& BTPath,
		TArray<FString>& OutReferences)
	{
		if (!Node)
		{
			return;
		}
		FindKeySelectorReferencesInStruct(Node->GetClass(), Node, KeyName, BTPath, DescribeBTNode(Node),
			FString(), OutReferences);
	}

	/**
	 * Walk a BT subtree — composite + its children, decorators, services and tasks — for
	 * selector references. Reads Children directly (rather than GetChildNode(), which only
	 * exposes the child node and drops FBTCompositeChild::Decorators) because decorators are
	 * where a Blackboard-based selector most commonly lives. Depth-bounded like every tree walk.
	 */
	void CollectBTNodeSelectorRefs(UBTCompositeNode* Composite, const FName& KeyName, const FString& BTPath,
		TArray<FString>& OutReferences, int32 Depth = 0)
	{
		if (!Composite || Depth > VibeBT::MaxTreeDepth)
		{
			return;
		}

		FindKeySelectorReferences(Composite, KeyName, BTPath, OutReferences);
		for (const UBTService* Service : Composite->Services)
		{
			FindKeySelectorReferences(Service, KeyName, BTPath, OutReferences);
		}

		for (const FBTCompositeChild& Child : Composite->Children)
		{
			for (const UBTDecorator* Decorator : Child.Decorators)
			{
				FindKeySelectorReferences(Decorator, KeyName, BTPath, OutReferences);
			}

			if (Child.ChildComposite)
			{
				CollectBTNodeSelectorRefs(Child.ChildComposite, KeyName, BTPath, OutReferences, Depth + 1);
			}
			else if (Child.ChildTask)
			{
				FindKeySelectorReferences(Child.ChildTask, KeyName, BTPath, OutReferences);
				for (const UBTService* Service : Child.ChildTask->Services)
				{
					FindKeySelectorReferences(Service, KeyName, BTPath, OutReferences);
				}
			}
		}
	}

	/**
	 * Every content root worth sweeping for Behavior Trees: /Game and every mounted plugin root,
	 * minus /Engine (engine content does not reference project blackboards) and the non-asset
	 * roots. Trailing slashes stripped to the "/Game" form the asset-registry filter takes.
	 */
	TArray<FString> SweepRoots()
	{
		TArray<FString> Roots;
		FPackageName::QueryRootContentPaths(Roots);
		Roots.RemoveAll([](const FString& Root)
			{
				return Root.StartsWith(TEXT("/Engine")) || Root.StartsWith(TEXT("/Script"))
					|| Root.StartsWith(TEXT("/Temp")) || Root.StartsWith(TEXT("/Memory"));
			});
		for (FString& Root : Roots)
		{
			Root.RemoveFromEnd(TEXT("/"));
		}
		return Roots;
	}

	/**
	 * Every BT selector bound to KeyName across every Behavior Tree that reads Board — directly,
	 * or via a child blackboard that inherits the key. Swept over the runtime tree: RootNode, its
	 * composites' children/decorators/services, AND UBehaviorTree::RootDecorators — root-level
	 * decorators live on the ASSET, not inside RootNode, and a sweep that misses them reports a
	 * clean bill for exactly the decorator most likely to carry a Blackboard condition.
	 */
	TArray<FString> SweepKeyReferences(UBlackboardData* Board, const FName& KeyName)
	{
		TArray<FString> References;
		for (const FString& Root : SweepRoots())
		{
			for (const FString& TreePath : VibeBT::ListAssetsOfClass(UBehaviorTree::StaticClass(), Root))
			{
				UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *TreePath, nullptr,
					LOAD_NoWarn | LOAD_Quiet);
				if (!Tree || !Tree->BlackboardAsset)
				{
					continue;
				}

				const bool bUsesBoard =
					Tree->BlackboardAsset == Board || Tree->BlackboardAsset->IsChildOf(*Board);
				if (!bUsesBoard)
				{
					continue;
				}

				for (const UBTDecorator* RootDecorator : Tree->RootDecorators)
				{
					FindKeySelectorReferences(RootDecorator, KeyName, TreePath, References);
				}
				CollectBTNodeSelectorRefs(Tree->RootNode, KeyName, TreePath, References);
			}
		}
		return References;
	}
}

TArray<FString> UBlackboardService::ListBlackboards(const FString& DirectoryPath)
{
	return VibeBT::ListAssetsOfClass(UBlackboardData::StaticClass(), DirectoryPath);
}

bool UBlackboardService::CreateBlackboard(const FString& AssetPath, const FString& ParentBlackboardPath)
{
	// The bool return has no error channel, so refusals are logged — but they are still refusals:
	// this must never overwrite. Both existence checks mirror CreateBehaviorTree's, and for the
	// same two failure shapes: an unloaded .uasset on disk would be silently replaced by the
	// package save below, and a LOADED object at this path is worse — NewObject over an existing
	// object of a different class is a fatal engine error, an editor kill.
	const FString PathError = VibeBT::CheckWritableAssetPath(AssetPath);
	if (!PathError.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateBlackboard(%s) refused: %s"), *AssetPath, *PathError);
		return false;
	}

	FString PackagePath;
	FString AssetName;
	if (!AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
		|| AssetName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateBlackboard(%s) refused: not a valid asset path"), *AssetPath);
		return false;
	}

	// Filesystem first, FindPackage second — same reasoning as CreateBehaviorTree: the on-disk
	// check catches an asset this process never loaded, FindPackage catches one created this
	// session and not yet saved, and DoesPackageExist's startup-time index would refuse paths
	// whose file was deleted out of band (every rerun of the test suite).
	FString ExistingFilename;
	const bool bAssetFileOnDisk =
		FPackageName::TryConvertLongPackageNameToFilename(
			AssetPath, ExistingFilename, FPackageName::GetAssetPackageExtension())
		&& IFileManager::Get().FileExists(*ExistingFilename);
	if (bAssetFileOnDisk || FindPackage(nullptr, *AssetPath))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("CreateBlackboard(%s) refused: an asset already exists there. Creating over it "
				 "would replace its keys; delete it first if that is really what is wanted."),
			*AssetPath);
		return false;
	}

	UBlackboardData* ParentBoard = nullptr;
	if (!ParentBlackboardPath.IsEmpty())
	{
		ParentBoard = LoadBlackboard(ParentBlackboardPath);
		if (!ParentBoard)
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateBlackboard(%s) refused: parent Blackboard not "
				"found: %s"), *AssetPath, *ParentBlackboardPath);
			return false;
		}
	}

	UPackage* Package = CreatePackage(*AssetPath);
	if (!Package)
	{
		return false;
	}

	UBlackboardData* NewBoard = NewObject<UBlackboardData>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!NewBoard)
	{
		return false;
	}

	if (ParentBoard)
	{
		NewBoard->Parent = ParentBoard;
	}

	// Run the same fix-up the engine runs after any Parent change (editor property change,
	// or PostLoad): recomputes FirstKeyID against the parent chain — required so this board's
	// key IDs don't collide with the parent's — and, for a parent-less board, leaves in place
	// the "SelfActor" key that PostInitProperties already injected (AISystem's
	// bAddBlackboardSelfKey, on by default). A hand-authored Blackboard always carries that key;
	// a service-created one must match, or a BT node bound to "SelfActor" silently fails to
	// resolve against it.
	NewBoard->UpdateParentKeys();

	FAssetRegistryModule::AssetCreated(NewBoard);

	NewBoard->Modify();
	return SaveBlackboard(NewBoard).IsEmpty();
}

TArray<FBBKeyInfo> UBlackboardService::GetBlackboardKeys(const FString& AssetPath)
{
	TArray<FBBKeyInfo> Result;

	UBlackboardData* Board = LoadBlackboard(AssetPath);
	if (!Board)
	{
		return Result;
	}

	auto AppendEntries = [&Result](const TArray<FBlackboardEntry>& Entries, bool bInherited)
	{
		for (const FBlackboardEntry& Entry : Entries)
		{
			FBBKeyInfo Info;
			Info.Name = Entry.EntryName.ToString();
			Info.Type = KeyTypeToName(Entry.KeyType);
			Info.bInstanceSynced = Entry.bInstanceSynced;
			Info.bInherited = bInherited;
#if WITH_EDITORONLY_DATA
			Info.Category = Entry.EntryCategory.ToString();
			if (Info.Category == TEXT("None"))
			{
				Info.Category.Reset();
			}
			Info.Description = Entry.EntryDescription;
#endif

			if (const UBlackboardKeyType_Object* ObjectType = Cast<UBlackboardKeyType_Object>(Entry.KeyType))
			{
				Info.ObjectClassPath = ObjectType->BaseClass ? ObjectType->BaseClass->GetPathName() : FString();
			}
			else if (const UBlackboardKeyType_Class* ClassType = Cast<UBlackboardKeyType_Class>(Entry.KeyType))
			{
				Info.ObjectClassPath = ClassType->BaseClass ? ClassType->BaseClass->GetPathName() : FString();
			}
			else if (const UBlackboardKeyType_Enum* EnumType = Cast<UBlackboardKeyType_Enum>(Entry.KeyType))
			{
				Info.ObjectClassPath = EnumType->EnumType ? EnumType->EnumType->GetPathName() : FString();
			}

			Result.Add(MoveTemp(Info));
		}
	};

	AppendEntries(Board->Keys, false);

	TArray<FBlackboardEntry> Inherited;
	CollectInheritedKeys(Board, Inherited);
	AppendEntries(Inherited, true);

	return Result;
}

FString UBlackboardService::AddBlackboardKey(const FString& AssetPath, const FString& KeyName,
	const FString& KeyType, bool bInstanceSynced)
{
	if (KeyName.IsEmpty())
	{
		return TEXT("KeyName is empty");
	}
	const FString LengthError = VibeBT::CheckNameLength(KeyName, TEXT("KeyName"));
	if (!LengthError.IsEmpty())
	{
		return LengthError;
	}

	UBlackboardData* Board = nullptr;
	const FString GuardError = BlackboardWriteGuard(AssetPath, Board);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	const FName Name(*KeyName);
	if (FindOwnEntry(Board, Name) || IsInheritedName(Board, Name))
	{
		return FString::Printf(TEXT("Key already exists: %s"), *KeyName);
	}

	UClass* TypeClass = ResolveKeyTypeClass(KeyType);
	if (!TypeClass)
	{
		return FString::Printf(TEXT("Unknown key type: %s. Valid types: Bool, Int, Float, String, "
			"Name, Vector, Rotator, Object, Class, Enum, NativeEnum."), *KeyType);
	}

	Board->Modify();
	FBlackboardEntry Entry;
	Entry.EntryName = Name;
	Entry.KeyType = NewObject<UBlackboardKeyType>(Board, TypeClass, NAME_None, RF_Transactional);
	Entry.bInstanceSynced = bInstanceSynced;
	Board->Keys.Add(Entry);
	RunKeyChangeFixups(Board);

	return SaveBlackboard(Board);
}

FString UBlackboardService::SetBlackboardKeyObjectClass(const FString& AssetPath, const FString& KeyName,
	const FString& ClassOrEnumPath)
{
	const FString PathLengthError = VibeBT::CheckNameLength(ClassOrEnumPath, TEXT("ClassOrEnumPath"));
	if (!PathLengthError.IsEmpty())
	{
		return PathLengthError;
	}

	UBlackboardData* Board = nullptr;
	FString Error;
	FBlackboardEntry* Entry = ResolveOwnKeyForWrite(AssetPath, KeyName, Board, Error);
	if (!Entry)
	{
		return Error;
	}

	Board->Modify();
	if (UBlackboardKeyType_Object* ObjectType = Cast<UBlackboardKeyType_Object>(Entry->KeyType))
	{
		UClass* Class = LoadObject<UClass>(nullptr, *ClassOrEnumPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Class)
		{
			return FString::Printf(TEXT("Class not found: %s"), *ClassOrEnumPath);
		}
		ObjectType->Modify();
		ObjectType->BaseClass = Class;
	}
	else if (UBlackboardKeyType_Class* ClassType = Cast<UBlackboardKeyType_Class>(Entry->KeyType))
	{
		UClass* Class = LoadObject<UClass>(nullptr, *ClassOrEnumPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Class)
		{
			return FString::Printf(TEXT("Class not found: %s"), *ClassOrEnumPath);
		}
		ClassType->Modify();
		ClassType->BaseClass = Class;
	}
	else if (UBlackboardKeyType_Enum* EnumType = Cast<UBlackboardKeyType_Enum>(Entry->KeyType))
	{
		UEnum* Enum = LoadObject<UEnum>(nullptr, *ClassOrEnumPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Enum)
		{
			return FString::Printf(TEXT("Enum not found: %s"), *ClassOrEnumPath);
		}
		EnumType->Modify();
		EnumType->EnumType = Enum;
	}
	else
	{
		return FString::Printf(TEXT("Key '%s' is of type %s, which does not take a class or enum"),
			*KeyName, *KeyTypeToName(ToRawPtr(Entry->KeyType)));
	}

	RunKeyChangeFixups(Board);
	return SaveBlackboard(Board);
}

FString UBlackboardService::SetBlackboardKeyInstanceSynced(const FString& AssetPath,
	const FString& KeyName, bool bInstanceSynced)
{
	UBlackboardData* Board = nullptr;
	FString Error;
	FBlackboardEntry* Entry = ResolveOwnKeyForWrite(AssetPath, KeyName, Board, Error);
	if (!Entry)
	{
		return Error;
	}

	Board->Modify();
	Entry->bInstanceSynced = bInstanceSynced;
	RunKeyChangeFixups(Board);
	return SaveBlackboard(Board);
}

FString UBlackboardService::SetBlackboardKeyMetadata(const FString& AssetPath, const FString& KeyName,
	const FString& Category, const FString& Description)
{
	const FString CategoryLengthError = VibeBT::CheckNameLength(Category, TEXT("Category"));
	if (!CategoryLengthError.IsEmpty())
	{
		return CategoryLengthError;
	}

	UBlackboardData* Board = nullptr;
	FString Error;
	FBlackboardEntry* Entry = ResolveOwnKeyForWrite(AssetPath, KeyName, Board, Error);
	if (!Entry)
	{
		return Error;
	}

#if WITH_EDITORONLY_DATA
	Board->Modify();
	Entry->EntryCategory = Category.IsEmpty() ? NAME_None : FName(*Category);
	Entry->EntryDescription = Description;
	// Metadata changes nothing derived (no key IDs, no synchronized-key state), but an open
	// editor on a derived asset still shows it, so the broadcast is kept.
	UBlackboardData::OnUpdateKeys.Broadcast(Board);
	return SaveBlackboard(Board);
#else
	return TEXT("Key metadata is editor-only data and this build carries no editor data.");
#endif
}

FString UBlackboardService::SetBlackboardParent(const FString& AssetPath,
	const FString& ParentBlackboardPath)
{
	UBlackboardData* Board = nullptr;
	const FString GuardError = BlackboardWriteGuard(AssetPath, Board);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	UBlackboardData* NewParent = nullptr;
	if (!ParentBlackboardPath.IsEmpty())
	{
		NewParent = LoadBlackboard(ParentBlackboardPath);
		if (!NewParent)
		{
			return FString::Printf(TEXT("Parent Blackboard not found: %s"), *ParentBlackboardPath);
		}
		if (NewParent == Board)
		{
			return FString::Printf(TEXT("%s cannot be its own parent"), *AssetPath);
		}
		// The cycle the Blackboard editor detects and silently CLEARS (PostEditChangeProperty:
		// "Clearing value to avoid cycle") is an error here — a service must not report success
		// for an assignment it undid.
		if (ParentChainContains(NewParent, Board))
		{
			return FString::Printf(
				TEXT("%s is already in %s's parent chain; making it the parent would create a "
					 "cycle"),
				*AssetPath, *ParentBlackboardPath);
		}

		// A same-named key on both sides is not a merge: the child's key SHADOWS the parent's
		// (UBlackboardData::GetKey resolves child-first), silently re-typing what every selector
		// bound through the parent reads. Refused, with the collisions named, rather than left to
		// be discovered at runtime.
		//
		// The engine-persistent self key is exempt: every parent-less board owns one by engine
		// injection, and UpdateParentKeys -> UpdatePersistentKeys RECONCILES it on reparent
		// ("removes this BB asset's persistent keys that double the ones already present in the
		// parent", BlackboardData.cpp:264-265) rather than shadowing it. Counting it would refuse
		// every reparent of every ordinary board.
		TArray<FString> Collisions;
		for (const FBlackboardEntry& Own : Board->Keys)
		{
			if (Own.EntryName == FBlackboard::KeySelf)
			{
				continue;
			}
			int32 Depth = 0;
			for (const UBlackboardData* Ancestor = NewParent;
				Ancestor && Depth < kMaxParentDepth; Ancestor = Ancestor->Parent, ++Depth)
			{
				if (Ancestor->Keys.ContainsByPredicate([&Own](const FBlackboardEntry& Entry)
					{ return Entry.EntryName == Own.EntryName; }))
				{
					Collisions.Add(Own.EntryName.ToString());
					break;
				}
			}
		}
		if (Collisions.Num() > 0)
		{
			return FString::Printf(
				TEXT("%s owns key(s) the new parent chain also defines: %s. Same-named keys shadow "
					 "the parent's — silently, per key — so re-point or rename them first."),
				*AssetPath, *FString::Join(Collisions, TEXT(", ")));
		}
	}

	Board->Modify();
	Board->Parent = NewParent;
	// The engine's own Parent-change fix-ups (PostEditChangeProperty order): recompute
	// FirstKeyID/ParentKeys against the new chain, refresh the synchronized-key flag, and let
	// derived boards and listeners know.
	Board->UpdateParentKeys();
	Board->UpdateIfHasSynchronizedKeys();
	Board->PropagateKeyChangesToDerivedBlackboardAssets();
	UBlackboardData::OnUpdateKeys.Broadcast(Board);

	return SaveBlackboard(Board);
}

FString UBlackboardService::RemoveBlackboardKey(const FString& AssetPath, const FString& KeyName)
{
	UBlackboardData* Board = nullptr;
	FString Error;
	FBlackboardEntry* Entry = ResolveOwnKeyForWrite(AssetPath, KeyName, Board, Error);
	if (!Entry)
	{
		return Error;
	}

	// The engine-persistent self key: UpdatePersistentKey<UBlackboardKeyType_Object>(KeySelf)
	// re-injects it on every parent-less board at PostInitProperties/PostLoad, so removing it
	// "succeeds" and reads back undone after the next reload — the write-that-reads-back-different
	// failure mode this service exists to refuse.
	const FName Name(*KeyName);
	if (Name == FBlackboard::KeySelf && !Board->Parent)
	{
		return FString::Printf(
			TEXT("'%s' is the engine-persistent self key: UBlackboardData re-injects it on load, so "
				 "the removal would silently read back undone."),
			*KeyName);
	}

	// Selectors resolve by name, so bindings to the removed key break with no report of their own.
	// The removal is still performed — that is what was asked — but the blast radius is logged,
	// and FindBlackboardKeyReferences answers the same question before the fact.
	const TArray<FString> Broken = SweepKeyReferences(Board, Name);
	for (const FString& Reference : Broken)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RemoveBlackboardKey(%s, %s): breaks selector binding %s"),
			*AssetPath, *KeyName, *Reference);
	}

	const int32 Index = Board->Keys.IndexOfByPredicate(
		[&Name](const FBlackboardEntry& Entry2) { return Entry2.EntryName == Name; });
	if (Index == INDEX_NONE)
	{
		return FString::Printf(TEXT("Key not found: %s"), *KeyName);
	}

	Board->Modify();
	Board->Keys.RemoveAt(Index);
	RunKeyChangeFixups(Board);

	return SaveBlackboard(Board);
}

TArray<FString> UBlackboardService::FindBlackboardKeyReferences(const FString& AssetPath,
	const FString& KeyName)
{
	// Same sentinel convention as RenameBlackboardKey: a failure is one "ERROR: " entry, which a
	// real reference — always "<btPath>:<node>.<property>" with btPath under a content root —
	// can never look like.
	if (KeyName.IsEmpty())
	{
		return { TEXT("ERROR: KeyName is empty") };
	}
	const FString LengthError = VibeBT::CheckNameLength(KeyName, TEXT("KeyName"));
	if (!LengthError.IsEmpty())
	{
		return { FString::Printf(TEXT("ERROR: %s"), *LengthError) };
	}

	UBlackboardData* Board = LoadBlackboard(AssetPath);
	if (!Board)
	{
		return { FString::Printf(TEXT("ERROR: Blackboard not found: %s"), *AssetPath) };
	}

	return SweepKeyReferences(Board, FName(*KeyName));
}

TArray<FString> UBlackboardService::RenameBlackboardKey(const FString& AssetPath, const FString& OldName,
	const FString& NewName)
{
	// A rejected rename and a successful-but-unreferenced rename both have "nothing to report",
	// which would otherwise both come back as an empty array. To keep the two distinguishable
	// without changing the return type, a rejection returns a single sentinel entry prefixed
	// "ERROR: " — a shape a real "<btPath>:<node>.<property>" reference can never take, since a
	// BT package path always starts with a content root. A genuine, unreferenced success still
	// returns a plain empty array.
	if (OldName.IsEmpty() || NewName.IsEmpty() || OldName == NewName)
	{
		return { TEXT("ERROR: OldName and NewName must both be set and different") };
	}
	for (const FString& Candidate : { OldName, NewName })
	{
		const FString LengthError = VibeBT::CheckNameLength(Candidate, TEXT("key name"));
		if (!LengthError.IsEmpty())
		{
			return { FString::Printf(TEXT("ERROR: %s"), *LengthError) };
		}
	}

	UBlackboardData* Board = nullptr;
	const FString GuardError = BlackboardWriteGuard(AssetPath, Board);
	if (!GuardError.IsEmpty())
	{
		return { FString::Printf(TEXT("ERROR: %s"), *GuardError) };
	}

	const FName OldKeyName(*OldName);
	const FName NewKeyName(*NewName);

	FBlackboardEntry* Entry = FindOwnEntry(Board, OldKeyName);
	if (!Entry)
	{
		// Missing entirely, or only present on the parent chain — nothing this asset owns to rename.
		if (IsInheritedName(Board, OldKeyName))
		{
			return { FString::Printf(TEXT("ERROR: Key is inherited from the parent Blackboard: %s"), *OldName) };
		}
		return { FString::Printf(TEXT("ERROR: Key not found: %s"), *OldName) };
	}

	if (FindOwnEntry(Board, NewKeyName) || IsInheritedName(Board, NewKeyName))
	{
		// New name collides with an existing key — refuse rather than silently merging two keys.
		return { FString::Printf(TEXT("ERROR: Key already exists: %s"), *NewName) };
	}

	Board->Modify();
	Entry->EntryName = NewKeyName;
	RunKeyChangeFixups(Board);
	const FString SaveError = SaveBlackboard(Board);
	if (!SaveError.IsEmpty())
	{
		// SaveBlackboard has already discarded the failed in-memory state back to disk, so no
		// manual rollback is needed — and the BT sweep must not run against a rename that never
		// took effect.
		return { FString::Printf(TEXT("ERROR: %s"), *SaveError) };
	}

	// Sweep every BT that reads this blackboard — directly, or via a child blackboard that
	// still inherits the renamed key — for key selectors still pointing at the old name.
	return SweepKeyReferences(Board, OldKeyName);
}
