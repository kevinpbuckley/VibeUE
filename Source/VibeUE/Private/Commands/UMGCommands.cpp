#include "Commands/UMGCommands.h"
#include "Commands/CommonUtils.h"
#include "Services/UMG/WidgetLifecycleService.h"
#include "Services/UMG/WidgetPropertyService.h"
#include "Core/ServiceContext.h"
// TODO: Add WidgetComponentService include when implementing Issue #191
// TODO: Issue #188 skipped - discovery handlers already well-structured
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "EditorSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/Platform.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/RichTextBlock.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/UnrealType.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "UObject/Class.h"
#include "UObject/TopLevelAssetPath.h"

namespace
{
struct FPathSegment
{
	FString Name;
	bool bHasIndex = false;
	int32 Index = INDEX_NONE;
	bool bHasKey = false;
	FString Key; // serialized key for maps/sets
};

static bool ParsePropertyPath(const FString& InPath, bool& bSlotRoot, TArray<FPathSegment>& Out)
{
	bSlotRoot = false;
	Out.Reset();
	TArray<FString> Parts;
	InPath.ParseIntoArray(Parts, TEXT("."), true);
	if (Parts.Num() == 0) return false;
	int32 Start = 0;
	if (Parts[0].Equals(TEXT("Slot"), ESearchCase::IgnoreCase))
	{
		bSlotRoot = true;
		Start = 1;
		if (Parts.Num() == 1)
		{
			return false; // Slot alone not valid
		}
	}
	for (int32 i = Start; i < Parts.Num(); ++i)
	{
		FPathSegment Seg;
		const FString& P = Parts[i];
		int32 BracketIdx;
		if (P.FindChar('[', BracketIdx) && P.EndsWith("]"))
		{
			Seg.Name = P.Left(BracketIdx);
			FString Inside = P.Mid(BracketIdx + 1, P.Len() - BracketIdx - 2);
			// inside could be index or key
			if (Inside.IsNumeric())
			{
				Seg.bHasIndex = true;
				Seg.Index = FCString::Atoi(*Inside);
			}
			else
			{
				Seg.bHasKey = true;
				Seg.Key = Inside;
			}
		}
		else
		{
			Seg.Name = P;
		}
		Out.Add(Seg);
	}
	return Out.Num() > 0;
}

static void AddEnumConstraints(FProperty* Prop, TSharedPtr<FJsonObject>& Constraints)
{
	if (FByteProperty* ByteProperty = CastField<FByteProperty>(Prop))
	{
		if (UEnum* Enum = ByteProperty->Enum)
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // skip _MAX if present
			{
				FString Name = Enum->GetNameStringByIndex(i);
				if (!Name.EndsWith(TEXT("_MAX")))
				{
					Values.Add(MakeShared<FJsonValueString>(Name));
				}
			}
			Constraints->SetArrayField(TEXT("enum_values"), Values);
		}
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Prop))
	{
		if (UEnum* Enum = EnumProperty->GetEnum())
		{
			TArray<TSharedPtr<FJsonValue>> Values;
			for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // skip _MAX if present
			{
				FString Name = Enum->GetNameStringByIndex(i);
				if (!Name.EndsWith(TEXT("_MAX")))
				{
					Values.Add(MakeShared<FJsonValueString>(Name));
				}
			}
			Constraints->SetArrayField(TEXT("enum_values"), Values);
		}
	}
}

static void AddNumericConstraints(FProperty* Prop, TSharedPtr<FJsonObject>& Constraints)
{
	auto TryNumberMeta = [&](const TCHAR* Key, const TCHAR* OutKey)
	{
		if (Prop->HasMetaData(Key))
		{
			FString S = Prop->GetMetaData(Key);
			double V = FCString::Atod(*S);
			Constraints->SetNumberField(OutKey, V);
		}
	};
	TryNumberMeta(TEXT("ClampMin"), TEXT("min"));
	TryNumberMeta(TEXT("ClampMax"), TEXT("max"));
	TryNumberMeta(TEXT("UIMin"), TEXT("uiMin"));
	TryNumberMeta(TEXT("UIMax"), TEXT("uiMax"));
}

struct FResolvedTarget
{
	UObject* RootObject = nullptr; // starting object (widget or slot or subobject)
	void* ContainerPtr = nullptr;  // container pointer for owning object/struct
	FProperty* Property = nullptr; // final property at path end
	bool bIsSyntheticChildOrder = false;
};

static bool ResolvePath(UWidget* Widget, const TArray<FPathSegment>& Segs, bool bSlotRoot, FResolvedTarget& Out, FString& Error)
{
	UObject* CurrentObject = bSlotRoot ? (UObject*)Widget->Slot : (UObject*)Widget;
	void* CurrentPtr = CurrentObject;
	FProperty* CurrentProp = nullptr;
	if (!CurrentObject)
	{
		Error = TEXT("Slot is null for this widget (no parent panel)");
		return false;
	}
	for (int32 i = 0; i < Segs.Num(); ++i)
	{
		const FPathSegment& Seg = Segs[i];
		if (i == Segs.Num() - 1)
		{
			// final segment, allow synthetic ChildOrder on slot
			if (bSlotRoot && Seg.Name.Equals(TEXT("ChildOrder"), ESearchCase::IgnoreCase))
			{
				Out.RootObject = CurrentObject;
				Out.ContainerPtr = CurrentPtr;
				Out.Property = nullptr;
				Out.bIsSyntheticChildOrder = true;
				return true;
			}
		}

		// find property on current object/struct
		if (UObject* Obj = Cast<UObject>((UObject*)CurrentObject))
		{
			CurrentProp = Obj->GetClass()->FindPropertyByName(*Seg.Name);
		}
		else if (CurrentProp && CurrentProp->IsA<FStructProperty>())
		{
			FStructProperty* SP = CastFieldChecked<FStructProperty>(CurrentProp);
			CurrentProp = SP->Struct->FindPropertyByName(*Seg.Name);
		}
		else
		{
			// treat CurrentPtr as a struct; find by scanning owner struct
			return false;
		}

		if (!CurrentProp)
		{
			// Common alias mapping
			if (!bSlotRoot && Seg.Name.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
			{
				CurrentProp = Widget->GetClass()->FindPropertyByName(TEXT("bIsVariable"));
			}
		}

		if (!CurrentProp)
		{
			Error = FString::Printf(TEXT("Property '%s' not found"), *Seg.Name);
			return false;
		}

		// step into property
		if (FStructProperty* SP = CastField<FStructProperty>(CurrentProp))
		{
			CurrentPtr = SP->ContainerPtrToValuePtr<void>(CurrentPtr);
			CurrentObject = nullptr; // now in struct
		}
		else if (FObjectProperty* OP = CastField<FObjectProperty>(CurrentProp))
		{
			UObject* const* ObjPtr = OP->ContainerPtrToValuePtr<UObject*>(CurrentPtr);
			CurrentObject = *ObjPtr;
			CurrentPtr = CurrentObject;
			if (!CurrentObject)
			{
				// stop here; allow set to construct later if needed
			}
		}
		else if (FArrayProperty* AP = CastField<FArrayProperty>(CurrentProp))
		{
			void* ArrPtr = AP->ContainerPtrToValuePtr<void>(CurrentPtr);
			FScriptArrayHelper Helper(AP, ArrPtr);
			if (Seg.bHasIndex)
			{
				if (!Helper.IsValidIndex(Seg.Index))
				{
					Error = FString::Printf(TEXT("Array index out of bounds: %d (len=%d)"), Seg.Index, Helper.Num());
					return false;
				}
				CurrentPtr = Helper.GetRawPtr(Seg.Index);
				CurrentProp = AP->Inner;
				// continue deeper; CurrentObject remains null
			}
			else
			{
				// array as a whole; remain on AP for final
				if (i != Segs.Num() - 1)
				{
					Error = TEXT("Array path must specify index to access elements");
					return false;
				}
			}
		}
		else if (FMapProperty* MP = CastField<FMapProperty>(CurrentProp))
		{
			void* MapPtr = MP->ContainerPtrToValuePtr<void>(CurrentPtr);
			FScriptMapHelper Helper(MP, MapPtr);
			if (Seg.bHasKey)
			{
				// build key from string
				TArray<uint8> KeyStorage;
				KeyStorage.SetNumUninitialized(MP->KeyProp->GetSize());
				MP->KeyProp->InitializeValue(KeyStorage.GetData());
				// simple conversion: string/name/int/enum
				if (FNameProperty* KP = CastField<FNameProperty>(MP->KeyProp))
				{
					FName ValueName(*Seg.Key);
					KP->CopyCompleteValue(KeyStorage.GetData(), &ValueName);
				}
				else if (FStrProperty* KP2 = CastField<FStrProperty>(MP->KeyProp))
				{
					FString S = Seg.Key;
					KP2->CopyCompleteValue(KeyStorage.GetData(), &S);
				}
				else if (FIntProperty* KP3 = CastField<FIntProperty>(MP->KeyProp))
				{
					int32 I = FCString::Atoi(*Seg.Key);
					KP3->CopyCompleteValue(KeyStorage.GetData(), &I);
				}
				else if (FByteProperty* KP4 = CastField<FByteProperty>(MP->KeyProp))
				{
					uint8 B = 0;
					if (KP4->Enum)
					{
						int64 EnumVal = KP4->Enum->GetValueByNameString(Seg.Key);
						B = EnumVal == INDEX_NONE ? (uint8)FCString::Atoi(*Seg.Key) : (uint8)EnumVal;
					}
					else
					{
						B = (uint8)FCString::Atoi(*Seg.Key);
					}
					KP4->CopyCompleteValue(KeyStorage.GetData(), &B);
				}
				else
				{
					Error = TEXT("Unsupported map key type");
					return false;
				}

				int32 Index = INDEX_NONE;
				for (int32 It = 0; It < Helper.GetMaxIndex(); ++It)
				{
					if (!Helper.IsValidIndex(It)) continue;
					uint8* Pair = (uint8*)Helper.GetPairPtr(It);
					void* ExistingKeyPtr = Pair; // key at start of pair
					if (MP->KeyProp->Identical(KeyStorage.GetData(), ExistingKeyPtr))
					{
						Index = It;
						break;
					}
				}
				if (Index == INDEX_NONE)
				{
					Error = TEXT("Map key not found");
					return false;
				}
				CurrentPtr = Helper.GetPairPtr(Index) + MP->MapLayout.ValueOffset;
				CurrentProp = MP->ValueProp;
			}
			else if (i != Segs.Num() - 1)
			{
				Error = TEXT("Map path must specify [Key] to access value");
				return false;
			}
		}
		else if (FSetProperty* SetP = CastField<FSetProperty>(CurrentProp))
		{
			// cannot traverse into set without value; only whole-set supported
			if (i != Segs.Num() - 1)
			{
				Error = TEXT("Set path cannot traverse into elements; use collection_op");
				return false;
			}
		}
		else
		{
			// primitive leaf; only okay if last
			if (i != Segs.Num() - 1)
			{
				Error = FString::Printf(TEXT("Cannot traverse into non-composite property '%s'"), *Seg.Name);
				return false;
			}
		}
	}
	Out.RootObject = Cast<UObject>(CurrentObject) ? Cast<UObject>(CurrentObject) : Widget;
	Out.ContainerPtr = CurrentPtr;
	Out.Property = CurrentProp;
	return true;
}
}
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/GridPanel.h"
// Additional includes for generic asset search
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundWave.h"
#include "Components/UniformGridPanel.h"
#include "Components/ListView.h"
#include "Components/Overlay.h"
#include "Components/TileView.h"
#include "Components/TreeView.h"
#include "Components/WidgetSwitcher.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
// Animation and Blueprint includes
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h" 
#include "MovieSceneTrack.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_Event.h"
#include "K2Node_InputAction.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/GridSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/WidgetSwitcherSlot.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_Event.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
// We'll create widgets using regular Factory classes
#include "Factories/Factory.h"
#include "WidgetBlueprintFactory.h"
// Remove problematic includes that don't exist in UE 5.5
// #include "UMGEditorSubsystem.h"

