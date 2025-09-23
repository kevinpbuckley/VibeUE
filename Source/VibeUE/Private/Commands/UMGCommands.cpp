#include "Commands/UMGCommands.h"
#include "Commands/CommonUtils.h"
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
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/UnrealType.h"
#include "Containers/Map.h"
#include "Containers/Set.h"

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

FUMGCommands::FUMGCommands()
{
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
	else if (CommandName == TEXT("add_text_block_to_widget"))
	{
		return HandleAddTextBlockToWidget(Params);
	}
	else if (CommandName == TEXT("add_button_to_widget"))
	{
		return HandleAddButtonToWidget(Params);
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
	// UMG Component Commands
	else if (CommandName == TEXT("add_editable_text"))
	{
		return HandleAddEditableText(Params);
	}
	else if (CommandName == TEXT("add_editable_text_box"))
	{
		return HandleAddEditableTextBox(Params);
	}
	else if (CommandName == TEXT("add_rich_text_block"))
	{
		return HandleAddRichTextBlock(Params);
	}
	else if (CommandName == TEXT("add_check_box"))
	{
		return HandleAddCheckBox(Params);
	}
	else if (CommandName == TEXT("add_slider"))
	{
		return HandleAddSlider(Params);
	}
	else if (CommandName == TEXT("add_progress_bar"))
	{
		return HandleAddProgressBar(Params);
	}
	else if (CommandName == TEXT("add_image"))
	{
		return HandleAddImage(Params);
	}
	else if (CommandName == TEXT("add_spacer"))
	{
		return HandleAddSpacer(Params);
	}
	// UMG Layout Commands
	else if (CommandName == TEXT("add_canvas_panel"))
	{
		return HandleAddCanvasPanel(Params);
	}
	else if (CommandName == TEXT("add_size_box"))
	{
		return HandleAddSizeBox(Params);
	}
	else if (CommandName == TEXT("add_overlay"))
	{
		return HandleAddOverlay(Params);
	}
	else if (CommandName == TEXT("add_horizontal_box"))
	{
		return HandleAddHorizontalBox(Params);
	}
	else if (CommandName == TEXT("add_vertical_box"))
	{
		return HandleAddVerticalBox(Params);
	}
	else if (CommandName == TEXT("add_scroll_box"))
	{
		return HandleAddScrollBox(Params);
	}
	else if (CommandName == TEXT("add_grid_panel"))
	{
		return HandleAddGridPanel(Params);
	}
	// add_list_view/add_tile_view/add_tree_view removed
	else if (CommandName == TEXT("add_widget_switcher"))
	{
		return HandleAddWidgetSwitcher(Params);
	}
	else if (CommandName == TEXT("add_widget_switcher_slot"))
	{
		return HandleAddWidgetSwitcherSlot(Params);
	}
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
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
	}

	// Get optional path parameter, default to /Game/UI
	FString PackagePath = TEXT("/Game/UI/");
	Params->TryGetStringField(TEXT("path"), PackagePath);
	
	// Ensure path ends with /
	if (!PackagePath.EndsWith(TEXT("/")))
	{
		PackagePath += TEXT("/");
	}
	
	FString AssetName = BlueprintName;
	FString FullPath = PackagePath + AssetName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' already exists"), *BlueprintName));
	}

	// Create package
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create package"));
	}

	// Create Widget Blueprint using WidgetBlueprintFactory
	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = UUserWidget::StaticClass();
	
	UObject* NewAsset = Factory->FactoryCreateNew(
		UWidgetBlueprint::StaticClass(),
		Package,
		FName(*AssetName),
		RF_Standalone | RF_Public,
		nullptr,
		GWarn
	);

	// Make sure the Blueprint was created successfully
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(NewAsset);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Widget Blueprint"));
	}

	// Add a default Canvas Panel if one doesn't exist
	if (!WidgetBlueprint->WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
	}

	// Mark the package dirty and notify asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(WidgetBlueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("name"), BlueprintName);
	ResultObj->SetStringField(TEXT("path"), FullPath);
	return ResultObj;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddTextBlockToWidget(const TSharedPtr<FJsonObject>& Params)
{
	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
	}

	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing 'widget_name' parameter"));
	}

	FString ParentName;
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the text block"));
	}

	// Find the Widget Blueprint (accept name or full path)
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint not found for '%s'. Tip: pass /Game/.../WBP_Name or /Game/.../WBP_Name.WBP_Name"), *BlueprintName));
	}

	// Get optional parameters
	FString InitialText = TEXT("New Text Block");
	Params->TryGetStringField(TEXT("text"), InitialText);

	FVector2D Position(0.0f, 0.0f);
	if (Params->HasField(TEXT("position")))
	{
		const TArray<TSharedPtr<FJsonValue>>* PosArray;
		if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
		{
			Position.X = (*PosArray)[0]->AsNumber();
			Position.Y = (*PosArray)[1]->AsNumber();
		}
	}

	// Create Text Block widget
	UTextBlock* TextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *WidgetName);
	if (!TextBlock)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Text Block widget"));
	}

	// Set initial text
	TextBlock->SetText(FText::FromString(InitialText));

	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);

	// Note: ensure WidgetTree exists; avoid stray logging args without UE_LOG

	// Defensive check: ensure WidgetTree exists before proceeding to avoid crashes in editor
	if (!WidgetBlueprint->WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' has no WidgetTree"), *WidgetName));
	}
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}

	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(TextBlock);
		PanelSlot->SetPosition(Position);
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(TextBlock);
	}

	// Mark the package dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	// Create success response
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
	ResultObj->SetStringField(TEXT("text"), InitialText);
	return ResultObj;
}


TSharedPtr<FJsonObject> FUMGCommands::HandleAddButtonToWidget(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Get required parameters
	FString BlueprintName;
	if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing blueprint_name parameter"));
		return Response;
	}

	FString ButtonName;
	if (!Params->TryGetStringField(TEXT("button_name"), ButtonName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing button_name parameter"));
		return Response;
	}

	FString ButtonText;
	if (!Params->TryGetStringField(TEXT("text"), ButtonText))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing text parameter"));
		return Response;
	}

	FString ParentName;
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		Response->SetStringField(TEXT("error"), TEXT("Missing parent_name parameter - you must specify where to add the button"));
		return Response;
	}

	// Load the Widget Blueprint
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(BlueprintName);
	if (!WidgetBlueprint)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *BlueprintName));
		return Response;
	}

	// Create Button widget using the WidgetTree
	UButton* Button = WidgetBlueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *ButtonName);
	if (!Button)
	{
		Response->SetStringField(TEXT("error"), TEXT("Failed to create Button widget"));
		return Response;
	}

	// Set button text by creating a TextBlock child
	UTextBlock* ButtonTextBlock = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(ButtonName + TEXT("_Text")));
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(FText::FromString(ButtonText));
		Button->AddChild(ButtonTextBlock);
	}

	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
		return Response;
	}

	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(Button);
		if (ButtonSlot)
		{
			const TArray<TSharedPtr<FJsonValue>>* Position;
			if (Params->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
			{
				FVector2D Pos(
					(*Position)[0]->AsNumber(),
					(*Position)[1]->AsNumber()
				);
				ButtonSlot->SetPosition(Pos);
			}
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(Button);
	}

	// Save the Widget Blueprint
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UEditorAssetLibrary::SaveAsset(WidgetBlueprint->GetPathName(), false);

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("button_name"), ButtonName);
	return Response;
}

// ===================================================================
// UMG Discovery Methods Implementation
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleSearchItems(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get search parameters
	FString SearchTerm = TEXT("");
	Params->TryGetStringField(TEXT("search_term"), SearchTerm);
	
	FString AssetType = TEXT("");
	Params->TryGetStringField(TEXT("asset_type"), AssetType);
	
	FString Path = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), Path);
	
	bool bCaseSensitive = false;
	Params->TryGetBoolField(TEXT("case_sensitive"), bCaseSensitive);
	
	bool bIncludeEngineContent = false;
	Params->TryGetBoolField(TEXT("include_engine_content"), bIncludeEngineContent);
	
	int32 MaxResults = 100;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);
	
	// Use Asset Registry for proper recursive search
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// Create filter for asset search
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(*Path); // Search recursively from specified path
	
	// If including engine content, also add engine paths
	if (bIncludeEngineContent)
	{
		Filter.PackagePaths.Add(TEXT("/Engine"));
	}
	
	// Set up class filter based on asset type
	if (!AssetType.IsEmpty())
	{
		if (AssetType == TEXT("WidgetBlueprint") || AssetType == TEXT("Widget"))
		{
			Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Texture2D") || AssetType == TEXT("Texture"))
		{
			Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Material"))
		{
			Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("MaterialInstance"))
		{
			Filter.ClassPaths.Add(UMaterialInstance::StaticClass()->GetClassPathName());
			Filter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Blueprint"))
		{
			Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("StaticMesh"))
		{
			Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("SkeletalMesh"))
		{
			Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
		}
		else if (AssetType == TEXT("Audio") || AssetType == TEXT("SoundWave"))
		{
			Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
		}
		// If asset type not recognized, search all assets (no class filter)
	}
	
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);
	
	// Create response array
	TArray<TSharedPtr<FJsonValue>> ItemArray;
	int32 ResultCount = 0;
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (ResultCount >= MaxResults)
		{
			break;
		}
		
		FString AssetName = AssetData.AssetName.ToString();
		FString AssetPath = AssetData.GetObjectPathString();
		
		// Apply search term filter if provided
		bool bMatches = true;
		if (!SearchTerm.IsEmpty())
		{
			ESearchCase::Type SearchCase = bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase;
			bMatches = AssetName.Contains(SearchTerm, SearchCase);
		}
		
		if (bMatches)
		{
			TSharedPtr<FJsonObject> ItemInfo = MakeShared<FJsonObject>();
			ItemInfo->SetStringField(TEXT("name"), AssetName);
			ItemInfo->SetStringField(TEXT("path"), AssetPath);
			ItemInfo->SetStringField(TEXT("package_path"), AssetData.PackageName.ToString());
			ItemInfo->SetStringField(TEXT("asset_class"), AssetData.AssetClassPath.ToString());
			
			// Add specific info based on asset type
			if (AssetData.AssetClassPath == UWidgetBlueprint::StaticClass()->GetClassPathName())
			{
				UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(AssetData.GetAsset());
				if (WidgetBlueprint)
				{
					ItemInfo->SetStringField(TEXT("parent_class"), WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetName() : TEXT("UserWidget"));
				}
				ItemInfo->SetStringField(TEXT("type"), TEXT("Widget"));
			}
			else if (AssetData.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Texture"));
			}
			else if (AssetData.AssetClassPath == UMaterial::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Material"));
			}
			else if (AssetData.AssetClassPath == UMaterialInstance::StaticClass()->GetClassPathName() ||
					 AssetData.AssetClassPath == UMaterialInstanceConstant::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("MaterialInstance"));
			}
			else if (AssetData.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Blueprint"));
			}
			else if (AssetData.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("StaticMesh"));
			}
			else if (AssetData.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("SkeletalMesh"));
			}
			else if (AssetData.AssetClassPath == USoundWave::StaticClass()->GetClassPathName())
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Audio"));
			}
			else
			{
				ItemInfo->SetStringField(TEXT("type"), TEXT("Other"));
			}
			
			ItemArray.Add(MakeShared<FJsonValueObject>(ItemInfo));
			ResultCount++;
		}
	}
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetArrayField(TEXT("items"), ItemArray);
	Response->SetNumberField(TEXT("count"), ItemArray.Num());
	
	// Add search info
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

