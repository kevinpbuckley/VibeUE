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
#include "FileHelpers.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
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
		return LoadObject<UBlackboardData>(nullptr, *AssetPath);
	}

	/** Save through the package so the write actually reaches disk. */
	bool SaveBlackboard(UBlackboardData* Board)
	{
		Board->Modify();
		Board->MarkPackageDirty();
		return UEditorLoadingAndSavingUtils::SavePackages({ Board->GetOutermost() }, /*bOnlyDirty*/ false);
	}

	/** Find an entry this asset owns directly (not inherited) by name. */
	FBlackboardEntry* FindOwnEntry(UBlackboardData* Board, const FName& KeyName)
	{
		return Board->Keys.FindByPredicate(
			[&KeyName](const FBlackboardEntry& Entry) { return Entry.EntryName == KeyName; });
	}

	/**
	 * Every key inherited from Board's parent chain, walked manually rather than through the
	 * cached UBlackboardData::ParentKeys. ParentKeys is only refreshed by UpdateParentKeys()
	 * (called once, at creation, by CreateBlackboard) or by the engine's own PostLoad/
	 * PostEditChangeProperty; re-calling UpdateParentKeys() here on every read would also re-run
	 * UpdatePersistentKeys(), which is idempotent once the board matches engine expectations but
	 * is unnecessary work this service doesn't need to repeat on every query. Walking Keys
	 * directly is always correct regardless of cache freshness. Child keys shadow a same-named
	 * parent key, matching UBlackboardData::GetKey/InternalGetKeyID.
	 */
	void CollectInheritedKeys(const UBlackboardData* Board, TArray<FBlackboardEntry>& OutInherited)
	{
		TSet<FName> Seen;
		for (const FBlackboardEntry& Entry : Board->Keys)
		{
			Seen.Add(Entry.EntryName);
		}
		for (const UBlackboardData* Ancestor = Board->Parent; Ancestor; Ancestor = Ancestor->Parent)
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
		for (const UBlackboardData* Ancestor = Board->Parent; Ancestor; Ancestor = Ancestor->Parent)
		{
			if (Ancestor->Keys.ContainsByPredicate(
				[&KeyName](const FBlackboardEntry& Entry) { return Entry.EntryName == KeyName; }))
			{
				return true;
			}
		}
		return false;
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
	 * where a Blackboard-based selector most commonly lives: UBTDecorator_BlackboardBase's
	 * BlackboardKey is the base for every Blackboard-driven condition.
	 */
	void CollectBTNodeSelectorRefs(UBTCompositeNode* Composite, const FName& KeyName, const FString& BTPath,
		TArray<FString>& OutReferences)
	{
		if (!Composite)
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
				CollectBTNodeSelectorRefs(Child.ChildComposite, KeyName, BTPath, OutReferences);
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
}

TArray<FString> UBlackboardService::ListBlackboards(const FString& DirectoryPath)
{
	return VibeBT::ListAssetsOfClass(UBlackboardData::StaticClass(), DirectoryPath);
}

bool UBlackboardService::CreateBlackboard(const FString& AssetPath, const FString& ParentBlackboardPath)
{
	if (AssetPath.IsEmpty())
	{
		return false;
	}

	FString PackagePath;
	FString AssetName;
	if (!AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		return false;
	}
	if (AssetName.IsEmpty())
	{
		return false;
	}

	UBlackboardData* ParentBoard = nullptr;
	if (!ParentBlackboardPath.IsEmpty())
	{
		ParentBoard = LoadBlackboard(ParentBlackboardPath);
		if (!ParentBoard)
		{
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
	// bAddBlackboardSelfKey, on by default, not overridden in this project). A hand-authored
	// Blackboard always carries that key; a service-created one must match, or a BT node bound
	// to "SelfActor" silently fails to resolve against it.
	NewBoard->UpdateParentKeys();

	FAssetRegistryModule::AssetCreated(NewBoard);

	return SaveBlackboard(NewBoard);
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

	UBlackboardData* Board = LoadBlackboard(AssetPath);
	if (!Board)
	{
		return FString::Printf(TEXT("Blackboard not found: %s"), *AssetPath);
	}

	const FName Name(*KeyName);
	if (FindOwnEntry(Board, Name) || IsInheritedName(Board, Name))
	{
		return FString::Printf(TEXT("Key already exists: %s"), *KeyName);
	}

	UClass* TypeClass = ResolveKeyTypeClass(KeyType);
	if (!TypeClass)
	{
		return FString::Printf(TEXT("Unknown key type: %s"), *KeyType);
	}

	FBlackboardEntry Entry;
	Entry.EntryName = Name;
	Entry.KeyType = NewObject<UBlackboardKeyType>(Board, TypeClass);
	Entry.bInstanceSynced = bInstanceSynced;
	Board->Keys.Add(Entry);

	if (!SaveBlackboard(Board))
	{
		return TEXT("Failed to save Blackboard asset");
	}
	return FString();
}

FString UBlackboardService::SetBlackboardKeyObjectClass(const FString& AssetPath, const FString& KeyName,
	const FString& ClassOrEnumPath)
{
	UBlackboardData* Board = LoadBlackboard(AssetPath);
	if (!Board)
	{
		return FString::Printf(TEXT("Blackboard not found: %s"), *AssetPath);
	}

	const FName Name(*KeyName);
	FBlackboardEntry* Entry = FindOwnEntry(Board, Name);
	if (!Entry)
	{
		if (IsInheritedName(Board, Name))
		{
			return FString::Printf(TEXT("Key is inherited from the parent Blackboard: %s"), *KeyName);
		}
		return FString::Printf(TEXT("Key not found: %s"), *KeyName);
	}

	if (UBlackboardKeyType_Object* ObjectType = Cast<UBlackboardKeyType_Object>(Entry->KeyType))
	{
		UClass* Class = LoadObject<UClass>(nullptr, *ClassOrEnumPath);
		if (!Class)
		{
			return FString::Printf(TEXT("Class not found: %s"), *ClassOrEnumPath);
		}
		ObjectType->BaseClass = Class;
	}
	else if (UBlackboardKeyType_Class* ClassType = Cast<UBlackboardKeyType_Class>(Entry->KeyType))
	{
		UClass* Class = LoadObject<UClass>(nullptr, *ClassOrEnumPath);
		if (!Class)
		{
			return FString::Printf(TEXT("Class not found: %s"), *ClassOrEnumPath);
		}
		ClassType->BaseClass = Class;
	}
	else if (UBlackboardKeyType_Enum* EnumType = Cast<UBlackboardKeyType_Enum>(Entry->KeyType))
	{
		UEnum* Enum = LoadObject<UEnum>(nullptr, *ClassOrEnumPath);
		if (!Enum)
		{
			return FString::Printf(TEXT("Enum not found: %s"), *ClassOrEnumPath);
		}
		EnumType->EnumType = Enum;
	}
	else
	{
		return FString::Printf(TEXT("Key '%s' does not take a class or enum"), *KeyName);
	}

	if (!SaveBlackboard(Board))
	{
		return TEXT("Failed to save Blackboard asset");
	}
	return FString();
}

FString UBlackboardService::RemoveBlackboardKey(const FString& AssetPath, const FString& KeyName)
{
	UBlackboardData* Board = LoadBlackboard(AssetPath);
	if (!Board)
	{
		return FString::Printf(TEXT("Blackboard not found: %s"), *AssetPath);
	}

	const FName Name(*KeyName);
	const int32 Index = Board->Keys.IndexOfByPredicate(
		[&Name](const FBlackboardEntry& Entry) { return Entry.EntryName == Name; });
	if (Index == INDEX_NONE)
	{
		if (IsInheritedName(Board, Name))
		{
			return FString::Printf(TEXT("Key is inherited from the parent Blackboard: %s"), *KeyName);
		}
		return FString::Printf(TEXT("Key not found: %s"), *KeyName);
	}

	Board->Keys.RemoveAt(Index);

	if (!SaveBlackboard(Board))
	{
		return TEXT("Failed to save Blackboard asset");
	}
	return FString();
}

TArray<FString> UBlackboardService::RenameBlackboardKey(const FString& AssetPath, const FString& OldName,
	const FString& NewName)
{
	// A rejected rename and a successful-but-unreferenced rename both have "nothing to report",
	// which would otherwise both come back as an empty array. To keep the two distinguishable
	// without changing the return type, a rejection returns a single sentinel entry prefixed
	// "ERROR: " — a shape a real "<btPath>:<node>.<property>" reference can never take, since a
	// BT package path always starts with "/Game/...". A genuine, unreferenced success still
	// returns a plain empty array.
	if (OldName.IsEmpty() || NewName.IsEmpty() || OldName == NewName)
	{
		return { TEXT("ERROR: OldName and NewName must both be set and different") };
	}

	UBlackboardData* Board = LoadBlackboard(AssetPath);
	if (!Board)
	{
		return { FString::Printf(TEXT("ERROR: Blackboard not found: %s"), *AssetPath) };
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

	Entry->EntryName = NewKeyName;
	if (!SaveBlackboard(Board))
	{
		// Roll back the in-memory rename so it doesn't diverge from what's actually on disk,
		// and don't run the BT sweep against a rename that never took effect.
		Entry->EntryName = OldKeyName;
		return { TEXT("ERROR: Failed to save Blackboard asset") };
	}

	TArray<FString> AffectedReferences;

	// Sweep every BT that reads this blackboard — directly, or via a child blackboard that
	// still inherits the renamed key — for key selectors still pointing at the old name.
	const TArray<FString> TreePaths = VibeBT::ListAssetsOfClass(UBehaviorTree::StaticClass(), TEXT("/Game"));
	for (const FString& TreePath : TreePaths)
	{
		UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *TreePath);
		if (!Tree || !Tree->BlackboardAsset)
		{
			continue;
		}

		const bool bUsesBoard = Tree->BlackboardAsset == Board || Tree->BlackboardAsset->IsChildOf(*Board);
		if (!bUsesBoard)
		{
			continue;
		}

		CollectBTNodeSelectorRefs(Tree->RootNode, OldKeyName, TreePath, AffectedReferences);
	}

	return AffectedReferences;
}