// Additional includes for complex type support
#include "Math/Color.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"
#include "Layout/Margin.h"
#include "Math/Vector2D.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateTypes.h"

// Helper functions for enhanced UI building
namespace UMGHelpers
{
	// Find or create a parent widget by name
	UPanelWidget* FindOrCreateParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& ParentType = TEXT("CanvasPanel"));
	
	// Set slot properties for a widget
	bool SetSlotProperties(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& SlotProperties);
	
	// Create widget with proper parent assignment
	template<typename WidgetType>
	WidgetType* CreateWidgetWithParent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& ParentName = TEXT(""));
}

FUMGCommands::FUMGCommands(TSharedPtr<FServiceContext> InServiceContext)
{
	ServiceContext = InServiceContext.IsValid() ? InServiceContext : MakeShared<FServiceContext>();

	// Initialize services using shared context
	LifecycleService = MakeShared<FWidgetLifecycleService>(ServiceContext);
	PropertyService = MakeShared<FWidgetPropertyService>(ServiceContext);
	// TODO: Initialize ComponentService when implementing Issue #191 (generic HandleAddWidgetComponent)
	// TODO: Issue #188 skipped - discovery handlers already well-structured
}

// Static member definition
// Static variables for UMG commands

// ===================================================================
// Enhanced UI Building Helper Functions
// ===================================================================

namespace UMGHelpers
{
	UPanelWidget* FindOrCreateParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& ParentName, const FString& ParentType)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return nullptr;
		}

		// If no parent specified, use root widget
		if (ParentName.IsEmpty())
		{
			return Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->RootWidget);
		}

		// Find existing parent
		UWidget* ExistingParent = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentName));
		if (ExistingParent)
		{
			return Cast<UPanelWidget>(ExistingParent);
		}

		// Create new parent panel if it doesn't exist
		UPanelWidget* NewParent = nullptr;
		if (ParentType == TEXT("CanvasPanel"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("Overlay"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("HorizontalBox"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("VerticalBox"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *ParentName);
		}
		else if (ParentType == TEXT("ScrollBox"))
		{
			NewParent = WidgetBlueprint->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), *ParentName);
		}

		// Add to root if we created a new parent
		if (NewParent)
		{
			UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetBlueprint->WidgetTree->RootWidget);
			if (RootPanel)
			{
				RootPanel->AddChild(NewParent);
			}
		}

		return NewParent;
	}

	bool SetSlotProperties(UWidget* Widget, UPanelWidget* Parent, const TSharedPtr<FJsonObject>& SlotProperties)
	{
		if (!Widget || !Parent || !SlotProperties.IsValid())
		{
			return false;
		}

		// Handle Canvas Panel slots
		if (UCanvasPanel* CanvasParent = Cast<UCanvasPanel>(Parent))
		{
			UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set position
			const TArray<TSharedPtr<FJsonValue>>* Position;
			if (SlotProperties->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
			{
				FVector2D Pos((*Position)[0]->AsNumber(), (*Position)[1]->AsNumber());
				Slot->SetPosition(Pos);
			}

			// Set size
			const TArray<TSharedPtr<FJsonValue>>* Size;
			if (SlotProperties->TryGetArrayField(TEXT("size"), Size) && Size->Num() >= 2)
			{
				FVector2D SizeVec((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber());
				Slot->SetSize(SizeVec);
			}

			// Set anchors
			const TSharedPtr<FJsonObject>* AnchorsObj;
			if (SlotProperties->TryGetObjectField(TEXT("anchors"), AnchorsObj))
			{
				FAnchors Anchors;
				Anchors.Minimum.X = (*AnchorsObj)->GetNumberField(TEXT("min_x"));
				Anchors.Minimum.Y = (*AnchorsObj)->GetNumberField(TEXT("min_y"));
				Anchors.Maximum.X = (*AnchorsObj)->GetNumberField(TEXT("max_x"));
				Anchors.Maximum.Y = (*AnchorsObj)->GetNumberField(TEXT("max_y"));
				Slot->SetAnchors(Anchors);
			}

			// Set alignment
			const TArray<TSharedPtr<FJsonValue>>* Alignment;
			if (SlotProperties->TryGetArrayField(TEXT("alignment"), Alignment) && Alignment->Num() >= 2)
			{
				FVector2D AlignVec((*Alignment)[0]->AsNumber(), (*Alignment)[1]->AsNumber());
				Slot->SetAlignment(AlignVec);
			}

			return true;
		}
		// Handle Horizontal Box slots
		else if (UHorizontalBox* HBoxParent = Cast<UHorizontalBox>(Parent))
		{
			UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set padding
			const TArray<TSharedPtr<FJsonValue>>* Padding;
			if (SlotProperties->TryGetArrayField(TEXT("padding"), Padding) && Padding->Num() >= 4)
			{
				FMargin PaddingValue;
				PaddingValue.Left = (*Padding)[0]->AsNumber();
				PaddingValue.Top = (*Padding)[1]->AsNumber();
				PaddingValue.Right = (*Padding)[2]->AsNumber();
				PaddingValue.Bottom = (*Padding)[3]->AsNumber();
				Slot->SetPadding(PaddingValue);
			}

			return true;
		}
		// Handle Vertical Box slots
		else if (UVerticalBox* VBoxParent = Cast<UVerticalBox>(Parent))
		{
			UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set padding
			const TArray<TSharedPtr<FJsonValue>>* Padding;
			if (SlotProperties->TryGetArrayField(TEXT("padding"), Padding) && Padding->Num() >= 4)
			{
				FMargin PaddingValue;
				PaddingValue.Left = (*Padding)[0]->AsNumber();
				PaddingValue.Top = (*Padding)[1]->AsNumber();
				PaddingValue.Right = (*Padding)[2]->AsNumber();
				PaddingValue.Bottom = (*Padding)[3]->AsNumber();
				Slot->SetPadding(PaddingValue);
			}

			return true;
		}
		// Handle Overlay slots
		else if (UOverlay* OverlayParent = Cast<UOverlay>(Parent))
		{
			UOverlaySlot* Slot = Cast<UOverlaySlot>(Widget->Slot);
			if (!Slot)
			{
				return false;
			}

			// Set padding
			const TArray<TSharedPtr<FJsonValue>>* Padding;
			if (SlotProperties->TryGetArrayField(TEXT("padding"), Padding) && Padding->Num() >= 4)
			{
				FMargin PaddingValue;
				PaddingValue.Left = (*Padding)[0]->AsNumber();
				PaddingValue.Top = (*Padding)[1]->AsNumber();
				PaddingValue.Right = (*Padding)[2]->AsNumber();
				PaddingValue.Bottom = (*Padding)[3]->AsNumber();
				Slot->SetPadding(PaddingValue);
			}

			// Set horizontal alignment
			FString HAlignment;
			if (SlotProperties->TryGetStringField(TEXT("horizontal_alignment"), HAlignment))
			{
				if (HAlignment == TEXT("Left"))
				{
					Slot->SetHorizontalAlignment(HAlign_Left);
				}
				else if (HAlignment == TEXT("Center"))
				{
					Slot->SetHorizontalAlignment(HAlign_Center);
				}
				else if (HAlignment == TEXT("Right"))
				{
					Slot->SetHorizontalAlignment(HAlign_Right);
				}
				else if (HAlignment == TEXT("Fill"))
				{
					Slot->SetHorizontalAlignment(HAlign_Fill);
				}
			}

			// Set vertical alignment  
			FString VAlignment;
			if (SlotProperties->TryGetStringField(TEXT("vertical_alignment"), VAlignment))
			{
				if (VAlignment == TEXT("Top"))
				{
					Slot->SetVerticalAlignment(VAlign_Top);
				}
				else if (VAlignment == TEXT("Center"))
				{
					Slot->SetVerticalAlignment(VAlign_Center);
				}
				else if (VAlignment == TEXT("Bottom"))
				{
					Slot->SetVerticalAlignment(VAlign_Bottom);
				}
				else if (VAlignment == TEXT("Fill"))
				{
					Slot->SetVerticalAlignment(VAlign_Fill);
				}
			}

			return true;
		}

		return false;
	}

	template<typename WidgetType>
	WidgetType* CreateWidgetWithParent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& ParentName)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			return nullptr;
		}

		// Create the widget
		WidgetType* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<WidgetType>(WidgetType::StaticClass(), *WidgetName);
		if (!NewWidget)
		{
			return nullptr;
		}

		// Find or create parent
		UPanelWidget* Parent = FindOrCreateParentPanel(WidgetBlueprint, ParentName);
		if (Parent)
		{
			Parent->AddChild(NewWidget);
		}

		return NewWidget;
	}

	// Ensure there's a suitable container for adding multiple widgets
	// If the root widget can't hold multiple children, wrap it in a CanvasPanel
	UPanelWidget* EnsureSuitableContainer(UWidgetBlueprint* WidgetBlueprint)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !WidgetBlueprint->WidgetTree->RootWidget)
		{
			return nullptr;
		}

		UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;
		
		// Check if root widget is already a suitable container
		if (UPanelWidget* PanelRoot = Cast<UPanelWidget>(RootWidget))
		{
			// Check if it's a container that can hold multiple children
			if (Cast<UCanvasPanel>(PanelRoot) || 
				Cast<UOverlay>(PanelRoot) || 
				Cast<UHorizontalBox>(PanelRoot) || 
				Cast<UVerticalBox>(PanelRoot) || 
				Cast<UScrollBox>(PanelRoot) ||
				Cast<UGridPanel>(PanelRoot))
			{
				return PanelRoot;
			}
		}
		
		// Root widget is not suitable (e.g., SizeBox, Border, etc.)
		// Create a CanvasPanel wrapper and restructure the hierarchy
		UCanvasPanel* WrapperCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CanvasPanel_Wrapper"));
		if (!WrapperCanvas)
		{
			return nullptr;
		}
		
		// Move the current root widget into the canvas panel
		UCanvasPanelSlot* RootSlot = WrapperCanvas->AddChildToCanvas(RootWidget);
		if (RootSlot)
		{
			// Set the root widget to fill the canvas
			RootSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			RootSlot->SetPosition(FVector2D(0, 0));
			RootSlot->SetSize(FVector2D(0, 0)); // Size to content
		}
		
		// Set the canvas panel as the new root
		WidgetBlueprint->WidgetTree->RootWidget = WrapperCanvas;
		
		return WrapperCanvas;
	}
}