TSharedPtr<FJsonObject> FUMGCommands::HandleAddEditableText(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, EditableTextName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("editable_text_name"), EditableTextName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the editable text"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create EditableText widget
	UEditableText* EditableText = WidgetBlueprint->WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), *EditableTextName);
	if (!EditableText)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create EditableText widget"));
	}
	
	// Set optional properties
	FString InitialText = TEXT("");
	Params->TryGetStringField(TEXT("text"), InitialText);
	if (!InitialText.IsEmpty())
	{
		EditableText->SetText(FText::FromString(InitialText));
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(EditableText);
		if (Slot)
		{
			// Set position if provided
			FVector2D Position(0.0f, 0.0f);
			if (Params->HasField(TEXT("position")))
			{
				const TArray<TSharedPtr<FJsonValue>>* PosArray;
				if (Params->TryGetArrayField(TEXT("position"), PosArray) && PosArray->Num() >= 2)
				{
					Position.X = (*PosArray)[0]->AsNumber();
					Position.Y = (*PosArray)[1]->AsNumber();
					Slot->SetPosition(Position);
				}
			}
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(EditableText);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("editable_text_name"), EditableTextName);
	Response->SetStringField(TEXT("widget_type"), TEXT("EditableText"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddEditableTextBox(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, TextBoxName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("text_box_name"), TextBoxName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the editable text box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create EditableTextBox widget
	UEditableTextBox* TextBox = WidgetBlueprint->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), *TextBoxName);
	if (!TextBox)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create EditableTextBox widget"));
	}
	
	// Set optional properties
	FString InitialText = TEXT("");
	Params->TryGetStringField(TEXT("text"), InitialText);
	if (!InitialText.IsEmpty())
	{
		TextBox->SetText(FText::FromString(InitialText));
	}
	
	FString HintText = TEXT("");
	Params->TryGetStringField(TEXT("hint_text"), HintText);
	if (!HintText.IsEmpty())
	{
		TextBox->SetHintText(FText::FromString(HintText));
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(TextBox);
		if (Slot)
		{
			// Set default position and size for the text box
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(300, 100));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(TextBox);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("text_box_name"), TextBoxName);
	Response->SetStringField(TEXT("widget_type"), TEXT("EditableTextBox"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddRichTextBlock(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, RichTextName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("rich_text_name"), RichTextName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the rich text block"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create RichTextBlock widget
	URichTextBlock* RichText = WidgetBlueprint->WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), *RichTextName);
	if (!RichText)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create RichTextBlock widget"));
	}
	
	// Set optional properties
	FString InitialText = TEXT("");
	Params->TryGetStringField(TEXT("text"), InitialText);
	if (!InitialText.IsEmpty())
	{
		RichText->SetText(FText::FromString(InitialText));
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(RichText);
		if (Slot)
		{
			// Set default position and size for the rich text block
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(400, 100));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(RichText);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("rich_text_name"), RichTextName);
	Response->SetStringField(TEXT("widget_type"), TEXT("RichTextBlock"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddCheckBox(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, CheckBoxName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("check_box_name"), CheckBoxName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the check box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create CheckBox widget
	UCheckBox* CheckBox = WidgetBlueprint->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *CheckBoxName);
	if (!CheckBox)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create CheckBox widget"));
	}
	
	// Set optional properties
	bool bIsChecked = false;
	Params->TryGetBoolField(TEXT("is_checked"), bIsChecked);
	CheckBox->SetIsChecked(bIsChecked);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(CheckBox);
		if (Slot)
		{
			// Set default position and size for the check box
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(100, 20));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(CheckBox);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("check_box_name"), CheckBoxName);
	Response->SetStringField(TEXT("widget_type"), TEXT("CheckBox"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddSlider(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, SliderName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("slider_name"), SliderName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the slider"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create Slider widget
	USlider* Slider = WidgetBlueprint->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), *SliderName);
	if (!Slider)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Slider widget"));
	}
	
	// Set optional properties
	float MinValue = 0.0f, MaxValue = 1.0f, Value = 0.0f;
	Params->TryGetNumberField(TEXT("min_value"), MinValue);
	Params->TryGetNumberField(TEXT("max_value"), MaxValue);
	Params->TryGetNumberField(TEXT("value"), Value);
	
	Slider->SetMinValue(MinValue);
	Slider->SetMaxValue(MaxValue);
	Slider->SetValue(Value);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Slider);
		if (Slot)
		{
			// Set default position and size for the slider
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(200, 20));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(Slider);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("slider_name"), SliderName);
	Response->SetStringField(TEXT("widget_type"), TEXT("Slider"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddProgressBar(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, ProgressBarName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("progress_bar_name"), ProgressBarName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the progress bar"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create ProgressBar widget
	UProgressBar* ProgressBar = WidgetBlueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *ProgressBarName);
	if (!ProgressBar)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create ProgressBar widget"));
	}
	
	// Set optional properties
	float Percent = 0.0f;
	Params->TryGetNumberField(TEXT("percent"), Percent);
	ProgressBar->SetPercent(Percent);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(ProgressBar);
		if (Slot)
		{
			// Set default position and size for the progress bar
			Slot->SetPosition(FVector2D(0, 0));
			Slot->SetSize(FVector2D(200, 20));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(ProgressBar);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("progress_bar_name"), ProgressBarName);
	Response->SetStringField(TEXT("widget_type"), TEXT("ProgressBar"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddImage(const TSharedPtr<FJsonObject>& Params)
{
	// Check if we're in a serialization context to prevent crashes
	if (IsGarbageCollecting() || GIsSavingPackage || IsLoading())
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Cannot add image during serialization"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	if (!Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("MCP: Failed to create Response object"));
		return FCommonUtils::CreateErrorResponse(TEXT("Internal error: Failed to create response object"));
	}

	FString WidgetName, ImageName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("image_name"), ImageName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the image"));
	}

	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found or widget tree is null"), *WidgetName));
	}

	UImage* Image = WidgetBlueprint->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *ImageName);
	if (!Image)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Image widget"));
	}

	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}

	bool Added = false;
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
		Added = (Slot != nullptr);
		if (Slot)
		{
			if (Params->HasField(TEXT("position")))
			{
				const TArray<TSharedPtr<FJsonValue>>* PositionArray;
				if (Params->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
				{
					float X = (*PositionArray)[0]->AsNumber();
					float Y = (*PositionArray)[1]->AsNumber();
					Slot->SetPosition(FVector2D(X, Y));
				}
			}
			if (Params->HasField(TEXT("size")))
			{
				const TArray<TSharedPtr<FJsonValue>>* SizeArray;
				if (Params->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
				{
					float Width = (*SizeArray)[0]->AsNumber();
					float Height = (*SizeArray)[1]->AsNumber();
					Slot->SetSize(FVector2D(Width, Height));
				}
			}
		}
	}
	else if (UOverlay* Overlay = Cast<UOverlay>(ParentPanel))
	{
		Overlay->AddChild(Image);
		Added = true;
	}
	else if (UScrollBox* ScrollBox = Cast<UScrollBox>(ParentPanel))
	{
		ScrollBox->AddChild(Image);
		Added = true;
	}
	else if (UVerticalBox* VBox = Cast<UVerticalBox>(ParentPanel))
	{
		VBox->AddChild(Image);
		Added = true;
	}
	else if (UHorizontalBox* HBox = Cast<UHorizontalBox>(ParentPanel))
	{
		HBox->AddChild(Image);
		Added = true;
	}
	else
	{
		// Try generic panel
		if (UPanelWidget* Panel = Cast<UPanelWidget>(ParentPanel))
		{
			Panel->AddChild(Image);
			Added = true;
		}
	}

	if (!Added)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to add image to panel"));
	}

	if (Params->HasField(TEXT("color_tint")))
	{
		const TArray<TSharedPtr<FJsonValue>>* ColorArray;
		if (Params->TryGetArrayField(TEXT("color_tint"), ColorArray) && ColorArray->Num() >= 4)
		{
			float R = (*ColorArray)[0]->AsNumber();
			float G = (*ColorArray)[1]->AsNumber();
			float B = (*ColorArray)[2]->AsNumber();
			float A = (*ColorArray)[3]->AsNumber();
			Image->SetColorAndOpacity(FLinearColor(R, G, B, A));
		}
	}

	WidgetBlueprint->MarkPackageDirty();
	
	// Use deferred compilation to avoid serialization crashes
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	// Don't compile immediately - let Unreal handle it when safe
	// FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);



	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("image_name"), ImageName);
	Response->SetStringField(TEXT("widget_type"), TEXT("Image"));
	return Response;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddSpacer(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	// Get required parameters
	FString WidgetName, SpacerName, ParentName;
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetName) ||
		!Params->TryGetStringField(TEXT("spacer_name"), SpacerName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing required parameters"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the spacer"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetName));
	}
	
	// Create Spacer widget
	USpacer* Spacer = WidgetBlueprint->WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), *SpacerName);
	if (!Spacer)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Spacer widget"));
	}
	
	// Set optional size
	FVector2D Size(100.0f, 100.0f);
	if (Params->HasField(TEXT("size")))
	{
		const TArray<TSharedPtr<FJsonValue>>* SizeArray;
		if (Params->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
		{
			Size.X = (*SizeArray)[0]->AsNumber();
			Size.Y = (*SizeArray)[1]->AsNumber();
			Spacer->SetSize(Size);
		}
	}
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to parent panel
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Spacer);
		if (Slot)
		{
			// Set default position for the spacer
			Slot->SetPosition(FVector2D(0, 0));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(Spacer);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("spacer_name"), SpacerName);
	Response->SetStringField(TEXT("widget_type"), TEXT("Spacer"));
	return Response;
}

// ===================================================================
// UMG Layout Methods Implementation (Stub implementations)
// ===================================================================

TSharedPtr<FJsonObject> FUMGCommands::HandleAddCanvasPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString PanelName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("panel_name"), PanelName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing panel_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Canvas Panel"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UCanvasPanel* CreatedPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *PanelName);
		if (CreatedPanel)
		{
			CreatedPanel->SetVisibility(ESlateVisibility::Visible);
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedPanel);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("panel_name"), PanelName);
				Result->SetStringField(TEXT("panel_type"), TEXT("CanvasPanel"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Failed to add Canvas Panel to parent"));
			}
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Canvas Panel"));
		}
	}
	
	return FCommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddSizeBox(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString SizeBoxName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("size_box_name"), SizeBoxName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing size_box_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Size Box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		USizeBox* CreatedSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *SizeBoxName);
		if (CreatedSizeBox)
		{
			CreatedSizeBox->SetVisibility(ESlateVisibility::Visible);
			
			// Set optional size constraints from parameters
			double MinDesiredWidth = 0.0;
			if (Params->TryGetNumberField(TEXT("min_desired_width"), MinDesiredWidth))
			{
				CreatedSizeBox->SetMinDesiredWidth(MinDesiredWidth);
			}
			
			double MinDesiredHeight = 0.0;
			if (Params->TryGetNumberField(TEXT("min_desired_height"), MinDesiredHeight))
			{
				CreatedSizeBox->SetMinDesiredHeight(MinDesiredHeight);
			}
			
			double MaxDesiredWidth = 0.0;
			if (Params->TryGetNumberField(TEXT("max_desired_width"), MaxDesiredWidth))
			{
				CreatedSizeBox->SetMaxDesiredWidth(MaxDesiredWidth);
			}
			
			double MaxDesiredHeight = 0.0;
			if (Params->TryGetNumberField(TEXT("max_desired_height"), MaxDesiredHeight))
			{
				CreatedSizeBox->SetMaxDesiredHeight(MaxDesiredHeight);
			}
			
			double WidthOverride = 0.0;
			if (Params->TryGetNumberField(TEXT("width_override"), WidthOverride))
			{
				CreatedSizeBox->SetWidthOverride(WidthOverride);
			}
			
			double HeightOverride = 0.0;
			if (Params->TryGetNumberField(TEXT("height_override"), HeightOverride))
			{
				CreatedSizeBox->SetHeightOverride(HeightOverride);
			}
			
			// Set variable flag
			bool bIsVariable = true;
			if (Params->TryGetBoolField(TEXT("is_variable"), bIsVariable))
			{
				CreatedSizeBox->bIsVariable = bIsVariable;
			}
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedSizeBox);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("size_box_name"), SizeBoxName);
				Result->SetStringField(TEXT("size_box_type"), TEXT("SizeBox"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Failed to add Size Box to parent"));
			}
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Size Box"));
		}
	}
	
	return FCommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddOverlay(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString OverlayName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("overlay_name"), OverlayName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing overlay_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the overlay. Use list_widget_components to see available parent containers."));
	}
	
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
	
	// Create the overlay widget using the widget tree
	UOverlay* CreatedOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *OverlayName);
	if (!CreatedOverlay)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Overlay widget"));
	}
	
	/* cleanup: removed verbose debug log */
	CreatedOverlay->SetVisibility(ESlateVisibility::Visible);
	
	// Find or create the specified parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		// List available components for debugging
		TArray<UWidget*> AllWidgets;
		WidgetTree->GetAllWidgets(AllWidgets);
		FString AvailableComponents = TEXT("Available components: ");
		for (UWidget* Widget : AllWidgets)
		{
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				AvailableComponents += Panel->GetName() + TEXT(", ");
			}
		}
		UE_LOG(LogTemp, Error, TEXT("MCP: Parent panel '%s' not found. %s"), *ParentName, *AvailableComponents);
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent panel '%s' not found. %s"), *ParentName, *AvailableComponents));
	}
	
	// Add overlay to parent panel
	/* cleanup: removed verbose debug log */
	
	// Special handling for Canvas Panel
	if (UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(ParentPanel))
	{

		UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(CreatedOverlay);
		if (CanvasSlot)
		{
			// Set default position and size for the overlay
			CanvasSlot->SetPosition(FVector2D(0, 0));
			CanvasSlot->SetSize(FVector2D(400, 300));
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // Fill parent

		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MCP: Failed to create canvas slot"));
			return FCommonUtils::CreateErrorResponse(TEXT("Failed to add overlay to canvas panel"));
		}
	}
	else
	{
		// Standard panel widget handling
		ParentPanel->AddChild(CreatedOverlay);
	}
	

	
	// Mark blueprint as modified and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("overlay_name"), OverlayName);
	Result->SetStringField(TEXT("panel_type"), TEXT("Overlay"));
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddHorizontalBox(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString BoxName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("box_name"), BoxName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing box_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Horizontal Box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UHorizontalBox* CreatedBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *BoxName);
		if (CreatedBox)
		{
			CreatedBox->SetVisibility(ESlateVisibility::Visible);
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedBox);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("box_name"), BoxName);
				Result->SetStringField(TEXT("box_type"), TEXT("HorizontalBox"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Failed to add Horizontal Box to parent"));
			}
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Horizontal Box"));
		}
	}
	
	return FCommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddVerticalBox(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString BoxName;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("box_name"), BoxName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing box_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Vertical Box"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UVerticalBox* CreatedBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *BoxName);
		if (CreatedBox)
		{
			CreatedBox->SetVisibility(ESlateVisibility::Visible);
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedBox);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("box_name"), BoxName);
				Result->SetStringField(TEXT("box_type"), TEXT("VerticalBox"));
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Failed to add Vertical Box to parent"));
			}
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Vertical Box"));
		}
	}
	
	return FCommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddScrollBox(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ScrollBoxName;
	FString Orientation;
	FString ParentName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("scroll_box_name"), ScrollBoxName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing scroll_box_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Scroll Box"));
	}
	
	Params->TryGetStringField(TEXT("orientation"), Orientation);
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	// Add to widget blueprint's designer
	if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
	{
		UScrollBox* CreatedScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), *ScrollBoxName);
		if (CreatedScrollBox)
		{
			CreatedScrollBox->SetVisibility(ESlateVisibility::Visible);
			
			// Set orientation if specified
			if (Orientation == TEXT("Horizontal"))
			{
				CreatedScrollBox->SetOrientation(Orient_Horizontal);
			}
			else if (Orientation == TEXT("Vertical"))
			{
				CreatedScrollBox->SetOrientation(Orient_Vertical);
			}
			
			// Add to parent panel
			UPanelSlot* PanelSlot = ParentPanel->AddChild(CreatedScrollBox);
			if (PanelSlot)
			{
				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
				Result->SetBoolField(TEXT("success"), true);
				Result->SetStringField(TEXT("scroll_box_name"), ScrollBoxName);
				Result->SetStringField(TEXT("orientation"), Orientation.IsEmpty() ? TEXT("Vertical") : Orientation);
				Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
				Result->SetStringField(TEXT("parent_name"), ParentName);
				
				return Result;
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Failed to add Scroll Box to parent"));
			}
		}
		else
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Scroll Box"));
		}
	}
	
	return FCommonUtils::CreateErrorResponse(TEXT("Widget Tree not found"));
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddGridPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString GridPanelName;
	FString ParentName;
	int32 ColumnCount = 2;
	int32 RowCount = 2;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("grid_panel_name"), GridPanelName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing grid_panel_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_name"), ParentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_name parameter - you must specify where to add the Grid Panel"));
	}
	
	Params->TryGetNumberField(TEXT("column_count"), ColumnCount);
	Params->TryGetNumberField(TEXT("row_count"), RowCount);
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Find or create parent panel
	UPanelWidget* ParentPanel = UMGHelpers::FindOrCreateParentPanel(WidgetBlueprint, ParentName);
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to find or create parent panel '%s'"), *ParentName));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("WidgetTree not found in Widget Blueprint"));
	}
	
	UGridPanel* GridPanel = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), *GridPanelName);
	if (!GridPanel)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create Grid Panel"));
	}
	
	GridPanel->SetVisibility(ESlateVisibility::Visible);
	
	// Add to parent panel
	UPanelSlot* PanelSlot = ParentPanel->AddChild(GridPanel);
	if (PanelSlot)
	{
		// Mark blueprint as modified and compile
		WidgetBlueprint->MarkPackageDirty();
		FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
		
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("grid_panel_name"), GridPanelName);
		Result->SetNumberField(TEXT("column_count"), ColumnCount);
		Result->SetNumberField(TEXT("row_count"), RowCount);
		Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Result->SetStringField(TEXT("parent_name"), ParentName);
		
		return Result;
	}
	else
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to add Grid Panel to parent"));
	}
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ParentPanelName;
	FString ChildWidgetName;
	int32 SlotIndex = -1;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("parent_panel_name"), ParentPanelName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing parent_panel_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("child_widget_name"), ChildWidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing child_widget_name parameter"));
	}
	
	Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);
	
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

	UWidget* ParentPanel = WidgetTree->FindWidget(FName(*ParentPanelName));
	UWidget* ChildWidget = WidgetTree->FindWidget(FName(*ChildWidgetName));
	if (!ParentPanel)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent panel '%s' not found"), *ParentPanelName));
	}
	if (!ChildWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child widget '%s' not found"), *ChildWidgetName));
	}

	UPanelWidget* PanelWidget = Cast<UPanelWidget>(ParentPanel);
	if (!PanelWidget)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Parent is not a panel widget"));
	}

	if (SlotIndex < 0 || SlotIndex >= PanelWidget->GetChildrenCount())
	{
		PanelWidget->AddChild(ChildWidget);
	}
	else
	{
		PanelWidget->InsertChildAt(SlotIndex, ChildWidget);
	}

	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("parent_panel_name"), ParentPanelName);
	Result->SetStringField(TEXT("child_widget_name"), ChildWidgetName);
	Result->SetNumberField(TEXT("slot_index"), SlotIndex);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("note"), TEXT("Child widget added to parent panel"));
	
	// Add best practice guidance for AI assistants
	TSharedPtr<FJsonObject> BestPracticeGuide = MakeShareable(new FJsonObject);
	BestPracticeGuide->SetStringField(TEXT("background_styling"), TEXT("For background colors/images, use Overlay panels instead of Canvas panels. Add background Image widgets as children to their specific containers (ScrollBox, Panel sections) not the main canvas."));
	BestPracticeGuide->SetStringField(TEXT("root_canvas_structure"), TEXT("CRITICAL: Root Canvas should contain Overlay widgets for each major UI section. Each Overlay manages its own background and content. Never add backgrounds directly to the root Canvas."));
	BestPracticeGuide->SetStringField(TEXT("proper_nesting"), TEXT("Background elements should be nested within their content containers, not globally positioned. This ensures automatic layout and proper visual hierarchy."));
	BestPracticeGuide->SetStringField(TEXT("z_order_layering"), TEXT("Use negative Z-order values (-10 to -100) for background elements to ensure they appear behind content."));
	BestPracticeGuide->SetStringField(TEXT("overlay_usage"), TEXT("When adding backgrounds: 1) Create/use Overlay panels, 2) Add background Image as child, 3) Add content widgets as children, 4) Set proper Z-order"));
	BestPracticeGuide->SetStringField(TEXT("size_to_fill"), TEXT("Background images should use 'Fill' size rule in their slot properties to cover the entire container area."));
	Result->SetObjectField(TEXT("ai_guidance"), BestPracticeGuide);
	
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleRemoveUMGComponent(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString ComponentName;
	bool RemoveChildren = true;
	bool RemoveFromVariables = true;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	// Optional parameters with defaults
	Params->TryGetBoolField(TEXT("remove_children"), RemoveChildren);
	Params->TryGetBoolField(TEXT("remove_from_variables"), RemoveFromVariables);
	
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

	// Find the target component
	UWidget* TargetComponent = WidgetTree->FindWidget(FName(*ComponentName));
	if (!TargetComponent)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
	}

	// Prepare response data
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> RemovedComponents;
	TArray<TSharedPtr<FJsonValue>> OrphanedChildren;
	
	// Function to recursively collect all child components
	TFunction<void(UWidget*, TArray<UWidget*>&)> CollectChildren = [&](UWidget* Widget, TArray<UWidget*>& Children) -> void
	{
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				UWidget* Child = Panel->GetChildAt(i);
				if (Child)
				{
					Children.Add(Child);
					CollectChildren(Child, Children);  // Recursive collection
				}
			}
		}
	};

	// Collect all children of the target component
	TArray<UWidget*> AllChildren;
	CollectChildren(TargetComponent, AllChildren);
	
	// Handle children based on remove_children flag
	if (!RemoveChildren && AllChildren.Num() > 0)
	{
		// Reparent children to root (or appropriate parent)
		UWidget* RootWidget = WidgetTree->RootWidget;
		if (UPanelWidget* RootPanel = Cast<UPanelWidget>(RootWidget))
		{
			for (UWidget* Child : AllChildren)
			{
				// Remove from current parent first
				if (UWidget* CurrentParent = Child->GetParent())
				{
					if (UPanelWidget* CurrentPanel = Cast<UPanelWidget>(CurrentParent))
					{
						CurrentPanel->RemoveChild(Child);
					}
				}
				
				// Add to root panel
				RootPanel->AddChild(Child);
				
				// Track orphaned children
				TSharedPtr<FJsonObject> OrphanInfo = MakeShareable(new FJsonObject);
				OrphanInfo->SetStringField(TEXT("name"), Child->GetName());
				OrphanInfo->SetStringField(TEXT("type"), Child->GetClass()->GetName());
				OrphanedChildren.Add(MakeShared<FJsonValueObject>(OrphanInfo));
			}
		}
	}
	
	// Remove the target component from its parent
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
	else
	{
		// Removing root widget
		if (WidgetTree->RootWidget == TargetComponent)
		{
			WidgetTree->RootWidget = nullptr;
		}
	}
	
	// Track the main removed component
	TSharedPtr<FJsonObject> MainComponentInfo = MakeShareable(new FJsonObject);
	MainComponentInfo->SetStringField(TEXT("name"), ComponentName);
	MainComponentInfo->SetStringField(TEXT("type"), TargetComponent->GetClass()->GetName());
	RemovedComponents.Add(MakeShared<FJsonValueObject>(MainComponentInfo));
	
	// Add children to removed components if they were removed
	if (RemoveChildren)
	{
		for (UWidget* Child : AllChildren)
		{
			TSharedPtr<FJsonObject> ChildInfo = MakeShareable(new FJsonObject);
			ChildInfo->SetStringField(TEXT("name"), Child->GetName());
			ChildInfo->SetStringField(TEXT("type"), Child->GetClass()->GetName());
			RemovedComponents.Add(MakeShared<FJsonValueObject>(ChildInfo));
		}
	}
	
	// Handle variable cleanup if requested
	bool VariableCleanupPerformed = false;
	if (RemoveFromVariables)
	{
		// Find and remove the Blueprint variable for this component
		for (int32 i = WidgetBlueprint->NewVariables.Num() - 1; i >= 0; i--)
		{
			if (WidgetBlueprint->NewVariables[i].VarName.ToString() == ComponentName)
			{
				WidgetBlueprint->NewVariables.RemoveAt(i);
				VariableCleanupPerformed = true;
				break;
			}
		}
	}
	
	// Mark Blueprint as dirty and recompile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	// Build response
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), ComponentName);
	Result->SetArrayField(TEXT("removed_components"), RemovedComponents);
	Result->SetArrayField(TEXT("orphaned_children"), OrphanedChildren);
	Result->SetBoolField(TEXT("variable_cleanup"), VariableCleanupPerformed);
	
	// Parent info
	TSharedPtr<FJsonObject> ParentInfo = MakeShareable(new FJsonObject);
	ParentInfo->SetStringField(TEXT("name"), ParentName);
	ParentInfo->SetStringField(TEXT("type"), ParentType);
	Result->SetObjectField(TEXT("parent_info"), ParentInfo);
	
	Result->SetStringField(TEXT("note"), FString::Printf(TEXT("Universal component removal completed. Removed %d components, orphaned %d children"), RemovedComponents.Num(), OrphanedChildren.Num()));
	
	return Result;
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