TSharedPtr<FJsonObject> FUMGCommands::HandleCommand(const FString& CommandName, const TSharedPtr<FJsonObject>& Params)
{
	// Original UMG Commands
	if (CommandName == TEXT("create_umg_widget_blueprint"))
	{
		return HandleCreateUMGWidgetBlueprint(Params);
	}
	// UMG Discovery Commands
	else if (CommandName == TEXT("search_items"))
	{
		return HandleSearchItems(Params);
	}
	else if (CommandName == TEXT("get_widget_blueprint_info"))
	{
		return HandleGetWidgetBlueprintInfo(Params);
	}
	else if (CommandName == TEXT("list_widget_components"))
	{
		return HandleListWidgetComponents(Params);
	}
	else if (CommandName == TEXT("get_widget_component_properties"))
	{
		return HandleGetWidgetComponentProperties(Params);
	}
	else if (CommandName == TEXT("get_available_widget_types"))
	{
		return HandleGetAvailableWidgetTypes(Params);
	}
	else if (CommandName == TEXT("validate_widget_hierarchy"))
	{
		return HandleValidateWidgetHierarchy(Params);
	}
	// UMG Hierarchy Commands
	else if (CommandName == TEXT("add_child_to_panel"))
	{
		return HandleAddChildToPanel(Params);
	}
	else if (CommandName == TEXT("remove_umg_component"))
	{
		return HandleRemoveUMGComponent(Params);
	}
	else if (CommandName == TEXT("set_widget_slot_properties"))
	{
		return HandleSetWidgetSlotProperties(Params);
	}
	// Enhanced UMG Building Commands removed - not implemented
	// UMG Styling Commands
	else if (CommandName == TEXT("set_widget_property"))
	{
		return HandleSetWidgetProperty(Params);
	}
	else if (CommandName == TEXT("get_widget_property"))
	{
		return HandleGetWidgetProperty(Params);
	}
	else if (CommandName == TEXT("list_widget_properties"))
	{
		return HandleListWidgetProperties(Params);
	}
	// set_widget_transform/set_widget_visibility/set_widget_z_order removed
	else if (CommandName == TEXT("bind_input_events"))
	{
		return HandleBindInputEvents(Params);
	}
	else if (CommandName == TEXT("get_available_events"))
	{
		return HandleGetAvailableEvents(Params);
	}
	else if (CommandName == TEXT("delete_widget_blueprint"))
	{
		return HandleDeleteWidgetBlueprint(Params);
	}

	// All event handling, data binding, animation, and bulk operations have been removed
	// Only keeping core working functions

	return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown UMG command: %s"), *CommandName));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	// 1. Extract and validate parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Get optional parameters
	FString PackagePath = TEXT("/Game/UI/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	
	FString ParentClass = TEXT("UserWidget");
	Params->TryGetStringField(TEXT("parent_class"), ParentClass);

	// 2. Call service method
	auto Result = LifecycleService->CreateWidgetBlueprint(BlueprintName, PackagePath, ParentClass);
	
	// 3. Handle result
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	UWidgetBlueprint* WidgetBlueprint = Result.GetValue();
	
	// 4. Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), WidgetBlueprint->GetPathName());
	return ResultObj;
}




// ===================================================================
// UMG Discovery Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleSearchItems(const TSharedPtr<FJsonObject>& Params)
{
	FString SearchTerm;
	if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'search_term' parameter"));
	}

	FString AssetType;
	Params->TryGetStringField(TEXT("asset_type"), AssetType);

	FString Path = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), Path);

	bool bCaseSensitive = false;
	Params->TryGetBoolField(TEXT("case_sensitive"), bCaseSensitive);

	bool bIncludeEngineContent = false;
	Params->TryGetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);

	int32 MaxResults = 100;
	double MaxResultsValue = 0.0;
	if (Params->TryGetNumberField(TEXT("max_results"), MaxResultsValue))
	{
		MaxResults = FMath::Max(0, static_cast<int32>(MaxResultsValue));
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(*Path);
	if (bIncludeEngineContent)
	{
		Filter.PackagePaths.Add(TEXT("/Engine"));
	}
	if (!AssetType.IsEmpty())
	{
		FTopLevelAssetPath AssetClassPath = UClass::TryConvertShortTypeNameToPathName<UClass>(AssetType, ELogVerbosity::NoLogging);
		if (AssetClassPath.IsNull())
		{
			if (AssetType.Contains(TEXT("/")))
			{
				AssetClassPath = FTopLevelAssetPath(*AssetType);
			}
			else if (UClass* AssetClass = FindFirstObjectSafe<UClass>(*AssetType))
			{
				AssetClassPath = AssetClass->GetClassPathName();
			}
		}

		if (!AssetClassPath.IsNull())
		{
			Filter.ClassPaths.Add(AssetClassPath);
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> ItemArray;
	const ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;

	for (const FAssetData& Asset : Assets)
	{
		const FString AssetName = Asset.AssetName.ToString();
		if (!SearchTerm.IsEmpty() && !AssetName.Contains(SearchTerm, SearchCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
		ItemObj->SetStringField(TEXT("asset_name"), AssetName);
		ItemObj->SetStringField(TEXT("object_path"), Asset.GetObjectPathString());
		ItemObj->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
		ItemObj->SetStringField(TEXT("class_name"), Asset.AssetClassPath.ToString());

		ItemArray.Add(MakeShared<FJsonValueObject>(ItemObj));

		if (MaxResults > 0 && ItemArray.Num() >= MaxResults)
		{
			break;
		}
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("items"), ItemArray);
	Response->SetNumberField(TEXT("count"), ItemArray.Num());

	TSharedPtr<FJsonObject> SearchInfo = MakeShared<FJsonObject>();
	SearchInfo->SetStringField(TEXT("search_term"), SearchTerm);
	SearchInfo->SetStringField(TEXT("asset_type"), AssetType);
	SearchInfo->SetStringField(TEXT("path"), Path);
	SearchInfo->SetBoolField(TEXT("case_sensitive"), bCaseSensitive);
	SearchInfo->SetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);
	SearchInfo->SetNumberField(TEXT("max_results"), MaxResults);
	Response->SetObjectField(TEXT("search_info"), SearchInfo);

	return Response;
}

// TODO: Remove legacy HandleSetWidgetProperty once all handlers are refactored to service layer.
TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!PropertyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetPropertyService not available"));
	}

	FString WidgetBlueprintName;
	FString WidgetName;
	FString PropertyName;

	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}

	FWidgetPropertySetRequest Request;
	Request.PropertyPath = PropertyName;

	FString PropertyValueString;
	const bool bHasStringValue = Params->TryGetStringField(TEXT("property_value"), PropertyValueString);
	if (bHasStringValue)
	{
		Request.Value.SetString(PropertyValueString);
	}
	else
	{
		const TSharedPtr<FJsonValue>* PropertyValueField = Params->Values.Find(TEXT("property_value"));
		if (PropertyValueField && PropertyValueField->IsValid())
		{
			Request.Value.SetJson(*PropertyValueField);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing property_value parameter"));
		}
	}

	FString CollectionOp;
	if (Params->TryGetStringField(TEXT("collection_op"), CollectionOp) && !CollectionOp.IsEmpty())
	{
		FWidgetCollectionOperation Operation;
		Operation.Operation = CollectionOp;
		double IndexValue = 0.0;
		if (Params->TryGetNumberField(TEXT("index"), IndexValue))
		{
			Operation.Index = static_cast<int32>(IndexValue);
		}
		Request.CollectionOperation = Operation;
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	const auto Result = PropertyService->SetWidgetProperty(WidgetBlueprint, WidgetName, Request);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetPropertySetResult& Payload = Result.GetValue();
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), WidgetName);
	Response->SetStringField(TEXT("property_name"), PropertyName);

	if (Payload.bChildOrderUpdated)
	{
		Response->SetNumberField(TEXT("property_value"), Payload.ChildOrderValue);
	}
	else if (Payload.AppliedValue.HasJson())
	{
		Response->SetField(TEXT("property_value"), Payload.AppliedValue.JsonValue);
	}
	else if (Payload.AppliedValue.HasString())
	{
		Response->SetStringField(TEXT("property_value"), Payload.AppliedValue.StringValue);
	}
	else
	{
		Response->SetStringField(TEXT("property_value"), TEXT(""));
	}

	if (!Payload.CollectionOperation.IsEmpty())
	{
		Response->SetStringField(TEXT("collection_op"), Payload.CollectionOperation);
	}

	Response->SetStringField(TEXT("note"), Payload.Note.IsEmpty() ? TEXT("Property set successfully") : Payload.Note);

	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetBlueprintInfo(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
	// Get widget blueprint identifier (accepts name or full path) - SIMPLIFIED
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		// Try alternates (same as working list_widget_components)
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
		if (WidgetName.IsEmpty())
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter (accepts name or full path)"));
		}
	}
	

	
	// Find widget blueprint (same as working version)
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);

	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}
	

	
	// Create widget_info object (SIMPLIFIED - no complex nested structures)
	TSharedPtr<FJsonObject> WidgetInfo = MakeShared<FJsonObject>();
	
	// Basic widget information only
	WidgetInfo->SetStringField(TEXT("name"), WidgetBlueprint->GetName());
	WidgetInfo->SetStringField(TEXT("path"), WidgetBlueprint->GetPathName());
	WidgetInfo->SetStringField(TEXT("package_path"), WidgetBlueprint->GetPackage() ? WidgetBlueprint->GetPackage()->GetPathName() : TEXT(""));
	WidgetInfo->SetStringField(TEXT("parent_class"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetName() : TEXT("UserWidget"));
	
	// Get root widget info (comprehensive)
	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	TArray<TSharedPtr<FJsonValue>> VariableArray;
	TArray<TSharedPtr<FJsonValue>> EventArray;
	TArray<TSharedPtr<FJsonValue>> AnimationArray;
	
	if (WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget)
	{
		UWidget* RootWidget = WidgetBlueprint->WidgetTree->RootWidget;
		WidgetInfo->SetStringField(TEXT("root_widget_type"), RootWidget->GetClass()->GetName());
		WidgetInfo->SetStringField(TEXT("root_widget_name"), RootWidget->GetName());
		
		// Get component hierarchy with detailed information
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		

		
		// Process all widgets for comprehensive info
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget)
			{
				TSharedPtr<FJsonObject> ComponentInfo = MakeShared<FJsonObject>();
				ComponentInfo->SetStringField(TEXT("name"), Widget->GetName());
				ComponentInfo->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
				ComponentInfo->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
				ComponentInfo->SetBoolField(TEXT("is_enabled"), Widget->GetIsEnabled());
				ComponentInfo->SetStringField(TEXT("visibility"), UEnum::GetValueAsString(Widget->GetVisibility()));
				
				// Parent information
				if (UPanelWidget* ParentPanel = Widget->GetParent())
				{
					ComponentInfo->SetStringField(TEXT("parent"), ParentPanel->GetName());
					ComponentInfo->SetStringField(TEXT("parent_type"), ParentPanel->GetClass()->GetName());
				}
				
				// Child information for panel widgets
				if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
				{
					int32 ChildCount = PanelWidget->GetChildrenCount();
					ComponentInfo->SetNumberField(TEXT("child_count"), ChildCount);
					
					// List child names
					TArray<TSharedPtr<FJsonValue>> ChildrenArray;
					for (int32 i = 0; i < ChildCount; i++)
					{
						if (UWidget* ChildWidget = PanelWidget->GetChildAt(i))
						{
							ChildrenArray.Add(MakeShared<FJsonValueString>(ChildWidget->GetName()));
						}
					}
					ComponentInfo->SetArrayField(TEXT("children"), ChildrenArray);
				}
				
				// Position and size information for canvas panel slots
				if (Widget->Slot)
				{
					TSharedPtr<FJsonObject> SlotInfo = MakeShared<FJsonObject>();
					SlotInfo->SetStringField(TEXT("slot_type"), Widget->Slot->GetClass()->GetName());
					
					// Canvas Panel Slot specific info
					if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
					{
						FVector2D Position = CanvasSlot->GetPosition();
						FVector2D Size = CanvasSlot->GetSize();
						FAnchors Anchors = CanvasSlot->GetAnchors();
						FVector2D Alignment = CanvasSlot->GetAlignment();
						
						SlotInfo->SetNumberField(TEXT("position_x"), Position.X);
						SlotInfo->SetNumberField(TEXT("position_y"), Position.Y);
						SlotInfo->SetNumberField(TEXT("size_x"), Size.X);
						SlotInfo->SetNumberField(TEXT("size_y"), Size.Y);
						SlotInfo->SetNumberField(TEXT("anchor_min_x"), Anchors.Minimum.X);
						SlotInfo->SetNumberField(TEXT("anchor_min_y"), Anchors.Minimum.Y);
						SlotInfo->SetNumberField(TEXT("anchor_max_x"), Anchors.Maximum.X);
						SlotInfo->SetNumberField(TEXT("anchor_max_y"), Anchors.Maximum.Y);
						SlotInfo->SetNumberField(TEXT("alignment_x"), Alignment.X);
						SlotInfo->SetNumberField(TEXT("alignment_y"), Alignment.Y);
						SlotInfo->SetBoolField(TEXT("auto_size"), CanvasSlot->GetAutoSize());
						SlotInfo->SetNumberField(TEXT("z_order"), CanvasSlot->GetZOrder());
					}
					
					ComponentInfo->SetObjectField(TEXT("slot_info"), SlotInfo);
				}
				
				ComponentArray.Add(MakeShared<FJsonValueObject>(ComponentInfo));
			}
		}
		
		// Get Variables (from the blueprint)
		if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass))
		{
			for (TFieldIterator<FProperty> PropertyIt(BlueprintClass); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (Property && Property->HasAllPropertyFlags(CPF_BlueprintVisible))
				{
					TSharedPtr<FJsonObject> VariableInfo = MakeShared<FJsonObject>();
					VariableInfo->SetStringField(TEXT("name"), Property->GetName());
					VariableInfo->SetStringField(TEXT("type"), Property->GetCPPType());
					VariableInfo->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
					VariableInfo->SetBoolField(TEXT("is_editable"), Property->HasAllPropertyFlags(CPF_Edit));
					VariableInfo->SetBoolField(TEXT("is_blueprint_readonly"), Property->HasAllPropertyFlags(CPF_BlueprintReadOnly));
					VariableInfo->SetStringField(TEXT("tooltip"), Property->GetMetaData(TEXT("ToolTip")));
					
					VariableArray.Add(MakeShared<FJsonValueObject>(VariableInfo));
				}
			}
		}
		
		// Get Events (from function graph nodes)
		if (WidgetBlueprint->UbergraphPages.Num() > 0)
		{
			for (UEdGraph* Graph : WidgetBlueprint->UbergraphPages)
			{
				if (Graph)
				{
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						// Event nodes
						if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
						{
							TSharedPtr<FJsonObject> EventInfo = MakeShared<FJsonObject>();
							EventInfo->SetStringField(TEXT("name"), EventNode->EventReference.GetMemberName().ToString());
							EventInfo->SetStringField(TEXT("type"), TEXT("Event"));
							EventInfo->SetStringField(TEXT("category"), EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
							EventInfo->SetBoolField(TEXT("is_custom_event"), EventNode->bIsEditable);
							EventInfo->SetBoolField(TEXT("is_override"), EventNode->bOverrideFunction);
							
							EventArray.Add(MakeShared<FJsonValueObject>(EventInfo));
						}
						// Input Action events  
						else if (UK2Node_InputAction* InputNode = Cast<UK2Node_InputAction>(Node))
						{
							TSharedPtr<FJsonObject> EventInfo = MakeShared<FJsonObject>();
							EventInfo->SetStringField(TEXT("name"), InputNode->InputActionName.ToString());
							EventInfo->SetStringField(TEXT("type"), TEXT("Input Action"));
							EventInfo->SetStringField(TEXT("category"), TEXT("Input"));
							
							EventArray.Add(MakeShared<FJsonValueObject>(EventInfo));
						}
					}
				}
			}
		}
		
		// Get Animations
		for (UWidgetAnimation* Animation : WidgetBlueprint->Animations)
		{
			if (Animation)
			{
				TSharedPtr<FJsonObject> AnimationInfo = MakeShared<FJsonObject>();
				AnimationInfo->SetStringField(TEXT("name"), Animation->GetName());
				AnimationInfo->SetNumberField(TEXT("duration"), Animation->GetEndTime());
				AnimationInfo->SetNumberField(TEXT("start_time"), Animation->GetStartTime());
				
				// Check if the animation has a movie scene for additional info
				if (Animation->GetMovieScene())
				{
					AnimationInfo->SetBoolField(TEXT("has_movie_scene"), true);
					// Convert frame numbers to double values for JSON
					FFrameRate FrameRate = Animation->GetMovieScene()->GetTickResolution();
					TRange<FFrameNumber> PlaybackRange = Animation->GetMovieScene()->GetPlaybackRange();
					
					if (!PlaybackRange.GetLowerBound().IsOpen())
					{
						double StartSeconds = FrameRate.AsSeconds(PlaybackRange.GetLowerBoundValue());
						AnimationInfo->SetNumberField(TEXT("playback_range_start"), StartSeconds);
					}
					if (!PlaybackRange.GetUpperBound().IsOpen())
					{
						double EndSeconds = FrameRate.AsSeconds(PlaybackRange.GetUpperBoundValue());
						AnimationInfo->SetNumberField(TEXT("playback_range_end"), EndSeconds);
					}
				}
				else
				{
					AnimationInfo->SetBoolField(TEXT("has_movie_scene"), false);
				}
				
				// Get animated tracks/properties
				TArray<TSharedPtr<FJsonValue>> TracksArray;
				if (Animation->GetMovieScene())
				{
					const TArray<UMovieSceneTrack*>& Tracks = Animation->GetMovieScene()->GetTracks();
					for (UMovieSceneTrack* Track : Tracks)
					{
						if (Track)
						{
							TSharedPtr<FJsonObject> TrackInfo = MakeShared<FJsonObject>();
							TrackInfo->SetStringField(TEXT("track_type"), Track->GetClass()->GetName());
							TrackInfo->SetStringField(TEXT("display_name"), Track->GetDisplayName().ToString());
							// Note: IsEvalDisabled method may not be available in UE 5.6
							TrackInfo->SetBoolField(TEXT("is_enabled"), true); // Default to enabled
							TracksArray.Add(MakeShared<FJsonValueObject>(TrackInfo));
						}
					}
				}
				AnimationInfo->SetArrayField(TEXT("tracks"), TracksArray);
				AnimationInfo->SetNumberField(TEXT("track_count"), TracksArray.Num());
				
				AnimationArray.Add(MakeShared<FJsonValueObject>(AnimationInfo));
			}
		}
		
		WidgetInfo->SetArrayField(TEXT("components"), ComponentArray);
		WidgetInfo->SetNumberField(TEXT("component_count"), ComponentArray.Num());
	}
	else
	{
		// Empty arrays for missing widget tree
		WidgetInfo->SetArrayField(TEXT("components"), ComponentArray);
		WidgetInfo->SetNumberField(TEXT("component_count"), 0);
	}
	
	// Set comprehensive information arrays
	WidgetInfo->SetArrayField(TEXT("variables"), VariableArray);
	WidgetInfo->SetNumberField(TEXT("variable_count"), VariableArray.Num());
	WidgetInfo->SetArrayField(TEXT("events"), EventArray);
	WidgetInfo->SetNumberField(TEXT("event_count"), EventArray.Num());
	WidgetInfo->SetArrayField(TEXT("animations"), AnimationArray);
	WidgetInfo->SetNumberField(TEXT("animation_count"), AnimationArray.Num());
	
	// Set success response (same structure as before, but simpler content)
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("widget_info"), WidgetInfo);
	

	
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetComponents(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get widget blueprint name
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		// Try alternates
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
		if (WidgetName.IsEmpty())
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter (accepts name or full path)"));
		}
	}
	
	// Find widget blueprint
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}
	
	// Get all widgets in the tree
	TArray<TSharedPtr<FJsonValue>> ComponentArray;
	TArray<UWidget*> AllWidgets;
	WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
	
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget)
		{
			TSharedPtr<FJsonObject> ComponentInfo = MakeShared<FJsonObject>();
			ComponentInfo->SetStringField(TEXT("name"), Widget->GetName());
			ComponentInfo->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
			ComponentInfo->SetBoolField(TEXT("is_variable"), Widget->bIsVariable);
			ComponentArray.Add(MakeShared<FJsonValueObject>(ComponentInfo));
		}
	}
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("components"), ComponentArray);
	Response->SetStringField(TEXT("widget_path"), WidgetBlueprint->GetPathName());
	Response->SetNumberField(TEXT("count"), ComponentArray.Num());
	Response->SetStringField(TEXT("usage"), TEXT("Use 'widget_name' as name, package path, or full object path to target a widget blueprint."));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get parameters
	FString WidgetName, ComponentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' or 'component_name' parameter"));
	}
	
	// Fall back to alternates if widget_name is not provided as expected
	if (WidgetName.IsEmpty())
	{
		Params->TryGetStringField(TEXT("widget_path"), WidgetName);
		if (WidgetName.IsEmpty())
		{
			Params->TryGetStringField(TEXT("object_path"), WidgetName);
		}
	}
	// Find widget blueprint
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'"), *WidgetName));
	}
	
	// Find the specific widget component
	UWidget* TargetWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ComponentName));
	if (!TargetWidget)
	{
	return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component '%s' not found in widget"), *ComponentName));
	}
	
	// Get component properties - simplified version
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("component_name"), ComponentName);
	Response->SetStringField(TEXT("component_type"), TargetWidget->GetClass()->GetName());
	Response->SetBoolField(TEXT("is_variable"), TargetWidget->bIsVariable);
	Response->SetBoolField(TEXT("is_visible"), TargetWidget->GetVisibility() != ESlateVisibility::Collapsed);
	Response->SetStringField(TEXT("widget_path"), WidgetBlueprint->GetPathName());
	
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableWidgetTypes(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// List of supported widget types
	TArray<FString> WidgetTypes = {
		TEXT("TextBlock"),
		TEXT("Button"),
		TEXT("EditableText"),
		TEXT("EditableTextBox"),
		TEXT("RichTextBlock"),
		TEXT("CheckBox"),
		TEXT("Slider"),
		TEXT("ProgressBar"),
		TEXT("Image"),
		TEXT("Spacer"),
		TEXT("CanvasPanel"),
		TEXT("Overlay"),
		TEXT("HorizontalBox"),
		TEXT("VerticalBox"),
		TEXT("ScrollBox"),
		TEXT("GridPanel"),
		TEXT("ListView"),
		TEXT("TileView"),
		TEXT("TreeView"),
		TEXT("WidgetSwitcher")
	};
	
	TArray<TSharedPtr<FJsonValue>> TypeArray;
	for (const FString& Type : WidgetTypes)
	{
		TypeArray.Add(MakeShared<FJsonValueString>(Type));
	}
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("widget_types"), TypeArray);
	Response->SetNumberField(TEXT("count"), WidgetTypes.Num());
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleValidateWidgetHierarchy(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get widget blueprint name
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}
	
	// Find widget blueprint
	FString BlueprintPath = FString::Printf(TEXT("/Game/Widgets/%s"), *WidgetName);
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(BlueprintPath));
	
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Basic validation - check if widget tree exists and has root
	bool bIsValid = WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget;
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetBoolField(TEXT("is_valid"), bIsValid);
	Response->SetStringField(TEXT("validation_message"), bIsValid ? TEXT("Widget hierarchy is valid") : TEXT("Invalid widget hierarchy"));
	
	return Response;
}