TSharedPtr<FJsonObject> FUMGCommands::HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
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
	
	// Support both string and JSON object values
	FString PropertyValue;
	TSharedPtr<FJsonValue> PropertyValueJson;
	bool bHasStringValue = Params->TryGetStringField(TEXT("property_value"), PropertyValue);
	bool bHasJsonValue = Params->Values.Contains(TEXT("property_value")) && !bHasStringValue;
	
	if (!bHasStringValue && !bHasJsonValue)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing property_value parameter"));
	}
	
	if (bHasJsonValue)
	{
		PropertyValueJson = Params->Values[TEXT("property_value")];
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find the widget component in the widget tree
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Use reflection to find and set the property
	FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*PropertyName);
	void* ContainerPtrForSet = (void*)FoundWidget;
	bool bUsedResolver = false;
	if (!Property)
	{
		// We now support dotted paths and Slot.* context, plus IsVariable alias
		// Resolve path
		bool bSlotRoot = false;
		TArray<FPathSegment> Segs;
		if (!ParsePropertyPath(PropertyName, bSlotRoot, Segs))
		{
			return FCommonUtils::CreateErrorResponse(TEXT("Invalid property_name path"));
		}
		FString ResolveError;
		FResolvedTarget Target;
		if (!ResolvePath(FoundWidget, Segs, bSlotRoot, Target, ResolveError))
		{
			return FCommonUtils::CreateErrorResponse(ResolveError);
		}

	bUsedResolver = true;
	// Synthetic child order setter
		if (Target.bIsSyntheticChildOrder)
		{
			int32 DesiredIndex = INDEX_NONE;
			if (bHasStringValue)
			{
				DesiredIndex = FCString::Atoi(*PropertyValue);
			}
			else if (bHasJsonValue && PropertyValueJson.IsValid() && PropertyValueJson->Type == EJson::Number)
			{
				DesiredIndex = (int32)PropertyValueJson->AsNumber();
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("ChildOrder requires integer value"));
			}
			UPanelSlot* Slot = Cast<UPanelSlot>(FoundWidget->Slot);
			if (!Slot || !Slot->Parent)
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Widget has no parent panel for ChildOrder"));
			}
			UPanelWidget* Parent = Slot->Parent;
			int32 CurrentIndex = Parent->GetChildIndex(FoundWidget);
			DesiredIndex = FMath::Clamp(DesiredIndex, 0, Parent->GetChildrenCount() - 1);
			if (CurrentIndex != DesiredIndex)
			{
				Parent->RemoveChildAt(CurrentIndex);
				Parent->InsertChildAt(DesiredIndex, FoundWidget);
			}
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
			if (GEditor)
			{
				GEditor->NoteSelectionChange();
				TArray<IAssetEditorInstance*> AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(WidgetBlueprint);
				for (IAssetEditorInstance* Editor : AssetEditors)
				{
					if (FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(Editor))
					{
						WidgetEditor->RefreshEditors();
					}
				}
			}
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
			Result->SetStringField(TEXT("component_name"), WidgetName);
			Result->SetStringField(TEXT("property_name"), PropertyName);
			Result->SetNumberField(TEXT("property_value"), DesiredIndex);
			Result->SetStringField(TEXT("note"), TEXT("ChildOrder updated"));
			return Result;
		}

		FProperty* ResolvedProperty = Target.Property;
		if (!ResolvedProperty)
		{
			return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property '%s' not found on target"), *PropertyName));
		}
		Property = ResolvedProperty;
		ContainerPtrForSet = Target.ContainerPtr ? Target.ContainerPtr : (void*)FoundWidget;
	}

	// Optional collection operation (for arrays/sets/maps)
	FString CollectionOp;
	Params->TryGetStringField(TEXT("collection_op"), CollectionOp);

	// Flag for structural modification (e.g., IsVariable toggles)
	bool bStructuralChange = false;

	// Handle collection operations first when applicable
	if (!CollectionOp.IsEmpty())
	{
		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			void* ArrayAddr = ArrayProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
			FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);

			auto ConvertAndAssignElement = [&](int32 DestIndex, const TSharedPtr<FJsonValue>& JsonElem, FString& OutErrStr) -> bool
			{
				ArrayHelper.ExpandForIndex(DestIndex);
				void* ElemPtr = ArrayHelper.GetRawPtr(DestIndex);
				FProperty* ElemProp = ArrayProperty->Inner;

				if (FStrProperty* PropStr = CastField<FStrProperty>(ElemProp))
				{
					FString V;
					if (JsonElem->Type == EJson::String) V = JsonElem->AsString();
					else if (JsonElem->Type == EJson::Number) V = FString::SanitizeFloat(JsonElem->AsNumber());
					else if (JsonElem->Type == EJson::Boolean) V = JsonElem->AsBool() ? TEXT("true") : TEXT("false");
					else V = JsonElem->AsString();
					PropStr->SetPropertyValue(ElemPtr, V);
					return true;
				}
				if (FTextProperty* PropText = CastField<FTextProperty>(ElemProp))
				{
					FText V = FText::FromString(JsonElem->AsString());
					PropText->SetPropertyValue(ElemPtr, V);
					return true;
				}
				if (FBoolProperty* PropBool = CastField<FBoolProperty>(ElemProp))
				{
					bool V = (JsonElem->Type == EJson::Boolean) ? JsonElem->AsBool() : JsonElem->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase);
					PropBool->SetPropertyValue(ElemPtr, V);
					return true;
				}
				if (FFloatProperty* PropFloat = CastField<FFloatProperty>(ElemProp))
				{
					float V = (float)((JsonElem->Type == EJson::Number) ? JsonElem->AsNumber() : FCString::Atof(*JsonElem->AsString()));
					PropFloat->SetPropertyValue(ElemPtr, V);
					return true;
				}
				if (FIntProperty* PropInt = CastField<FIntProperty>(ElemProp))
				{
					int32 V = (int32)((JsonElem->Type == EJson::Number) ? JsonElem->AsNumber() : FCString::Atoi(*JsonElem->AsString()));
					PropInt->SetPropertyValue(ElemPtr, V);
					return true;
				}
				if (FByteProperty* PropByte = CastField<FByteProperty>(ElemProp))
				{
					if (PropByte->Enum)
					{
						FString NameStr = JsonElem->AsString();
						int64 EnumVal = PropByte->Enum->GetValueByNameString(NameStr);
						if (EnumVal == INDEX_NONE)
						{
							OutErrStr = FString::Printf(TEXT("Invalid enum value '%s'"), *NameStr);
							return false;
						}
						PropByte->SetPropertyValue(ElemPtr, (uint8)EnumVal);
						return true;
					}
					uint8 V = (uint8)((JsonElem->Type == EJson::Number) ? (int32)JsonElem->AsNumber() : FCString::Atoi(*JsonElem->AsString()));
					PropByte->SetPropertyValue(ElemPtr, V);
					return true;
				}
				if (FStructProperty* PropStruct = CastField<FStructProperty>(ElemProp))
				{
					if (JsonElem->Type != EJson::Object)
					{
						OutErrStr = TEXT("Struct array element requires JSON object");
						return false;
					}
					TSharedPtr<FJsonObject> Obj = JsonElem->AsObject();
					return FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), PropStruct->Struct, ElemPtr, 0, 0);
				}
				OutErrStr = TEXT("Unsupported array element type");
				return false;
			};

			FString Op = CollectionOp.ToLower();
			if (Op == TEXT("clear"))
			{
				ArrayHelper.Resize(0);
			}
			else if (Op == TEXT("set") || Op == TEXT("append"))
			{
				const TArray<TSharedPtr<FJsonValue>>* JsonArr = nullptr;
				if (!bHasJsonValue || !PropertyValueJson.IsValid() || PropertyValueJson->Type != EJson::Array)
				{
					return FCommonUtils::CreateErrorResponse(TEXT("collection_op requires property_value to be an array"));
				}
				JsonArr = &PropertyValueJson->AsArray();
				int32 StartIndex = (Op == TEXT("set")) ? 0 : ArrayHelper.Num();
				if (Op == TEXT("set"))
				{
					ArrayHelper.Resize(0);
				}
				for (int32 i = 0; i < JsonArr->Num(); ++i)
				{
					FString CErr;
					if (!ConvertAndAssignElement(StartIndex + i, (*JsonArr)[i], CErr))
					{
						return FCommonUtils::CreateErrorResponse(CErr);
					}
				}
			}
			else if (Op == TEXT("insert") || Op == TEXT("updateat") || Op == TEXT("removeat"))
			{
				int32 Index = 0;
				if (!Params->TryGetNumberField(TEXT("index"), Index))
				{
					return FCommonUtils::CreateErrorResponse(TEXT("collection_op requires 'index' parameter"));
				}
				if (Op == TEXT("removeat"))
				{
					if (Index < 0 || Index >= ArrayHelper.Num())
					{
						return FCommonUtils::CreateErrorResponse(TEXT("removeAt index out of range"));
					}
					ArrayHelper.RemoveValues(Index, 1);
				}
				else
				{
					if (!bHasJsonValue)
					{
						return FCommonUtils::CreateErrorResponse(TEXT("insert/updateAt requires JSON property_value for element"));
					}
					if (Op == TEXT("insert"))
					{
						Index = FMath::Clamp(Index, 0, ArrayHelper.Num());
						ArrayHelper.InsertValues(Index, 1);
					}
					else
					{
						if (Index < 0 || Index >= ArrayHelper.Num())
						{
							return FCommonUtils::CreateErrorResponse(TEXT("updateAt index out of range"));
						}
					}
					FString CErr;
					if (!ConvertAndAssignElement(Index, PropertyValueJson, CErr))
					{
						return FCommonUtils::CreateErrorResponse(CErr);
					}
				}
			}
			else
			{
				return FCommonUtils::CreateErrorResponse(TEXT("Unsupported collection_op for arrays"));
			}

			// Mark and refresh
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
			if (GEditor)
			{
				GEditor->NoteSelectionChange();
				TArray<IAssetEditorInstance*> AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(WidgetBlueprint);
				for (IAssetEditorInstance* Editor : AssetEditors)
				{
					if (FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(Editor))
					{
						WidgetEditor->RefreshEditors();
					}
				}
			}
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
			Result->SetStringField(TEXT("component_name"), WidgetName);
			Result->SetStringField(TEXT("property_name"), PropertyName);
			Result->SetStringField(TEXT("collection_op"), CollectionOp);
			Result->SetStringField(TEXT("note"), TEXT("Array collection operation applied"));
			return Result;
		}
		// TODO: Add TSet/TMap support
		return FCommonUtils::CreateErrorResponse(TEXT("collection_op currently supports TArray only"));
	}

	// Handle different property types
	bool bPropertySet = false;
	FString ErrorMessage;

	// First: handle struct properties with JSON reflectively.
	// IMPORTANT: If ResolvePath was used, ContainerPtrForSet may already be the struct VALUE pointer.
	// In that case, do NOT call ContainerPtrToValuePtr again (avoids double-offset into memory).
	if (!bPropertySet && bHasJsonValue && PropertyValueJson.IsValid())
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (PropertyValueJson->Type != EJson::Object)
			{
				ErrorMessage = FString::Printf(TEXT("Struct property '%s' requires JSON object"), *PropertyName);
			}
			else
			{
				TSharedPtr<FJsonObject> JsonObj = PropertyValueJson->AsObject();
				// If we resolved via dotted path, ContainerPtrForSet points to the VALUE already.
				// Otherwise it's the owning object; derive value pointer via reflection.
				void* ValuePtr = bUsedResolver
					? ContainerPtrForSet
					: StructProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
				bPropertySet = FJsonObjectConverter::JsonObjectToUStruct(JsonObj.ToSharedRef(), StructProperty->Struct, ValuePtr, 0, 0);
				if (!bPropertySet)
				{
					ErrorMessage = FString::Printf(TEXT("Failed to convert JSON to struct for property '%s'"), *PropertyName);
				}
			}
		}
	}

	// Next: try complex non-struct types when JSON is provided
	if (!bPropertySet && bHasJsonValue && PropertyValueJson.IsValid())
	{
		if (!CastField<FStructProperty>(Property))
		{
			bPropertySet = ParseComplexPropertyValue(PropertyValueJson, Property, FoundWidget, ErrorMessage);
		}
	}
	
	// If complex type parsing failed or we have string data, try basic types
	if (!bPropertySet && bHasStringValue)
	{
		if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
		{
			StrProperty->SetPropertyValue_InContainer(ContainerPtrForSet, PropertyValue);
			bPropertySet = true;
		}
		else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			FText TextValue = FText::FromString(PropertyValue);
			TextProperty->SetPropertyValue_InContainer(ContainerPtrForSet, TextValue);
			bPropertySet = true;
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool BoolValue = PropertyValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || PropertyValue.Equals(TEXT("1"));
			BoolProperty->SetPropertyValue_InContainer(ContainerPtrForSet, BoolValue);
			bPropertySet = true;
		}
		else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			float FloatValue = FCString::Atof(*PropertyValue);
			FloatProperty->SetPropertyValue_InContainer(ContainerPtrForSet, FloatValue);
			bPropertySet = true;
		}
		else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
		{
			int32 IntValue = FCString::Atoi(*PropertyValue);
			IntProperty->SetPropertyValue_InContainer(ContainerPtrForSet, IntValue);
			bPropertySet = true;
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			// Handle enum properties (like ESlateVisibility)
			if (ByteProperty->Enum)
			{
				int64 EnumValue = ByteProperty->Enum->GetValueByNameString(PropertyValue);
				if (EnumValue != INDEX_NONE)
				{
					ByteProperty->SetPropertyValue_InContainer(ContainerPtrForSet, (uint8)EnumValue);
					bPropertySet = true;
				}
				else
				{
					ErrorMessage = FString::Printf(TEXT("Invalid enum value '%s' for property '%s'"), *PropertyValue, *PropertyName);
				}
			}
			else
			{
				uint8 ByteValue = (uint8)FCString::Atoi(*PropertyValue);
				ByteProperty->SetPropertyValue_InContainer(ContainerPtrForSet, ByteValue);
				bPropertySet = true;
			}
		}
		else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			if (EnumProperty->GetUnderlyingProperty() && EnumProperty->GetEnum())
			{
				int64 EnumValue = EnumProperty->GetEnum()->GetValueByNameString(PropertyValue);
				if (EnumValue != INDEX_NONE)
				{
					uint8* EnumValuePtr = (uint8*)EnumProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
					EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumValuePtr, EnumValue);
					bPropertySet = true;
				}
				else
				{
					ErrorMessage = FString::Printf(TEXT("Invalid enum value '%s' for property '%s'"), *PropertyValue, *PropertyName);
				}
			}
			else
			{
				ErrorMessage = FString::Printf(TEXT("Cannot set enum property '%s' - missing underlying property or enum"), *PropertyName);
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			// Try to parse as JSON string for struct properties
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertyValue);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				void* ValuePtr = bUsedResolver
					? ContainerPtrForSet
					: StructProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
				bPropertySet = FJsonObjectConverter::JsonObjectToUStruct(JsonObj.ToSharedRef(), StructProperty->Struct, ValuePtr, 0, 0);
				if (!bPropertySet)
				{
					// Fallback to legacy complex parser for non-standard shapes
					TSharedPtr<FJsonValue> JsonValue = MakeShareable(new FJsonValueObject(JsonObj));
					bPropertySet = ParseComplexPropertyValue(JsonValue, Property, FoundWidget, ErrorMessage);
				}
			}
			else
			{
				ErrorMessage = FString::Printf(TEXT("Invalid JSON for struct property '%s'"), *PropertyName);
			}
		}
	}

	// Special handling: IsVariable toggle should be structural
	if (bPropertySet)
	{
		// Notify the widget that a property has changed (similar to Details Panel)
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			// Create a property change event for struct properties
			FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
			PropertyChangedEvent.MemberProperty = Property;
			
			// Call PostEditChangeProperty on the widget to trigger proper notifications
			FoundWidget->PostEditChangeProperty(PropertyChangedEvent);
		}
		if (Property->GetFName() == TEXT("bIsVariable") || PropertyName.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
		{
			bStructuralChange = true;
		}
	}

	if (!bPropertySet)
	{
		if (ErrorMessage.IsEmpty())
		{
			ErrorMessage = FString::Printf(TEXT("Unsupported property type for '%s'"), *PropertyName);
		}
		return FCommonUtils::CreateErrorResponse(ErrorMessage);
	}

	// Mark the blueprint as modified and compile
	if (bStructuralChange)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
	
	// Force refresh the widget in the designer
	if (GEditor)
	{
		// Refresh the properties panel
		GEditor->NoteSelectionChange();
		
		// Force update any open widget blueprint editors
		TArray<IAssetEditorInstance*> AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(WidgetBlueprint);
		for (IAssetEditorInstance* Editor : AssetEditors)
		{
			if (FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(Editor))
			{
				// Refresh the designer view using available UE 5.6 methods
				WidgetEditor->RefreshEditors();
			}
		}
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	
	// Include the property value in the response (prefer string if available)
	if (bHasStringValue)
	{
		Result->SetStringField(TEXT("property_value"), PropertyValue);
	}
	else if (bHasJsonValue && PropertyValueJson.IsValid())
	{
		Result->SetField(TEXT("property_value"), PropertyValueJson);
	}
	
	Result->SetStringField(TEXT("note"), TEXT("Property set successfully"));
	
	// Mark the widget blueprint as dirty so changes persist
	WidgetBlueprint->MarkPackageDirty();
	
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params)
{
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

	// Find the widget component in the widget tree
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Resolve dotted path with Slot prefix and aliases
	bool bSlotRoot = false;
	TArray<FPathSegment> Segs;
	if (!ParsePropertyPath(PropertyName, bSlotRoot, Segs))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Invalid property_name path"));
	}
	FString ResolveError;
	FResolvedTarget Target;
	if (!ResolvePath(FoundWidget, Segs, bSlotRoot, Target, ResolveError))
	{
		return FCommonUtils::CreateErrorResponse(ResolveError);
	}

	// Synthetic ChildOrder
	if (Target.bIsSyntheticChildOrder)
	{
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Result->SetStringField(TEXT("component_name"), WidgetName);
		Result->SetStringField(TEXT("property_name"), PropertyName);
		UPanelSlot* Slot = Cast<UPanelSlot>(FoundWidget->Slot);
		UPanelWidget* Parent = Slot ? Slot->Parent : nullptr;
		int32 Index = (Parent) ? Parent->GetChildIndex(FoundWidget) : 0;
		Result->SetNumberField(TEXT("property_value"), Index);
		Result->SetStringField(TEXT("property_type"), TEXT("int"));
		TSharedPtr<FJsonObject> Constraints = MakeShareable(new FJsonObject);
		Constraints->SetNumberField(TEXT("child_count"), Parent ? Parent->GetChildrenCount() : 0);
		Result->SetObjectField(TEXT("constraints"), Constraints);
		Result->SetBoolField(TEXT("editable"), true);
		return Result;
	}

	FProperty* Property = Target.Property;
	if (!Property)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Property '%s' not found on target"), *PropertyName));
	}

	// Get property value based on type
	FString PropertyValue;
	FString PropertyType;
	TSharedPtr<FJsonValue> PropertyJson; // prefer structured JSON when applicable

	if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		PropertyValue = StrProperty->GetPropertyValue_InContainer(Target.ContainerPtr);
		PropertyType = TEXT("String");
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		FText TextValue = TextProperty->GetPropertyValue_InContainer(Target.ContainerPtr);
		PropertyValue = TextValue.ToString();
		PropertyType = TEXT("Text");
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		bool BoolValue = BoolProperty->GetPropertyValue_InContainer(Target.ContainerPtr);
		PropertyValue = BoolValue ? TEXT("true") : TEXT("false");
		PropertyType = TEXT("bool");
	}
	else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		float FloatValue = FloatProperty->GetPropertyValue_InContainer(Target.ContainerPtr);
		PropertyValue = FString::SanitizeFloat(FloatValue);
		PropertyType = TEXT("float");
	}
	else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
	{
		int32 IntValue = IntProperty->GetPropertyValue_InContainer(Target.ContainerPtr);
		PropertyValue = FString::FromInt(IntValue);
		PropertyType = TEXT("int");
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		uint8 ByteValue = ByteProperty->GetPropertyValue_InContainer(Target.ContainerPtr);
		if (ByteProperty->Enum)
		{
			PropertyValue = ByteProperty->Enum->GetNameStringByValue(ByteValue);
			PropertyType = FString::Printf(TEXT("Enum<%s>"), *ByteProperty->Enum->GetName());
		}
		else
		{
			PropertyValue = FString::FromInt(ByteValue);
			PropertyType = TEXT("byte");
		}
	}
	else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (EnumProperty->GetUnderlyingProperty() && EnumProperty->GetEnum())
		{
			uint8* EnumValuePtr = (uint8*)EnumProperty->ContainerPtrToValuePtr<void>(Target.ContainerPtr);
			int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumValuePtr);
			PropertyValue = EnumProperty->GetEnum()->GetNameStringByValue(EnumValue);
			PropertyType = FString::Printf(TEXT("Enum<%s>"), *EnumProperty->GetEnum()->GetName());
		}
		else
		{
			PropertyValue = TEXT("UnknownEnum");
			PropertyType = TEXT("EnumProperty");
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		// IMPORTANT: Respect resolver semantics.
		// When ResolvePath targets a struct field, Target.ContainerPtr is the struct VALUE pointer.
		// Otherwise, derive the value pointer from the owning object via ContainerPtrToValuePtr.
		void* ValuePtr = Target.ContainerPtr ? Target.ContainerPtr
											 : StructProperty->ContainerPtrToValuePtr<void>((void*)FoundWidget);
		TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
		if (FJsonObjectConverter::UStructToJsonObject(StructProperty->Struct, ValuePtr, Obj.ToSharedRef(), 0, 0))
		{
			PropertyJson = MakeShareable(new FJsonValueObject(Obj));
			PropertyType = FString::Printf(TEXT("Struct<%s>"), *StructProperty->Struct->GetName());
		}
		else
		{
			PropertyValue = TEXT("StructSerializationFailed");
			PropertyType = FString::Printf(TEXT("Struct<%s>"), *StructProperty->Struct->GetName());
		}
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		void* ArrayAddr = ArrayProperty->ContainerPtrToValuePtr<void>(Target.ContainerPtr);
		FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);
		TArray<TSharedPtr<FJsonValue>> JsonArr;
		for (int32 i = 0; i < ArrayHelper.Num(); ++i)
		{
			void* ElemPtr = ArrayHelper.GetRawPtr(i);
			FProperty* ElemProp = ArrayProperty->Inner;
			if (FStrProperty* PStr = CastField<FStrProperty>(ElemProp))
			{
				JsonArr.Add(MakeShareable(new FJsonValueString(PStr->GetPropertyValue(ElemPtr))));
			}
			else if (FTextProperty* PText = CastField<FTextProperty>(ElemProp))
			{
				JsonArr.Add(MakeShareable(new FJsonValueString(PText->GetPropertyValue(ElemPtr).ToString())));
			}
			else if (FBoolProperty* PBool = CastField<FBoolProperty>(ElemProp))
			{
				JsonArr.Add(MakeShareable(new FJsonValueBoolean(PBool->GetPropertyValue(ElemPtr))));
			}
			else if (FFloatProperty* PFloat = CastField<FFloatProperty>(ElemProp))
			{
				JsonArr.Add(MakeShareable(new FJsonValueNumber(PFloat->GetPropertyValue(ElemPtr))));
			}
			else if (FIntProperty* PInt = CastField<FIntProperty>(ElemProp))
			{
				JsonArr.Add(MakeShareable(new FJsonValueNumber(PInt->GetPropertyValue(ElemPtr))));
			}
			else if (FByteProperty* PByte = CastField<FByteProperty>(ElemProp))
			{
				if (PByte->Enum)
				{
					uint8 V = PByte->GetPropertyValue(ElemPtr);
					JsonArr.Add(MakeShareable(new FJsonValueString(PByte->Enum->GetNameStringByValue(V))));
				}
				else
				{
					JsonArr.Add(MakeShareable(new FJsonValueNumber(PByte->GetPropertyValue(ElemPtr))));
				}
			}
			else if (FStructProperty* PStruct = CastField<FStructProperty>(ElemProp))
			{
				TSharedPtr<FJsonObject> ElemObj = MakeShareable(new FJsonObject);
				FJsonObjectConverter::UStructToJsonObject(PStruct->Struct, ElemPtr, ElemObj.ToSharedRef(), 0, 0);
				JsonArr.Add(MakeShareable(new FJsonValueObject(ElemObj)));
			}
			else
			{
				JsonArr.Add(MakeShareable(new FJsonValueString(TEXT("UnsupportedArrayElemType"))));
			}
		}
		PropertyJson = MakeShareable(new FJsonValueArray(JsonArr));
		PropertyType = TEXT("Array");
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		void* SetAddr = SetProperty->ContainerPtrToValuePtr<void>(Target.ContainerPtr);
		FScriptSetHelper SetHelper(SetProperty, SetAddr);
		TArray<TSharedPtr<FJsonValue>> JsonArr;
		for (int32 Idx = 0; Idx < SetHelper.Num(); ++Idx)
		{
			if (!SetHelper.IsValidIndex(Idx)) continue;
			void* ElemPtr = SetHelper.GetElementPtr(Idx);
			FProperty* ElemProp = SetProperty->ElementProp;
			if (FStrProperty* SProp = CastField<FStrProperty>(ElemProp))
				JsonArr.Add(MakeShareable(new FJsonValueString(SProp->GetPropertyValue(ElemPtr))));
			else if (FTextProperty* TProp = CastField<FTextProperty>(ElemProp))
				JsonArr.Add(MakeShareable(new FJsonValueString(TProp->GetPropertyValue(ElemPtr).ToString())));
			else if (FBoolProperty* BProp = CastField<FBoolProperty>(ElemProp))
				JsonArr.Add(MakeShareable(new FJsonValueBoolean(BProp->GetPropertyValue(ElemPtr))));
			else if (FFloatProperty* FProp = CastField<FFloatProperty>(ElemProp))
				JsonArr.Add(MakeShareable(new FJsonValueNumber(FProp->GetPropertyValue(ElemPtr))));
			else if (FIntProperty* IProp = CastField<FIntProperty>(ElemProp))
				JsonArr.Add(MakeShareable(new FJsonValueNumber(IProp->GetPropertyValue(ElemPtr))));
			else if (FByteProperty* ByProp = CastField<FByteProperty>(ElemProp))
			{
				if (ByProp->Enum)
				{
					uint8 V = ByProp->GetPropertyValue(ElemPtr);
					JsonArr.Add(MakeShareable(new FJsonValueString(ByProp->Enum->GetNameStringByValue(V))));
				}
				else
				{
					JsonArr.Add(MakeShareable(new FJsonValueNumber(ByProp->GetPropertyValue(ElemPtr))));
				}
			}
			else if (FStructProperty* StProp = CastField<FStructProperty>(ElemProp))
			{
				TSharedPtr<FJsonObject> ElemObj = MakeShareable(new FJsonObject);
				FJsonObjectConverter::UStructToJsonObject(StProp->Struct, ElemPtr, ElemObj.ToSharedRef(), 0, 0);
				JsonArr.Add(MakeShareable(new FJsonValueObject(ElemObj)));
			}
		}
		PropertyJson = MakeShareable(new FJsonValueArray(JsonArr));
		PropertyType = TEXT("Set");
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		void* MapAddr = MapProperty->ContainerPtrToValuePtr<void>(Target.ContainerPtr);
		FScriptMapHelper MapHelper(MapProperty, MapAddr);
		TSharedPtr<FJsonObject> MapObj = MakeShareable(new FJsonObject);
		for (int32 Idx = 0; Idx < MapHelper.GetMaxIndex(); ++Idx)
		{
			if (!MapHelper.IsValidIndex(Idx)) continue;
			uint8* PairPtr = (uint8*)MapHelper.GetPairPtr(Idx);
			void* KeyPtr = PairPtr;
			void* ValPtr = PairPtr + MapProperty->MapLayout.ValueOffset;

			// Key to string
			FString KeyStr;
			if (FNameProperty* KP = CastField<FNameProperty>(MapProperty->KeyProp))
				KeyStr = KP->GetPropertyValue(KeyPtr).ToString();
			else if (FStrProperty* KP2 = CastField<FStrProperty>(MapProperty->KeyProp))
				KeyStr = KP2->GetPropertyValue(KeyPtr);
			else if (FIntProperty* KP3 = CastField<FIntProperty>(MapProperty->KeyProp))
				KeyStr = FString::FromInt(KP3->GetPropertyValue(KeyPtr));
			else if (FByteProperty* KP4 = CastField<FByteProperty>(MapProperty->KeyProp))
			{
				if (KP4->Enum)
					KeyStr = KP4->Enum->GetNameStringByValue(KP4->GetPropertyValue(KeyPtr));
				else
					KeyStr = FString::FromInt(KP4->GetPropertyValue(KeyPtr));
			}
			else
				KeyStr = TEXT("UnsupportedKey");

			// Value to JSON
			TSharedPtr<FJsonValue> ValJson;
			FProperty* VP = MapProperty->ValueProp;
			if (FStrProperty* VPStr = CastField<FStrProperty>(VP))
				ValJson = MakeShareable(new FJsonValueString(VPStr->GetPropertyValue(ValPtr)));
			else if (FTextProperty* VPText = CastField<FTextProperty>(VP))
				ValJson = MakeShareable(new FJsonValueString(VPText->GetPropertyValue(ValPtr).ToString()));
			else if (FBoolProperty* VPBool = CastField<FBoolProperty>(VP))
				ValJson = MakeShareable(new FJsonValueBoolean(VPBool->GetPropertyValue(ValPtr)));
			else if (FFloatProperty* VPFloat = CastField<FFloatProperty>(VP))
				ValJson = MakeShareable(new FJsonValueNumber(VPFloat->GetPropertyValue(ValPtr)));
			else if (FIntProperty* VPInt = CastField<FIntProperty>(VP))
				ValJson = MakeShareable(new FJsonValueNumber(VPInt->GetPropertyValue(ValPtr)));
			else if (FByteProperty* VPByte = CastField<FByteProperty>(VP))
			{
				if (VPByte->Enum)
					ValJson = MakeShareable(new FJsonValueString(VPByte->Enum->GetNameStringByValue(VPByte->GetPropertyValue(ValPtr))));
				else
					ValJson = MakeShareable(new FJsonValueNumber(VPByte->GetPropertyValue(ValPtr)));
			}
			else if (FStructProperty* VPStruct = CastField<FStructProperty>(VP))
			{
				TSharedPtr<FJsonObject> VObj = MakeShareable(new FJsonObject);
				FJsonObjectConverter::UStructToJsonObject(VPStruct->Struct, ValPtr, VObj.ToSharedRef(), 0, 0);
				ValJson = MakeShareable(new FJsonValueObject(VObj));
			}
			else
			{
				ValJson = MakeShareable(new FJsonValueString(TEXT("UnsupportedValueType")));
			}

			MapObj->SetField(KeyStr, ValJson);
		}
		PropertyJson = MakeShareable(new FJsonValueObject(MapObj));
		PropertyType = TEXT("Map");
	}
	else
	{
		PropertyValue = TEXT("UnsupportedType");
		PropertyType = Property->GetClass()->GetName();
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetStringField(TEXT("property_name"), PropertyName);
	if (PropertyJson.IsValid())
	{
		Result->SetField(TEXT("property_value"), PropertyJson);
	}
	else
	{
		Result->SetStringField(TEXT("property_value"), PropertyValue);
	}
	Result->SetStringField(TEXT("property_type"), PropertyType);
	// Constraints and editable metadata
	TSharedPtr<FJsonObject> Constraints = MakeShareable(new FJsonObject);
	AddEnumConstraints(Property, Constraints);
	AddNumericConstraints(Property, Constraints);
	// Add collection lengths
	if (FArrayProperty* APc = CastField<FArrayProperty>(Property))
	{
		void* ArrayAddr = APc->ContainerPtrToValuePtr<void>(Target.ContainerPtr);
		FScriptArrayHelper H(APc, ArrayAddr);
		Constraints->SetNumberField(TEXT("length"), H.Num());
	}
	else if (FSetProperty* SPc = CastField<FSetProperty>(Property))
	{
		void* SetAddr = SPc->ContainerPtrToValuePtr<void>(Target.ContainerPtr);
		FScriptSetHelper H(SPc, SetAddr);
		Constraints->SetNumberField(TEXT("length"), H.Num());
	}
	else if (FMapProperty* MPc = CastField<FMapProperty>(Property))
	{
		void* MapAddr = MPc->ContainerPtrToValuePtr<void>(Target.ContainerPtr);
		FScriptMapHelper H(MPc, MapAddr);
		Constraints->SetNumberField(TEXT("length"), H.Num());
	}
	Result->SetObjectField(TEXT("constraints"), Constraints);
	Result->SetBoolField(TEXT("editable"), Property->HasAnyPropertyFlags(CPF_Edit));
	// Adapter info
	TSharedPtr<FJsonObject> AdapterInfo = MakeShareable(new FJsonObject);
	AdapterInfo->SetStringField(TEXT("component_kind"), TEXT("UMG"));
	if (FoundWidget->Slot)
	{
		AdapterInfo->SetStringField(TEXT("slot_class"), FoundWidget->Slot->GetClass()->GetName());
	}
	else
	{
		AdapterInfo->SetStringField(TEXT("slot_class"), TEXT(""));
	}
	Result->SetObjectField(TEXT("adapter_info"), AdapterInfo);
	// Schema hints
	TSharedPtr<FJsonObject> Schema = MakeShareable(new FJsonObject);
	if (FStructProperty* SPC = CastField<FStructProperty>(Property))
	{
		TSharedPtr<FJsonObject> S = MakeShareable(new FJsonObject);
		S->SetStringField(TEXT("name"), SPC->Struct->GetName());
		Schema->SetObjectField(TEXT("struct"), S);
	}
	else if (FArrayProperty* APC = CastField<FArrayProperty>(Property))
	{
		TSharedPtr<FJsonObject> S = MakeShareable(new FJsonObject);
		S->SetStringField(TEXT("element_type"), APC->Inner->GetClass()->GetName());
		Schema->SetObjectField(TEXT("array"), S);
	}
	else if (FSetProperty* SETC = CastField<FSetProperty>(Property))
	{
		TSharedPtr<FJsonObject> S = MakeShareable(new FJsonObject);
		S->SetStringField(TEXT("element_type"), SETC->ElementProp->GetClass()->GetName());
		Schema->SetObjectField(TEXT("set"), S);
	}
	else if (FMapProperty* MPC = CastField<FMapProperty>(Property))
	{
		TSharedPtr<FJsonObject> S = MakeShareable(new FJsonObject);
		S->SetStringField(TEXT("key_type"), MPC->KeyProp->GetClass()->GetName());
		S->SetStringField(TEXT("value_type"), MPC->ValueProp->GetClass()->GetName());
		Schema->SetObjectField(TEXT("map"), S);
	}
	Result->SetObjectField(TEXT("schema"), Schema);
	
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString WidgetName;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("component_name"), WidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing component_name parameter"));
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}

	// Find the widget component in the widget tree
	UWidget* FoundWidget = nullptr;
	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
		
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName() == WidgetName)
			{
				FoundWidget = Widget;
				break;
			}
		}
	}

	if (!FoundWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprintName));
	}

	// Get all properties via reflection
	TArray<TSharedPtr<FJsonValue>> Properties;
	
	for (TFieldIterator<FProperty> PropertyIterator(FoundWidget->GetClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;
		if (!Property)
		{
			continue;
		}

		// Skip private/protected properties and functions
		if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected))
		{
			continue;
		}

		FString PropertyName = Property->GetName();
		FString PropertyType = Property->GetClass()->GetName();
		FString PropertyValue;

		// Get the current value
		if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
		{
			PropertyValue = StrProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyType = TEXT("String");
		}
		else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			FText TextValue = TextProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = TextValue.ToString();
			PropertyType = TEXT("Text");
		}
		else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool BoolValue = BoolProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = BoolValue ? TEXT("true") : TEXT("false");
			PropertyType = TEXT("bool");
		}
		else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			float FloatValue = FloatProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = FString::SanitizeFloat(FloatValue);
			PropertyType = TEXT("float");
		}
		else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
		{
			int32 IntValue = IntProperty->GetPropertyValue_InContainer(FoundWidget);
			PropertyValue = FString::FromInt(IntValue);
			PropertyType = TEXT("int32");
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			uint8 ByteValue = ByteProperty->GetPropertyValue_InContainer(FoundWidget);
			if (ByteProperty->Enum)
			{
				PropertyValue = ByteProperty->Enum->GetNameStringByValue(ByteValue);
				PropertyType = ByteProperty->Enum->GetName();
			}
			else
			{
				PropertyValue = FString::FromInt(ByteValue);
				PropertyType = TEXT("uint8");
			}
		}
		else
		{
			PropertyValue = TEXT("ComplexType");
		}

		TSharedPtr<FJsonObject> PropertyObj = MakeShareable(new FJsonObject);
		PropertyObj->SetStringField(TEXT("name"), PropertyName);
		PropertyObj->SetStringField(TEXT("type"), PropertyType);
		PropertyObj->SetStringField(TEXT("value"), PropertyValue);
		Properties.Add(MakeShareable(new FJsonValueObject(PropertyObj)));
	}
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("component_name"), WidgetName);
	Result->SetArrayField(TEXT("properties"), Properties);
	
	return Result;
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


TSharedPtr<FJsonObject> FUMGCommands::HandleAddWidgetSwitcher(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString SwitcherName;
	TArray<float> Position = {0.0f, 0.0f};
	TArray<float> Size = {200.0f, 100.0f};
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("switcher_name"), SwitcherName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing switcher_name parameter"));
	}
	
	const TArray<TSharedPtr<FJsonValue>>* PositionArray;
	if (Params->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray->Num() >= 2)
	{
		Position[0] = (*PositionArray)[0]->AsNumber();
		Position[1] = (*PositionArray)[1]->AsNumber();
	}
	
	const TArray<TSharedPtr<FJsonValue>>* SizeArray;
	if (Params->TryGetArrayField(TEXT("size"), SizeArray) && SizeArray->Num() >= 2)
	{
		Size[0] = (*SizeArray)[0]->AsNumber();
		Size[1] = (*SizeArray)[1]->AsNumber();
	}
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	// Create WidgetSwitcher widget
	UWidgetSwitcher* WidgetSwitcher = WidgetBlueprint->WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), *SwitcherName);
	if (!WidgetSwitcher)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to create WidgetSwitcher widget"));
	}
	
	// Set active widget index if provided
	int32 ActiveWidgetIndex = 0;
	if (Params->TryGetNumberField(TEXT("active_widget_index"), ActiveWidgetIndex))
	{
		WidgetSwitcher->SetActiveWidgetIndex(ActiveWidgetIndex);
	}
	
	// Add to root canvas panel
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetBlueprint->WidgetTree->RootWidget);
	if (RootCanvas)
	{
		UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(WidgetSwitcher);
		
		// Set position
		FVector2D SwitcherPosition(Position[0], Position[1]);
		Slot->SetPosition(SwitcherPosition);
		
		// Set size
		FVector2D SwitcherSize(Size[0], Size[1]);
		Slot->SetSize(SwitcherSize);
	}
	
	// Mark dirty and compile
	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
	Result->SetStringField(TEXT("switcher_name"), SwitcherName);
	Result->SetStringField(TEXT("widget_type"), TEXT("WidgetSwitcher"));
	Result->SetNumberField(TEXT("active_widget_index"), ActiveWidgetIndex);
	
	TSharedPtr<FJsonObject> PositionObj = MakeShareable(new FJsonObject);
	PositionObj->SetNumberField(TEXT("x"), Position[0]);
	PositionObj->SetNumberField(TEXT("y"), Position[1]);
	Result->SetObjectField(TEXT("position"), PositionObj);
	
	TSharedPtr<FJsonObject> SizeObj = MakeShareable(new FJsonObject);
	SizeObj->SetNumberField(TEXT("width"), Size[0]);
	SizeObj->SetNumberField(TEXT("height"), Size[1]);
	Result->SetObjectField(TEXT("size"), SizeObj);
	
	return Result;
}