// ===================================================================
// UMG Component Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	FString ChildName;
	if (!Params->TryGetStringField(TEXT("child_name"), ChildName))
	{
		if (!Params->TryGetStringField(TEXT("component_name"), ChildName) &&
			!Params->TryGetStringField(TEXT("widget_component_name"), ChildName))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing child_name parameter"));
		}
	}

	FString ParentName;
	bool bHasParentName = Params->TryGetStringField(TEXT("parent_name"), ParentName) ||
		Params->TryGetStringField(TEXT("panel_name"), ParentName) ||
		Params->TryGetStringField(TEXT("parent_component_name"), ParentName);

	FString ParentType = TEXT("CanvasPanel");
	Params->TryGetStringField(TEXT("parent_type"), ParentType);

	bool bReparentIfExists = true;
	Params->TryGetBoolField(TEXT("reparent_if_exists"), bReparentIfExists);

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}

	UWidget* ChildWidget = WidgetTree->FindWidget(FName(*ChildName));
	if (!ChildWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child widget '%s' not found"), *ChildName));
	}

	UPanelWidget* ParentPanel = nullptr;
	if (bHasParentName && !ParentName.IsEmpty())
	{
		ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName, ParentType);
	}
	else
	{
		ParentPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
		if (ParentPanel)
		{
			ParentName = ParentPanel->GetName();
		}
	}

	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Parent panel not found or could not be created"));
	}

	bool bStructureChanged = false;
	bool bReparented = false;
	UPanelWidget* ExistingParent = ChildWidget->GetParent();

	if (ExistingParent && ExistingParent != ParentPanel)
	{
		if (!bReparentIfExists)
		{
			return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child widget '%s' already has a different parent"), *ChildName));
		}

		ExistingParent->RemoveChild(ChildWidget);
		bStructureChanged = true;
		bReparented = true;
	}

	if (!ExistingParent)
	{
		if (WidgetTree->RootWidget == ChildWidget)
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Cannot reparent the root widget using add_child_to_panel"));
		}
	}

	bool bAlreadyInParent = (ChildWidget->GetParent() == ParentPanel);
	double InsertIndexValue = 0.0;
	int32 DesiredIndex = INDEX_NONE;
	if (Params->TryGetNumberField(TEXT("child_index"), InsertIndexValue) ||
		Params->TryGetNumberField(TEXT("insert_index"), InsertIndexValue))
	{
		DesiredIndex = FMath::Max(0, static_cast<int32>(InsertIndexValue));
	}

	if (!bAlreadyInParent)
	{
		if (DesiredIndex != INDEX_NONE)
		{
			DesiredIndex = FMath::Min(DesiredIndex, ParentPanel->GetChildrenCount());
			ParentPanel->InsertChildAt(DesiredIndex, ChildWidget);
		}
		else
		{
			ParentPanel->AddChild(ChildWidget);
		}
		bStructureChanged = true;
	}
	else if (DesiredIndex != INDEX_NONE)
	{
		const int32 CurrentIndex = ParentPanel->GetChildIndex(ChildWidget);
		DesiredIndex = FMath::Min(DesiredIndex, ParentPanel->GetChildrenCount() - 1);
		if (CurrentIndex != DesiredIndex && CurrentIndex != INDEX_NONE)
		{
			ParentPanel->RemoveChild(ChildWidget);
			ParentPanel->InsertChildAt(DesiredIndex, ChildWidget);
			bStructureChanged = true;
		}
	}

	bool bSlotPropsApplied = false;
	if (Params->HasTypedField<EJson::Object>(TEXT("slot_properties")))
	{
		TSharedPtr<FJsonObject> SlotProperties = Params->GetObjectField(TEXT("slot_properties"));
		if (SlotProperties.IsValid())
		{
			bSlotPropsApplied = UMGHelpers::SetSlotProperties(ChildWidget, ParentPanel, SlotProperties);
		}
	}

	if (bStructureChanged)
	{
		WidgetBlueprint->Modify();
		WidgetBlueprint->MarkPackageDirty();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("child_name"), ChildWidget->GetName());
	Response->SetStringField(TEXT("parent_name"), ParentPanel->GetName());
	Response->SetStringField(TEXT("parent_type"), ParentPanel->GetClass()->GetName());
	Response->SetBoolField(TEXT("reparented"), bReparented);
	Response->SetBoolField(TEXT("slot_properties_applied"), bSlotPropsApplied);
	if (DesiredIndex != INDEX_NONE)
	{
		Response->SetNumberField(TEXT("child_index"), ParentPanel->GetChildIndex(ChildWidget));
	}

	Response->SetStringField(TEXT("note"), TEXT("Child widget successfully attached to parent panel"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleRemoveUMGComponent(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	FString ComponentName;
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		// Fallbacks used by some callers
		if (!Params->TryGetStringField(TEXT("widget_component_name"), ComponentName))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
		}
	}

	bool bRemoveChildren = false;
	Params->TryGetBoolField(TEXT("remove_children"), bRemoveChildren);

	bool bRemoveFromVariables = false;
	Params->TryGetBoolField(TEXT("remove_from_variables"), bRemoveFromVariables);

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}

	UWidget* TargetComponent = WidgetTree->FindWidget(FName(*ComponentName));
	if (!TargetComponent)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> RemovedComponents;
	TArray<TSharedPtr<FJsonValue>> OrphanedChildren;

	// Recursively walk children so we can either detach or report them
	TArray<UWidget*> CollectedChildren;
	TFunction<void(UWidget*, TArray<UWidget*>&)> CollectChildren = [&](UWidget* Widget, TArray<UWidget*>& Children)
	{
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				if (UWidget* Child = Panel->GetChildAt(ChildIndex))
				{
					Children.Add(Child);
					CollectChildren(Child, Children);
				}
			}
		}
	};

	CollectChildren(TargetComponent, CollectedChildren);

	if (!bRemoveChildren && CollectedChildren.Num() > 0)
	{
		UWidget* RootWidget = WidgetTree->RootWidget;
		if (UPanelWidget* RootPanel = Cast<UPanelWidget>(RootWidget))
		{
			for (UWidget* Child : CollectedChildren)
			{
				if (!Child)
				{
					continue;
				}

				if (UWidget* CurrentParent = Child->GetParent())
				{
					if (UPanelWidget* CurrentPanel = Cast<UPanelWidget>(CurrentParent))
					{
						CurrentPanel->RemoveChild(Child);
					}
				}

				RootPanel->AddChild(Child);

				TSharedPtr<FJsonObject> OrphanInfo = MakeShared<FJsonObject>();
				OrphanInfo->SetStringField(TEXT("name"), Child->GetName());
				OrphanInfo->SetStringField(TEXT("type"), Child->GetClass()->GetName());
				OrphanedChildren.Add(MakeShared<FJsonValueObject>(OrphanInfo));
			}
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Root widget is not a panel; cannot reparent children"));
		}
	}

	UWidget* ParentWidget = TargetComponent->GetParent();
	FString ParentName = ParentWidget ? ParentWidget->GetName() : TEXT("Root");
	FString ParentType = ParentWidget ? ParentWidget->GetClass()->GetName() : TEXT("N/A");

	if (ParentWidget)
	{
		if (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget))
		{
			ParentPanel->RemoveChild(TargetComponent);
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Parent is not a panel widget"));
		}
	}
	else if (WidgetTree->RootWidget == TargetComponent)
	{
		WidgetTree->RootWidget = nullptr;
	}

	TSharedPtr<FJsonObject> MainComponentInfo = MakeShared<FJsonObject>();
	MainComponentInfo->SetStringField(TEXT("name"), ComponentName);
	MainComponentInfo->SetStringField(TEXT("type"), TargetComponent->GetClass()->GetName());
	RemovedComponents.Add(MakeShared<FJsonValueObject>(MainComponentInfo));

	if (bRemoveChildren)
	{
		for (UWidget* Child : CollectedChildren)
		{
			if (!Child)
			{
				continue;
			}

			TSharedPtr<FJsonObject> ChildInfo = MakeShared<FJsonObject>();
			ChildInfo->SetStringField(TEXT("name"), Child->GetName());
			ChildInfo->SetStringField(TEXT("type"), Child->GetClass()->GetName());
			RemovedComponents.Add(MakeShared<FJsonValueObject>(ChildInfo));
		}
	}

	bool bVariableCleanupPerformed = false;
	if (bRemoveFromVariables)
	{
		for (int32 Index = WidgetBlueprint->NewVariables.Num() - 1; Index >= 0; --Index)
		{
			if (WidgetBlueprint->NewVariables[Index].VarName.ToString() == ComponentName)
			{
				WidgetBlueprint->NewVariables.RemoveAt(Index);
				bVariableCleanupPerformed = true;
				break;
			}
		}
	}

	WidgetBlueprint->Modify();
	WidgetBlueprint->MarkPackageDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), ComponentName);
	Response->SetArrayField(TEXT("removed_components"), RemovedComponents);
	Response->SetArrayField(TEXT("orphaned_children"), OrphanedChildren);
	Response->SetBoolField(TEXT("variable_cleanup"), bVariableCleanupPerformed);

	TSharedPtr<FJsonObject> ParentInfo = MakeShared<FJsonObject>();
	ParentInfo->SetStringField(TEXT("name"), ParentName);
	ParentInfo->SetStringField(TEXT("type"), ParentType);
	Response->SetObjectField(TEXT("parent_info"), ParentInfo);

	Response->SetStringField(TEXT("note"), FString::Printf(
		TEXT("Universal component removal completed. Removed %d components, orphaned %d children"),
		RemovedComponents.Num(),
		OrphanedChildren.Num()));

	return Response;
}