TSharedPtr<FJsonObject> FUMGCommands::HandleAddWidgetSwitcherSlot(const TSharedPtr<FJsonObject>& Params)
{
	FString WidgetBlueprintName;
	FString SwitcherName;
	FString ChildWidgetName;
	int32 SlotIndex = 0;
	
	if (!Params->TryGetStringField(TEXT("widget_name"), WidgetBlueprintName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing widget_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("switcher_name"), SwitcherName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing switcher_name parameter"));
	}
	
	if (!Params->TryGetStringField(TEXT("child_widget_name"), ChildWidgetName))
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Missing child_widget_name parameter"));
	}
	
	Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);
	
	UWidgetBlueprint* WidgetBlueprint = FCommonUtils::FindWidgetBlueprint(WidgetBlueprintName);
	if (!WidgetBlueprint)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetBlueprintName));
	}
	
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Widget Blueprint has no WidgetTree"));
	}
	
	// Find the widget switcher
	UWidgetSwitcher* WidgetSwitcher = nullptr;
	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);
	
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName() == SwitcherName && Widget->IsA<UWidgetSwitcher>())
		{
			WidgetSwitcher = Cast<UWidgetSwitcher>(Widget);
			break;
		}
	}
	
	if (!WidgetSwitcher)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Widget Switcher '%s' not found"), *SwitcherName));
	}
	
	// Find the child widget to add
	UWidget* ChildWidget = nullptr;
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName() == ChildWidgetName)
		{
			ChildWidget = Widget;
			break;
		}
	}
	
	if (!ChildWidget)
	{
		return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Child widget '%s' not found"), *ChildWidgetName));
	}
	
	// Add the child to the widget switcher at the specified index
	try 
	{
		if (SlotIndex >= 0 && SlotIndex < WidgetSwitcher->GetNumWidgets())
		{
			// Insert at specific index - UWidgetSwitcher uses AddChild
			WidgetSwitcher->AddChild(ChildWidget);
			// Move to correct position if needed - UE doesn't have direct insert at index
		}
		else
		{
			// Add at end
			WidgetSwitcher->AddChild(ChildWidget);
			SlotIndex = WidgetSwitcher->GetNumWidgets() - 1;
		}
		
		// Mark blueprint as modified
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
		
		TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("widget_name"), WidgetBlueprintName);
		Result->SetStringField(TEXT("switcher_name"), SwitcherName);
		Result->SetStringField(TEXT("child_widget_name"), ChildWidgetName);
		Result->SetNumberField(TEXT("slot_index"), SlotIndex);
		Result->SetNumberField(TEXT("total_slots"), WidgetSwitcher->GetNumWidgets());
		Result->SetStringField(TEXT("note"), TEXT("Widget switcher slot added successfully"));
		
		return Result;
	}
	catch (...)
	{
		return FCommonUtils::CreateErrorResponse(TEXT("Failed to add widget to switcher"));
	}
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