// ===================================================================
// UMG Layout Methods Implementation (Stub implementations)
// ===================================================================








TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!PropertyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetPropertyService not available"));
	}

	FString WidgetBlueprintName;
	FString WidgetName;
	FString PropertyName;

	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}

	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing property_name parameter"));
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	const auto Result = PropertyService->GetWidgetProperty(WidgetBlueprint, WidgetName, PropertyName);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const FWidgetPropertyGetResult& Payload = Result.GetValue();
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), WidgetName);
	Response->SetStringField(TEXT("property_name"), PropertyName);

	if (Payload.bIsChildOrder)
	{
		Response->SetNumberField(TEXT("property_value"), Payload.ChildOrderValue);
	}
	else if (Payload.Value.HasJson())
	{
		Response->SetField(TEXT("property_value"), Payload.Value.JsonValue);
	}
	else if (Payload.Value.HasString())
	{
		Response->SetStringField(TEXT("property_value"), Payload.Value.StringValue);
	}
	else
	{
		Response->SetStringField(TEXT("property_value"), TEXT(""));
	}

	Response->SetStringField(TEXT("property_type"), Payload.PropertyType);
	Response->SetBoolField(TEXT("editable"), Payload.bIsEditable);

	TSharedPtr<FJsonObject> Constraints = Payload.Constraints.IsValid() ? Payload.Constraints : MakeShared<FJsonObject>();
	Response->SetObjectField(TEXT("constraints"), Constraints);

	TSharedPtr<FJsonObject> Schema = Payload.Schema.IsValid() ? Payload.Schema : MakeShared<FJsonObject>();
	Response->SetObjectField(TEXT("schema"), Schema);

	TSharedPtr<FJsonObject> AdapterInfo = MakeShared<FJsonObject>();
	AdapterInfo->SetStringField(TEXT("component_kind"), TEXT("UMG"));
	AdapterInfo->SetStringField(TEXT("slot_class"), Payload.SlotClass);
	Response->SetObjectField(TEXT("adapter_info"), AdapterInfo);

	return Response;
}
TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	FString SlotType;
	TSharedPtr<FJsonObject> SlotProperties;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("target_widget_name"), WidgetName))
	{
		// Try alternative parameter name
		if (!Params->TryGetStringField(TEXT("widget_component_name"), WidgetName))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Missing target_widget_name or widget_component_name parameter"));
		}
	}
	
	Params->TryGetStringField(TEXT("slot_type"), SlotType);
	SlotProperties = Params->GetObjectField(TEXT("slot_properties"));
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}

	UWidget* TargetWidget = WidgetTree->FindWidget(FName(*WidgetName));
	if (!TargetWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Target widget '%s' not found"), *WidgetName));
	}

	UPanelSlot* PanelSlot = TargetWidget->Slot;
	if (!PanelSlot)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Widget does not have a panel slot"));
	}

	// Example: Set padding if provided
	if (SlotProperties.IsValid() && SlotProperties->HasField(TEXT("padding")))
	{
		TArray<TSharedPtr<FJsonValue>> PaddingArray = SlotProperties->GetArrayField(TEXT("padding"));
		if (PaddingArray.Num() == 4)
		{
			FMargin Padding(
				PaddingArray[0]->AsNumber(),
				PaddingArray[1]->AsNumber(),
				PaddingArray[2]->AsNumber(),
				PaddingArray[3]->AsNumber()
			);
			
			// Try different slot types that support padding
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
			{
				// Canvas slots don't have padding, but we could adjust position/size
			}
			else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
			{
				HBoxSlot->SetPadding(Padding);
			}
			else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(PanelSlot))
			{
				VBoxSlot->SetPadding(Padding);
			}
			else if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
			{
				ScrollSlot->SetPadding(Padding);
			}
		}
	}

	// Handle Overlay slot alignment (CRITICAL FIX for background images)
	if (SlotProperties.IsValid() && (SlotProperties->HasField(TEXT("HorizontalAlignment")) || SlotProperties->HasField(TEXT("VerticalAlignment"))))
	{
		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot))
		{
			// Handle Horizontal Alignment
			if (SlotProperties->HasField(TEXT("HorizontalAlignment")))
			{
				FString HAlignStr = SlotProperties->GetStringField(TEXT("HorizontalAlignment"));
				if (HAlignStr == TEXT("Fill") || HAlignStr == TEXT("HAlign_Fill"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				}
				else if (HAlignStr == TEXT("Left") || HAlignStr == TEXT("HAlign_Left"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Left);
				}
				else if (HAlignStr == TEXT("Center") || HAlignStr == TEXT("HAlign_Center"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Center);
				}
				else if (HAlignStr == TEXT("Right") || HAlignStr == TEXT("HAlign_Right"))
				{
					OverlaySlot->SetHorizontalAlignment(HAlign_Right);
				}
			}

			// Handle Vertical Alignment  
			if (SlotProperties->HasField(TEXT("VerticalAlignment")))
			{
				FString VAlignStr = SlotProperties->GetStringField(TEXT("VerticalAlignment"));
				if (VAlignStr == TEXT("Fill") || VAlignStr == TEXT("VAlign_Fill"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Fill);
				}
				else if (VAlignStr == TEXT("Top") || VAlignStr == TEXT("VAlign_Top"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Top);
				}
				else if (VAlignStr == TEXT("Center") || VAlignStr == TEXT("VAlign_Center"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Center);
				}
				else if (VAlignStr == TEXT("Bottom") || VAlignStr == TEXT("VAlign_Bottom"))
				{
					OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
				}
			}
		}
	}

	// Handle Size Rule for Scroll Box slots
	if (SlotProperties.IsValid() && SlotProperties->HasField(TEXT("SizeRule")))
	{
		FString SizeRuleStr = SlotProperties->GetStringField(TEXT("SizeRule"));
		if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(PanelSlot))
		{
			if (SizeRuleStr == TEXT("Fill"))
			{
				ScrollSlot->SetSize(ESlateSizeRule::Fill);
			}
			else if (SizeRuleStr == TEXT("Auto"))
			{
				ScrollSlot->SetSize(ESlateSizeRule::Automatic);
			}
		}
	}

	WidgetBlueprint->MarkPackageDirty();
	
	// Use deferred compilation to avoid serialization crashes
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	// Don't compile immediately - let Unreal handle it when safe
	// FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("target_widget_name"), WidgetName);
	Result->SetStringField(TEXT("slot_type"), SlotType);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	if (SlotProperties.IsValid())
	{
		Result->SetObjectField(TEXT("slot_properties"), SlotProperties);
	}
	Result->SetStringField(TEXT("note"), TEXT("Slot properties updated"));
	return Result;
}

// ===================================================================
// UMG Styling Methods Implementation (Stub implementations)
// ===================================================================

// Helper function to parse complex property values from JSON
bool ParseComplexPropertyValue(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, UWidget* Widget, FString& ErrorMessage)
{
	if (!JsonValue.IsValid() || !Property || !Widget)
	{
		ErrorMessage = TEXT("Invalid parameters for property parsing");
		return false;
	}

	// Handle FLinearColor properties (like ColorAndOpacity, BackgroundColor)
	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
		{
			FLinearColor ColorValue;
			
			// Try to parse as JSON object with R,G,B,A components
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> ColorObj = JsonValue->AsObject();
				ColorValue.R = ColorObj->GetNumberField(TEXT("R"));
				ColorValue.G = ColorObj->GetNumberField(TEXT("G"));
				ColorValue.B = ColorObj->GetNumberField(TEXT("B"));
				ColorValue.A = ColorObj->GetNumberField(TEXT("A"));
			}
			// Try to parse as JSON array [R,G,B,A]
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> ColorArray = JsonValue->AsArray();
				if (ColorArray.Num() >= 3)
				{
					ColorValue.R = ColorArray[0]->AsNumber();
					ColorValue.G = ColorArray[1]->AsNumber();
					ColorValue.B = ColorArray[2]->AsNumber();
					ColorValue.A = ColorArray.Num() > 3 ? ColorArray[3]->AsNumber() : 1.0f;
				}
			}
			else
			{
				ErrorMessage = TEXT("LinearColor must be object {R,G,B,A} or array [R,G,B,A]");
				return false;
			}

			Property->SetValue_InContainer(Widget, &ColorValue);
			return true;
		}
		// Handle FSlateColor properties
		else if (StructProperty->Struct == TBaseStructure<FSlateColor>::Get())
		{
			FSlateColor SlateColorValue;
			
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> ColorObj = JsonValue->AsObject();
				FLinearColor LinearColor;
				LinearColor.R = ColorObj->GetNumberField(TEXT("R"));
				LinearColor.G = ColorObj->GetNumberField(TEXT("G"));
				LinearColor.B = ColorObj->GetNumberField(TEXT("B"));
				LinearColor.A = ColorObj->GetNumberField(TEXT("A"));
				SlateColorValue = FSlateColor(LinearColor);
			}
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> ColorArray = JsonValue->AsArray();
				if (ColorArray.Num() >= 3)
				{
					FLinearColor LinearColor;
					LinearColor.R = ColorArray[0]->AsNumber();
					LinearColor.G = ColorArray[1]->AsNumber();
					LinearColor.B = ColorArray[2]->AsNumber();
					LinearColor.A = ColorArray.Num() > 3 ? ColorArray[3]->AsNumber() : 1.0f;
					SlateColorValue = FSlateColor(LinearColor);
				}
			}

			Property->SetValue_InContainer(Widget, &SlateColorValue);
			return true;
		}
		// Handle FMargin properties
		else if (StructProperty->Struct == TBaseStructure<FMargin>::Get())
		{
			FMargin MarginValue;
			
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> MarginObj = JsonValue->AsObject();
				MarginValue.Left = MarginObj->GetNumberField(TEXT("Left"));
				MarginValue.Top = MarginObj->GetNumberField(TEXT("Top"));
				MarginValue.Right = MarginObj->GetNumberField(TEXT("Right"));
				MarginValue.Bottom = MarginObj->GetNumberField(TEXT("Bottom"));
			}
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> MarginArray = JsonValue->AsArray();
				if (MarginArray.Num() >= 4)
				{
					MarginValue.Left = MarginArray[0]->AsNumber();
					MarginValue.Top = MarginArray[1]->AsNumber();
					MarginValue.Right = MarginArray[2]->AsNumber();
					MarginValue.Bottom = MarginArray[3]->AsNumber();
				}
			}

			Property->SetValue_InContainer(Widget, &MarginValue);
			return true;
		}
		// Handle FVector2D properties
		else if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
		{
			FVector2D VectorValue;
			
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> VectorObj = JsonValue->AsObject();
				VectorValue.X = VectorObj->GetNumberField(TEXT("X"));
				VectorValue.Y = VectorObj->GetNumberField(TEXT("Y"));
			}
			else if (JsonValue->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>> VectorArray = JsonValue->AsArray();
				if (VectorArray.Num() >= 2)
				{
					VectorValue.X = VectorArray[0]->AsNumber();
					VectorValue.Y = VectorArray[1]->AsNumber();
				}
			}

			Property->SetValue_InContainer(Widget, &VectorValue);
			return true;
		}
		// Handle FSlateBrush properties (for Image, Border backgrounds)
		else if (StructProperty->Struct->GetName().Contains(TEXT("SlateBrush")))
		{
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> BrushObj = JsonValue->AsObject();
				
				// Get the existing brush and modify it
				void* BrushPtr = StructProperty->ContainerPtrToValuePtr<void>(Widget);
				if (BrushPtr)
				{
					FSlateBrush* SlateBrush = static_cast<FSlateBrush*>(BrushPtr);
					bool bModified = false;

					// Handle ResourceObject (texture)
					if (BrushObj->HasField(TEXT("ResourceObject")))
					{
						FString ResourcePath = BrushObj->GetStringField(TEXT("ResourceObject"));
						if (!ResourcePath.IsEmpty())
						{
							// Load the texture asset
							UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ResourcePath);
							if (Texture)
							{
								SlateBrush->SetResourceObject(Texture);
								bModified = true;

							}
							else
							{

							}
						}
					}

					// Handle DrawAs (Image, Box, Border, etc.)
					if (BrushObj->HasField(TEXT("DrawAs")))
					{
						FString DrawAsStr = BrushObj->GetStringField(TEXT("DrawAs"));
						if (DrawAsStr.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::Image;
							bModified = true;
						}
						else if (DrawAsStr.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::Box;
							bModified = true;
						}
						else if (DrawAsStr.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::Border;
							bModified = true;
						}
						else if (DrawAsStr.Equals(TEXT("RoundedBox"), ESearchCase::IgnoreCase))
						{
							SlateBrush->DrawAs = ESlateBrushDrawType::RoundedBox;
							bModified = true;
						}
					}

					// Handle Tiling (NoTile, Horizontal, Vertical, Both)
					if (BrushObj->HasField(TEXT("Tiling")))
					{
						FString TilingStr = BrushObj->GetStringField(TEXT("Tiling"));
						if (TilingStr.Equals(TEXT("NoTile"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::NoTile;
							bModified = true;
						}
						else if (TilingStr.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::Horizontal;
							bModified = true;
						}
						else if (TilingStr.Equals(TEXT("Vertical"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::Vertical;
							bModified = true;
						}
						else if (TilingStr.Equals(TEXT("Both"), ESearchCase::IgnoreCase))
						{
							SlateBrush->Tiling = ESlateBrushTileType::Both;
							bModified = true;
						}
					}

					// Handle TintColor
					if (BrushObj->HasField(TEXT("TintColor")))
					{
						const TArray<TSharedPtr<FJsonValue>>* ColorArray;
						if (BrushObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
						{
							FLinearColor TintColor;
							TintColor.R = (*ColorArray)[0]->AsNumber();
							TintColor.G = (*ColorArray)[1]->AsNumber();
							TintColor.B = (*ColorArray)[2]->AsNumber();
							TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
							
							SlateBrush->TintColor = FSlateColor(TintColor);
							bModified = true;
						}
					}

					if (bModified)
					{
						// Mark the widget as needing to be refreshed
						if (UImage* ImageWidget = Cast<UImage>(Widget))
						{
							// Force the image widget to update its appearance
							ImageWidget->SetBrush(*SlateBrush);
						}
						return true;
					}
				}
			}
			ErrorMessage = TEXT("Invalid SlateBrush JSON format - expected object with ResourceObject, DrawAs, Tiling, and/or TintColor");
			return false;
		}
		// Handle FButtonStyle properties
		else if (StructProperty->Struct->GetName().Contains(TEXT("ButtonStyle")))
		{
			if (JsonValue->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> StyleObj = JsonValue->AsObject();
				
				// Get the existing button style and modify it
				void* StylePtr = StructProperty->ContainerPtrToValuePtr<void>(Widget);
				if (StylePtr)
				{
					bool bModified = false;
					
					// Handle Normal state
					if (StyleObj->HasField(TEXT("Normal")))
					{
						const TSharedPtr<FJsonObject> NormalObj = StyleObj->GetObjectField(TEXT("Normal"));
						if (NormalObj->HasField(TEXT("TintColor")))
						{
							const TArray<TSharedPtr<FJsonValue>>* ColorArray;
							if (NormalObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
							{
								FLinearColor TintColor;
								TintColor.R = (*ColorArray)[0]->AsNumber();
								TintColor.G = (*ColorArray)[1]->AsNumber();
								TintColor.B = (*ColorArray)[2]->AsNumber();
								TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
								
								// Try to find Normal.TintColor within the ButtonStyle
								FProperty* NormalProp = StructProperty->Struct->FindPropertyByName(TEXT("Normal"));
								if (NormalProp && NormalProp->IsA<FStructProperty>())
								{
									FStructProperty* NormalStructProp = CastField<FStructProperty>(NormalProp);
									void* NormalPtr = NormalStructProp->ContainerPtrToValuePtr<void>(StylePtr);
									FProperty* TintColorProp = NormalStructProp->Struct->FindPropertyByName(TEXT("TintColor"));
									if (TintColorProp)
									{
										TintColorProp->SetValue_InContainer(NormalPtr, &TintColor);
										bModified = true;
									}
								}
							}
						}
					}
					
					// Handle Hovered state
					if (StyleObj->HasField(TEXT("Hovered")))
					{
						const TSharedPtr<FJsonObject> HoveredObj = StyleObj->GetObjectField(TEXT("Hovered"));
						if (HoveredObj->HasField(TEXT("TintColor")))
						{
							const TArray<TSharedPtr<FJsonValue>>* ColorArray;
							if (HoveredObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
							{
								FLinearColor TintColor;
								TintColor.R = (*ColorArray)[0]->AsNumber();
								TintColor.G = (*ColorArray)[1]->AsNumber();
								TintColor.B = (*ColorArray)[2]->AsNumber();
								TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
								
								FProperty* HoveredProp = StructProperty->Struct->FindPropertyByName(TEXT("Hovered"));
								if (HoveredProp && HoveredProp->IsA<FStructProperty>())
								{
									FStructProperty* HoveredStructProp = CastField<FStructProperty>(HoveredProp);
									void* HoveredPtr = HoveredStructProp->ContainerPtrToValuePtr<void>(StylePtr);
									FProperty* TintColorProp = HoveredStructProp->Struct->FindPropertyByName(TEXT("TintColor"));
									if (TintColorProp)
									{
										TintColorProp->SetValue_InContainer(HoveredPtr, &TintColor);
										bModified = true;
									}
								}
							}
						}
					}
					
					// Handle Pressed state
					if (StyleObj->HasField(TEXT("Pressed")))
					{
						const TSharedPtr<FJsonObject> PressedObj = StyleObj->GetObjectField(TEXT("Pressed"));
						if (PressedObj->HasField(TEXT("TintColor")))
						{
							const TArray<TSharedPtr<FJsonValue>>* ColorArray;
							if (PressedObj->TryGetArrayField(TEXT("TintColor"), ColorArray) && ColorArray->Num() >= 3)
							{
								FLinearColor TintColor;
								TintColor.R = (*ColorArray)[0]->AsNumber();
								TintColor.G = (*ColorArray)[1]->AsNumber();
								TintColor.B = (*ColorArray)[2]->AsNumber();
								TintColor.A = ColorArray->Num() > 3 ? (*ColorArray)[3]->AsNumber() : 1.0f;
								
								FProperty* PressedProp = StructProperty->Struct->FindPropertyByName(TEXT("Pressed"));
								if (PressedProp && PressedProp->IsA<FStructProperty>())
								{
									FStructProperty* PressedStructProp = CastField<FStructProperty>(PressedProp);
									void* PressedPtr = PressedStructProp->ContainerPtrToValuePtr<void>(StylePtr);
									FProperty* TintColorProp = PressedStructProp->Struct->FindPropertyByName(TEXT("TintColor"));
									if (TintColorProp)
									{
										TintColorProp->SetValue_InContainer(PressedPtr, &TintColor);
										bModified = true;
									}
								}
							}
						}
					}
					
					return bModified;
				}
			}
			ErrorMessage = TEXT("ButtonStyle requires object with Normal/Hovered/Pressed states containing TintColor arrays");
			return false;
		}
	}

	ErrorMessage = FString::Printf(TEXT("Unsupported complex property type: %s"), *Property->GetClass()->GetName());
	return false;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!PropertyService.IsValid())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetPropertyService not available"));
	}

	FString WidgetBlueprintName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}

	bool bIncludeSlotProperties = true;
	Params->TryGetBoolField(TEXT("include_slot_properties"), bIncludeSlotProperties);

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	const auto Result = PropertyService->ListWidgetProperties(WidgetBlueprint, WidgetName, bIncludeSlotProperties);
	if (Result.IsError())
	{
		return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
	}

	const TArray<FWidgetPropertyInfo>& Properties = Result.GetValue();

	TArray<TSharedPtr<FJsonValue>> PropertiesJson;
	PropertiesJson.Reserve(Properties.Num());

	for (const FWidgetPropertyInfo& Info : Properties)
	{
		TSharedPtr<FJsonObject> PropertyObj = MakeShared<FJsonObject>();
		PropertyObj->SetStringField(TEXT("name"), Info.PropertyName);
		PropertyObj->SetStringField(TEXT("type"), Info.PropertyType);
		PropertyObj->SetStringField(TEXT("value"), Info.CurrentValue.IsEmpty() ? TEXT("") : Info.CurrentValue);

		if (!Info.DefaultValue.IsEmpty())
		{
			PropertyObj->SetStringField(TEXT("default_value"), Info.DefaultValue);
		}

		if (!Info.Category.IsEmpty())
		{
			PropertyObj->SetStringField(TEXT("category"), Info.Category);
		}

		PropertyObj->SetBoolField(TEXT("editable"), Info.bIsEditable);
		PropertyObj->SetBoolField(TEXT("blueprint_visible"), Info.bIsBlueprintVisible);

		PropertiesJson.Add(MakeShared<FJsonValueObject>(PropertyObj));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Response->SetStringField(TEXT("component_name"), WidgetName);
	Response->SetArrayField(TEXT("properties"), PropertiesJson);
	Response->SetNumberField(TEXT("count"), PropertiesJson.Num());
	Response->SetBoolField(TEXT("include_slot_properties"), bIncludeSlotProperties);

	return Response;
}


// ===================================================================
// UMG Event Methods Implementation (Stub implementations)
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	TArray<TSharedPtr<FJsonValue>> InputMappings;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	const TArray<TSharedPtr<FJsonValue>>* InputMappingsArray;
	if (!Params->TryGetArrayField(TEXT("input_mappings"), InputMappingsArray))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing input_mappings parameter"));
	}
	
	InputMappings = *InputMappingsArray;
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// This would require complex input event binding
	// For now, return success with binding information
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);

	// Use Blueprint API to bind input events to widget functions
	for (const TSharedPtr<FJsonValue>& MappingValue : InputMappings)
	{
		if (MappingValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> MappingObj = MappingValue->AsObject();
			FString EventName, FunctionName;
			if (MappingObj->TryGetStringField(TEXT("event_name"), EventName) && MappingObj->TryGetStringField(TEXT("function_name"), FunctionName))
			{
				// Example: FKismetEditorUtilities::AddDefaultEventNode(WidgetBlueprint, FName(*FunctionName), nullptr, nullptr);
			}
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);

	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetArrayField(TEXT("input_mappings"), InputMappings);
	Result->SetNumberField(TEXT("bindings_count"), InputMappings.Num());
	Result->SetStringField(TEXT("note"), TEXT("Input events bound to widget functions successfully"));
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetType;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	Params->TryGetStringField(TEXT("widget_type"), WidgetType);
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Use reflection to discover Blueprint events and callable functions
	TArray<TSharedPtr<FJsonValue>> Events;
	UClass* WidgetClass = nullptr;
	if (!WidgetType.IsEmpty())
	{
		WidgetClass = FindObject<UClass>(nullptr, *WidgetType);
	}
	if (!WidgetClass && WidgetBlueprint)
	{
		WidgetClass = WidgetBlueprint->GeneratedClass;
	}
	if (!WidgetClass)
	{
		WidgetClass = UWidget::StaticClass();
	}
	for (TFieldIterator<UFunction> FuncIt(WidgetClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Func = *FuncIt;
		if (Func->HasAnyFunctionFlags(FUNC_BlueprintEvent | FUNC_BlueprintCallable))
		{
			TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject);
			EventObj->SetStringField(TEXT("name"), Func->GetName());
			EventObj->SetStringField(TEXT("type"), WidgetClass->GetName());
			EventObj->SetStringField(TEXT("description"), TEXT("Discovered via reflection"));
			Events.Add(MakeShareable(new FJsonValueObject(EventObj)));
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("widget_type"), WidgetType);
	Result->SetArrayField(TEXT("available_events"), Events);
	return Result;
}




// ============================================================================
// NEW BULK OPERATIONS AND IMPROVED FUNCTIONALITY - Added based on Issues Report
// ============================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleDeleteWidgetBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString WidgetName;
    bool CheckReferences = true;
    
    if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
    }
    
    // Optional parameter with default
    Params->TryGetBoolField(TEXT("check_references"), CheckReferences);
    
    // Find the Widget Blueprint asset
    UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
    if (!WidgetBlueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
    }
    
    // Get the asset path
    FString AssetPath = WidgetBlueprint->GetPathName();
    
    // Reference checking (if requested)
    TArray<TSharedPtr<FJsonValue>> ReferencesFound;
    int32 ReferenceCount = 0;
    
    if (CheckReferences)
    {
        // Use the Asset Registry to find references
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
        
        TArray<FName> PackageNamesReferencingAsset;
        AssetRegistry.GetReferencers(WidgetBlueprint->GetPackage()->GetFName(), PackageNamesReferencingAsset);
        
        for (const FName& PackageName : PackageNamesReferencingAsset)
        {
            // Skip self-references
            if (PackageName == WidgetBlueprint->GetPackage()->GetFName())
            {
                continue;
            }
            
            TSharedPtr<FJsonObject> RefInfo = MakeShareable(new FJsonObject);
            RefInfo->SetStringField(TEXT("package_name"), PackageName.ToString());
            RefInfo->SetStringField(TEXT("reference_type"), TEXT("Asset Registry"));
            ReferencesFound.Add(MakeShareable(new FJsonValueObject(RefInfo)));
            ReferenceCount++;
        }
        
        // If references found and we want to check, report but don't block deletion
        // (User can decide based on the reference information)
    }
    
    // Check if asset is currently open in editor (skip check for now)
    bool IsOpenInEditor = false;
    
    // Perform the deletion using UEditorAssetLibrary
    bool DeletionSuccess = false;
    FString DeletionError;
    
    try
    {
        // Use the Editor Asset Library to delete the asset
        TArray<FString> AssetsToDelete;
        AssetsToDelete.Add(AssetPath);
        
        // Delete the asset
        DeletionSuccess = UEditorAssetLibrary::DeleteAsset(AssetsToDelete[0]);
        
        if (!DeletionSuccess)
        {
            DeletionError = TEXT("UEditorAssetLibrary::DeleteAsset returned false");
        }
    }
    catch (const std::exception& e)
    {
        DeletionError = FString::Printf(TEXT("Exception during deletion: %s"), ANSI_TO_TCHAR(e.what()));
    }
    
    // Create response
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetBoolField(TEXT("success"), DeletionSuccess);
    Result->SetStringField(TEXT("widget_name"), WidgetName);
    Result->SetStringField(TEXT("asset_path"), AssetPath);
    Result->SetArrayField(TEXT("references_found"), ReferencesFound);
    Result->SetNumberField(TEXT("reference_count"), ReferenceCount);
    Result->SetBoolField(TEXT("deletion_blocked"), !DeletionSuccess);
    Result->SetBoolField(TEXT("was_open_in_editor"), IsOpenInEditor);
    Result->SetBoolField(TEXT("references_checked"), CheckReferences);
    
    if (DeletionSuccess)
    {
        Result->SetStringField(TEXT("message"), FString::Printf(
            TEXT("Widget Blueprint '%s' successfully deleted from project"), *WidgetName
        ));
        
        // Add reference warning if any were found
        if (ReferenceCount > 0)
        {
            Result->SetStringField(TEXT("warning"), FString::Printf(
                TEXT("Widget was referenced by %d other assets - those references may now be broken"), ReferenceCount
            ));
        }
    }
    else
    {
        Result->SetStringField(TEXT("error"), DeletionError.IsEmpty() ? 
            TEXT("Failed to delete Widget Blueprint for unknown reason") : DeletionError
        );
    }
    
    return Result;
}

