// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWidgetService.h"
#include "WidgetBlueprint.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/GridPanel.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Components/RichTextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/MultiLineEditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/InputKeySelector.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/BackgroundBlur.h"
#include "Components/SafeZone.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Components/InvalidationBox.h"
#include "Components/RetainerBox.h"
#include "Components/Throbber.h"
#include "Components/CircularThrobber.h"
#include "Components/ListView.h"
#include "Components/TreeView.h"
#include "Components/TileView.h"
#include "Components/ExpandableArea.h"
#include "Components/MenuAnchor.h"
#include "Components/NativeWidgetHost.h"
#include "Components/PanelSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Animation/MovieSceneMarginTrack.h"
#include "Animation/MovieSceneMarginSection.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Channels/MovieSceneChannelTraits.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Fonts/SlateFontInfo.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "PlayInEditorDataTypes.h"
#include "Sections/MovieSceneColorSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/SlateBrush.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "UObject/PropertyIterator.h"
#include "UObject/UnrealType.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "MVVMPropertyPath.h"
#include "MVVMViewModelBase.h"
#include "Types/MVVMFieldVariant.h"
#include "ViewModel/MVVMViewModelBlueprint.h"
#include "WidgetBlueprintExtension.h"

// Static list of available widget types
static const TArray<FString> GAvailableWidgetTypes = {
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
	TEXT("WidgetSwitcher"),
	TEXT("ComboBoxString"),
	TEXT("SpinBox"),
	TEXT("MultiLineEditableText"),
	TEXT("MultiLineEditableTextBox"),
	TEXT("InputKeySelector"),
	TEXT("Border"),
	TEXT("SizeBox"),
	TEXT("ScaleBox"),
	TEXT("BackgroundBlur"),
	TEXT("SafeZone"),
	TEXT("UniformGridPanel"),
	TEXT("WrapBox"),
	TEXT("InvalidationBox"),
	TEXT("RetainerBox"),
	TEXT("Throbber"),
	TEXT("CircularThrobber"),
	TEXT("ListView"),
	TEXT("TreeView"),
	TEXT("TileView"),
	TEXT("ExpandableArea"),
	TEXT("MenuAnchor"),
	TEXT("NativeWidgetHost")
};

namespace
{
	enum class EWidgetAnimationTrackType : uint8
	{
		Float,
		Color,
		Margin
	};

	struct FResolvedWidgetProperty
	{
		UObject* TargetObject = nullptr;
		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ResolvedPath;
		// The property name with any "Slot." prefix stripped — what value aliases key off.
		FString AliasName;
		// True when the name also matched the slot but the widget's own property won (issue #553).
		bool bAmbiguous = false;
	};

	struct FWidgetAnimationPropertyTarget
	{
		UObject* AnimatedObject = nullptr;
		EWidgetAnimationTrackType TrackType = EWidgetAnimationTrackType::Float;
		FName PropertyName;
		FString PropertyPath;
		int32 MarginChannelIndex = INDEX_NONE;
	};

	TMap<FString, TWeakObjectPtr<UUserWidget>> GPIEWidgetInstances;

	FProperty* FindPropertyCaseInsensitive(UStruct* StructType, const FString& PropertyName)
	{
		if (!StructType)
		{
			return nullptr;
		}

		if (FProperty* Property = StructType->FindPropertyByName(FName(*PropertyName)))
		{
			return Property;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}

		return nullptr;
	}

	bool ResolvePropertyPath(UStruct* StructType, void* ContainerPtr, const FString& PropertyPath, FProperty*& OutProperty, void*& OutValuePtr)
	{
		OutProperty = nullptr;
		OutValuePtr = nullptr;

		if (!StructType || !ContainerPtr || PropertyPath.IsEmpty())
		{
			return false;
		}

		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.Num() == 0)
		{
			return false;
		}

		UStruct* CurrentStruct = StructType;
		void* CurrentContainer = ContainerPtr;

		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			FProperty* Property = FindPropertyCaseInsensitive(CurrentStruct, Segments[Index]);
			if (!Property)
			{
				return false;
			}

			void* NextValuePtr = Property->ContainerPtrToValuePtr<void>(CurrentContainer);
			if (Index == Segments.Num() - 1)
			{
				OutProperty = Property;
				OutValuePtr = NextValuePtr;
				return true;
			}

			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CurrentStruct = StructProperty->Struct;
				CurrentContainer = NextValuePtr;
				continue;
			}

			if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				UObject* NextObject = ObjectProperty->GetObjectPropertyValue(NextValuePtr);
				if (!NextObject)
				{
					return false;
				}

				CurrentStruct = NextObject->GetClass();
				CurrentContainer = NextObject;
				continue;
			}

			return false;
		}

		return false;
	}

	bool ResolveSpecialWidgetProperty(UWidget* Widget, const FString& PropertyName, UObject*& OutTargetObject, FString& OutResolvedPath)
	{
		if (!Widget)
		{
			return false;
		}

		if (PropertyName.Equals(TEXT("Position X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Left");
			return true;
		}

		if (PropertyName.Equals(TEXT("Position Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Top");
			return true;
		}

		if (PropertyName.Equals(TEXT("Size X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Right");
			return true;
		}

		if (PropertyName.Equals(TEXT("Size Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Offsets.Bottom");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Min X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Minimum.X");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Min Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Minimum.Y");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Max X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Maximum.X");
			return true;
		}

		if (PropertyName.Equals(TEXT("Anchor Max Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Anchors.Maximum.Y");
			return true;
		}

		// Canvas pivot alignment. With anchors at a point (e.g. 0.5,0.5), Alignment 0.5,0.5
		// centers the widget on that point (top-left otherwise).
		if (PropertyName.Equals(TEXT("Alignment X"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Alignment.X");
			return true;
		}

		if (PropertyName.Equals(TEXT("Alignment Y"), ESearchCase::IgnoreCase) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("LayoutData.Alignment.Y");
			return true;
		}

		// Canvas Z-order (#424). UCanvasPanelSlot::ZOrder is a direct int32 UPROPERTY.
		if ((PropertyName.Equals(TEXT("ZOrder"), ESearchCase::IgnoreCase) ||
			 PropertyName.Equals(TEXT("Z Order"), ESearchCase::IgnoreCase)) && Widget->Slot)
		{
			OutTargetObject = Widget->Slot;
			OutResolvedPath = TEXT("ZOrder");
			return true;
		}

		// Box / Overlay slot alignment + padding + size (#423). These are UPROPERTYs shared by
		// UVerticalBoxSlot / UHorizontalBoxSlot / UOverlaySlot (alignment/padding) and the box slots
		// (Size). Routing by property name keeps this slot-type agnostic.
		if (Widget->Slot)
		{
			if (PropertyName.Equals(TEXT("Horizontal Alignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("HorizontalAlignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("H Align"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("HAlign"), ESearchCase::IgnoreCase))
			{
				OutTargetObject = Widget->Slot;
				OutResolvedPath = TEXT("HorizontalAlignment");
				return true;
			}
			if (PropertyName.Equals(TEXT("Vertical Alignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("VerticalAlignment"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("V Align"), ESearchCase::IgnoreCase) ||
				PropertyName.Equals(TEXT("VAlign"), ESearchCase::IgnoreCase))
			{
				OutTargetObject = Widget->Slot;
				OutResolvedPath = TEXT("VerticalAlignment");
				return true;
			}
			if (PropertyName.Equals(TEXT("Padding"), ESearchCase::IgnoreCase))
			{
				OutTargetObject = Widget->Slot;
				OutResolvedPath = TEXT("Padding");
				return true;
			}
			if (PropertyName.Equals(TEXT("Padding Left"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Left"); return true; }
			if (PropertyName.Equals(TEXT("Padding Top"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Top"); return true; }
			if (PropertyName.Equals(TEXT("Padding Right"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Right"); return true; }
			if (PropertyName.Equals(TEXT("Padding Bottom"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Padding.Bottom"); return true; }
			if (PropertyName.Equals(TEXT("Size Rule"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Size.SizeRule"); return true; }
			if (PropertyName.Equals(TEXT("Size Value"), ESearchCase::IgnoreCase)) { OutTargetObject = Widget->Slot; OutResolvedPath = TEXT("Size.Value"); return true; }
		}

		return false;
	}

	// When a property name fails to resolve, suggest the alias that actually works. Whole-struct
	// guesses like "Anchors"/"Alignment"/"Size" are the common false-negative that makes an agent
	// wrongly conclude "no such capability exists" — point it at the scalar slot aliases instead.
	// Returns an empty string when there's no specific suggestion.
	FString BuildSlotPropertyHint(UWidget* Widget, const FString& PropertyName)
	{
		const FString Name = PropertyName.TrimStartAndEnd();
		const bool bCanvas = Widget && Widget->Slot && Widget->Slot->IsA<UCanvasPanelSlot>();
		const bool bBoxOrOverlay = Widget && Widget->Slot &&
			(Widget->Slot->IsA<UVerticalBoxSlot>() || Widget->Slot->IsA<UHorizontalBoxSlot>() || Widget->Slot->IsA<UOverlaySlot>());

		auto Eq = [&Name](const TCHAR* S) { return Name.Equals(S, ESearchCase::IgnoreCase); };

		if (Eq(TEXT("Anchors")) || Eq(TEXT("Anchor")))
		{
			return bCanvas || !Widget || !Widget->Slot
				? TEXT("'Anchor Min X', 'Anchor Min Y', 'Anchor Max X', 'Anchor Max Y' (Canvas slot scalars)")
				: TEXT("anchors apply only to Canvas Panel children");
		}
		if (Eq(TEXT("Alignment")))
		{
			if (bCanvas) return TEXT("'Alignment X' and 'Alignment Y' (Canvas pivot)");
			if (bBoxOrOverlay) return TEXT("'Horizontal Alignment' / 'Vertical Alignment' (accepts Fill/Left/Center/Right/Top/Bottom)");
			return TEXT("'Alignment X/Y' (Canvas) or 'Horizontal/Vertical Alignment' (Box/Overlay)");
		}
		if (Eq(TEXT("Position")) || Eq(TEXT("Offset")) || Eq(TEXT("Offsets")))
		{
			return TEXT("'Position X', 'Position Y', 'Size X', 'Size Y' (Canvas slot)");
		}
		if (Eq(TEXT("Size")))
		{
			if (bBoxOrOverlay) return TEXT("'Size Rule' and 'Size Value' (Box slot)");
			return TEXT("'Size X' and 'Size Y' (Canvas slot)");
		}
		if (Eq(TEXT("Margin")))
		{
			return TEXT("'Padding' (or 'Padding Left/Top/Right/Bottom')");
		}
		if (Eq(TEXT("Color")) || Eq(TEXT("Colour")) || Eq(TEXT("Tint")))
		{
			return TEXT("text: 'ColorAndOpacity', image: 'ColorAndOpacity', button: 'Background Color' — value as UE struct text '(R=..,G=..,B=..,A=..)'");
		}
		return FString();
	}

	// Compose the "not found" warning with an optional did-you-mean hint and a generic pointer.
	FString BuildPropertyNotFoundMessage(const TCHAR* Func, UWidget* Widget, const FString& ComponentName, const FString& PropertyName)
	{
		FString Msg = FString::Printf(TEXT("UWidgetService::%s: Property '%s' not found on widget '%s'."), Func, *PropertyName, *ComponentName);
		const FString Hint = BuildSlotPropertyHint(Widget, PropertyName);
		if (!Hint.IsEmpty())
		{
			Msg += FString::Printf(TEXT(" Did you mean %s?"), *Hint);
		}
		Msg += TEXT(" Slot layout is set via scalar aliases or an explicit 'Slot.<Prop>' path (see the umg-widgets skill); use list_properties for component property names.");
		return Msg;
	}

	// Normalize a friendly slot-alias value into the form ImportText expects.
	// Alignment aliases accept "Fill"/"Left"/"Center"/"Right"/"Top"/"Bottom" (or the full enum name);
	// size-rule accepts "Fill"/"Automatic". Returns the value unchanged for non-alias properties.
	FString NormalizeSlotAliasValue(const FString& PropertyName, const FString& Value)
	{
		const bool bIsHAlign = PropertyName.Equals(TEXT("Horizontal Alignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("HorizontalAlignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("H Align"), ESearchCase::IgnoreCase) || PropertyName.Equals(TEXT("HAlign"), ESearchCase::IgnoreCase);
		const bool bIsVAlign = PropertyName.Equals(TEXT("Vertical Alignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("VerticalAlignment"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("V Align"), ESearchCase::IgnoreCase) || PropertyName.Equals(TEXT("VAlign"), ESearchCase::IgnoreCase);

		FString V = Value;
		V.TrimStartAndEndInline();

		// Full "Padding" with a single number -> apply to all four sides (FMargin's ImportText needs
		// the struct form, so expand "8" to "(Left=8,Top=8,Right=8,Bottom=8)").
		if (PropertyName.Equals(TEXT("Padding"), ESearchCase::IgnoreCase) &&
			!V.Contains(TEXT("=")) && !V.Contains(TEXT(",")))
		{
			float Pad = 0.0f;
			if (LexTryParseString(Pad, *V))
			{
				return FString::Printf(TEXT("(Left=%f,Top=%f,Right=%f,Bottom=%f)"), Pad, Pad, Pad, Pad);
			}
		}

		if (bIsHAlign)
		{
			if (V.StartsWith(TEXT("HAlign_"), ESearchCase::IgnoreCase)) return V;
			if (V.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Fill");
			if (V.Equals(TEXT("Left"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Left");
			if (V.Equals(TEXT("Center"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Centre"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Center");
			if (V.Equals(TEXT("Right"), ESearchCase::IgnoreCase)) return TEXT("HAlign_Right");
		}
		else if (bIsVAlign)
		{
			if (V.StartsWith(TEXT("VAlign_"), ESearchCase::IgnoreCase)) return V;
			if (V.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Fill");
			if (V.Equals(TEXT("Top"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Top");
			if (V.Equals(TEXT("Center"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Centre"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Center");
			if (V.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase)) return TEXT("VAlign_Bottom");
		}

		return Value;
	}

	bool ResolveWidgetProperty(UWidget* Widget, const FString& PropertyName, FResolvedWidgetProperty& OutResolved)
	{
		OutResolved = {};
		if (!Widget)
		{
			return false;
		}
		OutResolved.AliasName = PropertyName;

		// Explicit slot targeting: "Slot.<Prop>" always addresses the widget's slot (issue #553).
		if (PropertyName.StartsWith(TEXT("Slot."), ESearchCase::IgnoreCase))
		{
			if (!Widget->Slot)
			{
				return false;
			}
			OutResolved.AliasName = PropertyName.RightChop(5);
			OutResolved.TargetObject = Widget->Slot;
			OutResolved.ResolvedPath = OutResolved.AliasName;
			// Keep the friendly aliases working behind the prefix ("Slot.Padding Left" etc.).
			UObject* AliasTarget = Widget->Slot;
			FString AliasPath = OutResolved.AliasName;
			if (ResolveSpecialWidgetProperty(Widget, OutResolved.AliasName, AliasTarget, AliasPath) && AliasTarget == Widget->Slot)
			{
				OutResolved.ResolvedPath = AliasPath;
			}
			return ResolvePropertyPath(OutResolved.TargetObject->GetClass(), OutResolved.TargetObject, OutResolved.ResolvedPath, OutResolved.Property, OutResolved.ValuePtr);
		}

		// The widget's own property wins over a same-named slot property — the old slot-first order
		// silently wrote the SLOT and left the widget's property untouched (issue #553).
		if (ResolvePropertyPath(Widget->GetClass(), Widget, PropertyName, OutResolved.Property, OutResolved.ValuePtr))
		{
			OutResolved.TargetObject = Widget;
			OutResolved.ResolvedPath = PropertyName;
			UObject* AliasTarget = Widget;
			FString AliasPath = PropertyName;
			if (ResolveSpecialWidgetProperty(Widget, PropertyName, AliasTarget, AliasPath) && AliasTarget != Widget)
			{
				OutResolved.bAmbiguous = true;
			}
			return true;
		}

		// Fall back to the slot aliases (Canvas scalars, box alignment/padding, ZOrder, ...).
		OutResolved.TargetObject = Widget;
		OutResolved.ResolvedPath = PropertyName;
		if (ResolveSpecialWidgetProperty(Widget, PropertyName, OutResolved.TargetObject, OutResolved.ResolvedPath))
		{
			if (ResolvePropertyPath(OutResolved.TargetObject->GetClass(), OutResolved.TargetObject, OutResolved.ResolvedPath, OutResolved.Property, OutResolved.ValuePtr))
			{
				return true;
			}
		}

		return false;
	}

	void MarkWidgetBlueprintModified(UWidgetBlueprint* WidgetBP, bool bStructural = false)
	{
		if (!WidgetBP)
		{
			return;
		}

		WidgetBP->Modify();
		if (bStructural)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		}
		else
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
		}
	}

	UUserWidget* CreateWidgetInstanceForBlueprint(UWidgetBlueprint* WidgetBP)
	{
		if (!WidgetBP)
		{
			return nullptr;
		}

		if (!WidgetBP->GeneratedClass || WidgetBP->Status == BS_Dirty)
		{
			FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		}

		if (!WidgetBP->GeneratedClass || !WidgetBP->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
		{
			return nullptr;
		}

		UWorld* World = nullptr;
		if (GEditor)
		{
			if (GEditor->PlayWorld)
			{
				World = GEditor->PlayWorld.Get();
			}
			else
			{
				World = GEditor->GetEditorWorldContext().World();
			}
		}

		if (!World)
		{
			return nullptr;
		}

		UClass* WidgetClass = WidgetBP->GeneratedClass;
		if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
		{
			return nullptr;
		}

		const FName WidgetName = MakeUniqueObjectName(World, WidgetClass, FName(*FString::Printf(TEXT("%s_Instance"), *WidgetBP->GetName())));
		return CreateWidget<UUserWidget>(World, WidgetClass, WidgetName);
	}

	UWidget* FindRuntimeWidgetByName(UUserWidget* WidgetInstance, const FString& ComponentName)
	{
		if (!WidgetInstance || !WidgetInstance->WidgetTree)
		{
			return nullptr;
		}

		if (ComponentName.IsEmpty())
		{
			return WidgetInstance->WidgetTree->RootWidget;
		}

		TArray<UWidget*> AllWidgets;
		WidgetInstance->WidgetTree->GetAllWidgets(AllWidgets);
		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && Widget->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				return Widget;
			}
		}

		return nullptr;
	}

	template<typename StructType>
	bool ImportStructFromText(const FString& Text, StructType& OutValue)
	{
		const UScriptStruct* ScriptStruct = TBaseStructure<StructType>::Get();
		return ScriptStruct && ScriptStruct->ImportText(*Text, &OutValue, nullptr, PPF_None, GLog, ScriptStruct->GetName()) != nullptr;
	}

	bool ParseLinearColor(const FString& Text, FLinearColor& OutColor)
	{
		return ImportStructFromText(Text, OutColor);
	}

	bool ParseVector2D(const FString& Text, FVector2D& OutVector)
	{
		return ImportStructFromText(Text, OutVector);
	}

	bool ParseMargin(const FString& Text, FMargin& OutMargin)
	{
		return ImportStructFromText(Text, OutMargin);
	}

	bool ParseCornerRadius(const FString& Text, FVector4& OutCornerRadius)
	{
		OutCornerRadius = FVector4::Zero();

		FString Trimmed = Text;
		Trimmed.TrimStartAndEndInline();
		Trimmed.RemoveFromStart(TEXT("("));
		Trimmed.RemoveFromEnd(TEXT(")"));

		TArray<FString> Parts;
		Trimmed.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			FString Key;
			FString Value;
			if (!Part.Split(TEXT("="), &Key, &Value))
			{
				continue;
			}

			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();

			float ParsedValue = 0.0f;
			if (!LexTryParseString(ParsedValue, *Value))
			{
				continue;
			}

			if (Key.Equals(TEXT("TopLeft"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.X = ParsedValue;
			}
			else if (Key.Equals(TEXT("TopRight"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.Y = ParsedValue;
			}
			else if (Key.Equals(TEXT("BottomRight"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.Z = ParsedValue;
			}
			else if (Key.Equals(TEXT("BottomLeft"), ESearchCase::IgnoreCase))
			{
				OutCornerRadius.W = ParsedValue;
			}
		}

		return true;
	}

	ESlateBrushDrawType::Type StringToBrushDrawType(const FString& DrawAs)
	{
		if (DrawAs.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::Box;
		}
		if (DrawAs.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::Border;
		}
		if (DrawAs.Equals(TEXT("RoundedBox"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::RoundedBox;
		}
		if (DrawAs.Equals(TEXT("NoDrawType"), ESearchCase::IgnoreCase) || DrawAs.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return ESlateBrushDrawType::NoDrawType;
		}
		return ESlateBrushDrawType::Image;
	}

	FString BrushDrawTypeToString(ESlateBrushDrawType::Type DrawType)
	{
		switch (DrawType)
		{
		case ESlateBrushDrawType::Box:
			return TEXT("Box");
		case ESlateBrushDrawType::Border:
			return TEXT("Border");
		case ESlateBrushDrawType::RoundedBox:
			return TEXT("RoundedBox");
		case ESlateBrushDrawType::NoDrawType:
			return TEXT("NoDrawType");
		default:
			return TEXT("Image");
		}
	}

	EMovieSceneKeyInterpolation StringToKeyInterpolation(const FString& Interpolation)
	{
		if (Interpolation.Equals(TEXT("Constant"), ESearchCase::IgnoreCase))
		{
			return EMovieSceneKeyInterpolation::Constant;
		}
		if (Interpolation.Equals(TEXT("Cubic"), ESearchCase::IgnoreCase))
		{
			return EMovieSceneKeyInterpolation::Auto;
		}
		return EMovieSceneKeyInterpolation::Linear;
	}

	bool TryGetPropertyText(UObject* Object, const FString& PropertyPath, FString& OutText)
	{
		if (!Object)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		if (!ResolvePropertyPath(Object->GetClass(), Object, PropertyPath, Property, ValuePtr))
		{
			return false;
		}

		// ExportTextItem_Direct APPENDS to OutText; reset first so callers passing a
		// struct field that has a default-value initializer (e.g. FWidgetFontInfo.ShadowOffset)
		// get the value alone, not "<default><exported>" concatenated. (issue #470)
		OutText.Reset();
		Property->ExportTextItem_Direct(OutText, ValuePtr, nullptr, Object, PPF_None);
		return true;
	}

	bool TrySetPropertyText(UObject* Object, const FString& PropertyPath, const FString& PropertyValue)
	{
		if (!Object)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		if (!ResolvePropertyPath(Object->GetClass(), Object, PropertyPath, Property, ValuePtr))
		{
			return false;
		}

		return Property->ImportText_Direct(*PropertyValue, ValuePtr, Object, PPF_None) != nullptr;
	}

	// A text widget's ColorAndOpacity is an FSlateColor: a bare "(R=..,G=..,B=..,A=..)" fed to
	// ImportText_Direct matches no member and is a lenient no-op that still reports success — so the
	// colour silently never lands. Parse the linear tuple and assign FSlateColor(Linear) reflectively
	// (verified by readback); if the text is instead a fully-qualified FSlateColor tuple
	// ("(SpecifiedColor=(...),ColorUseRule=UseColor_Specified)") fall back to a strict text import.
	// Returns false when a non-empty colour did not land. (VibeUE A4)
	bool TrySetSlateColorProperty(UObject* Object, const FString& PropertyName, const FString& ColorText)
	{
		if (!Object || ColorText.IsEmpty())
		{
			return false;
		}

		FStructProperty* StructProp = FindFProperty<FStructProperty>(Object->GetClass(), *PropertyName);
		if (!StructProp || StructProp->Struct != FSlateColor::StaticStruct())
		{
			return false;
		}

		FSlateColor* ColorPtr = StructProp->ContainerPtrToValuePtr<FSlateColor>(Object);
		if (!ColorPtr)
		{
			return false;
		}

		FLinearColor Linear;
		if (ParseLinearColor(ColorText, Linear))
		{
			*ColorPtr = FSlateColor(Linear);
			// Verify by readback rather than by return value.
			return ColorPtr->GetSpecifiedColor().Equals(Linear);
		}

		// Fully-qualified FSlateColor tuple: let the struct's own importer handle it.
		return StructProp->ImportText_Direct(*ColorText, ColorPtr, Object, PPF_None) != nullptr;
	}

	// UMG "optional override" values (USizeBox::WidthOverride/HeightOverride/MinDesiredWidth/…,
	// UImage aspect-ratio overrides, etc.) are inert unless their companion bOverride_<Name> bool
	// is also set — that bool is the "checkbox" the UMG designer ticks for you. Writing only the
	// value field silently no-ops: the layout engine keeps ignoring it (e.g. a SizeBox width that
	// never clamps, so the box fills its parent). Whenever we set such a value directly on an
	// object, raise the matching bOverride_ companion so the intent actually applies. (issue #508)
	void RaiseOverrideCompanionIfPresent(UObject* Object, const FProperty* ValueProperty)
	{
		if (!Object || !ValueProperty)
		{
			return;
		}

		// Only direct object members carry a bOverride_ companion; struct-nested fields
		// (e.g. WidgetStyle.Normal.TintColor) don't, and GetOwnerClass() is null for those.
		UClass* OwnerClass = ValueProperty->GetOwnerClass();
		if (!OwnerClass || !Object->GetClass()->IsChildOf(OwnerClass))
		{
			return;
		}

		const FString CompanionName = FString(TEXT("bOverride_")) + ValueProperty->GetName();
		FBoolProperty* OverrideFlag = FindFProperty<FBoolProperty>(Object->GetClass(), *CompanionName);
		if (!OverrideFlag)
		{
			return;
		}

		void* FlagPtr = OverrideFlag->ContainerPtrToValuePtr<void>(Object);
		if (!OverrideFlag->GetPropertyValue(FlagPtr))
		{
			OverrideFlag->SetPropertyValue(FlagPtr, true);
		}
	}

	FString GetLastPathSegment(const FString& PropertyPath)
	{
		FString Left;
		FString Right;
		if (PropertyPath.Split(TEXT("."), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			return Right;
		}
		return PropertyPath;
	}

	bool IsSlateFontProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty && StructProperty->Struct == TBaseStructure<FSlateFontInfo>::Get();
	}

	bool IsSlateBrushProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty && StructProperty->Struct == TBaseStructure<FSlateBrush>::Get();
	}

	bool ResolveFontProperty(UWidget* Widget, const FString& RequestedPropertyName, FResolvedWidgetProperty& OutResolved)
	{
		TArray<FString> CandidatePaths;
		if (!RequestedPropertyName.IsEmpty() && !RequestedPropertyName.Equals(TEXT("Font"), ESearchCase::IgnoreCase))
		{
			CandidatePaths.Add(RequestedPropertyName);
		}
		else
		{
			CandidatePaths = {
				TEXT("Font"),
				TEXT("WidgetStyle.Font"),
				TEXT("WidgetStyle.TextStyle.Font"),
				TEXT("TextStyle.Font")
			};
		}

		for (const FString& CandidatePath : CandidatePaths)
		{
			if (ResolveWidgetProperty(Widget, CandidatePath, OutResolved) && IsSlateFontProperty(OutResolved.Property))
			{
				return true;
			}
		}

		return false;
	}

	bool ResolveBrushProperty(UWidget* Widget, const FString& SlotName, FResolvedWidgetProperty& OutResolved)
	{
		TArray<FString> CandidatePaths;

		if (Cast<UImage>(Widget))
		{
			CandidatePaths = { TEXT("Brush") };
		}
		else if (Cast<UButton>(Widget))
		{
			if (SlotName.Equals(TEXT("Normal"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Normal") };
			else if (SlotName.Equals(TEXT("Hovered"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Hovered") };
			else if (SlotName.Equals(TEXT("Pressed"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Pressed") };
			else if (SlotName.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.Disabled") };
		}
		else if (Cast<UBorder>(Widget))
		{
			CandidatePaths = { TEXT("Background") };
		}
		else if (Cast<UProgressBar>(Widget))
		{
			CandidatePaths = { SlotName };
		}
		else if (Cast<UCheckBox>(Widget))
		{
			CandidatePaths = { FString::Printf(TEXT("WidgetStyle.%s"), *SlotName) };
		}
		else if (Cast<USlider>(Widget))
		{
			if (SlotName.Equals(TEXT("BarImage"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.NormalBarImage") };
			else if (SlotName.Equals(TEXT("ThumbImage"), ESearchCase::IgnoreCase)) CandidatePaths = { TEXT("WidgetStyle.NormalThumbImage") };
			else CandidatePaths = { FString::Printf(TEXT("WidgetStyle.%s"), *SlotName) };
		}

		CandidatePaths.AddUnique(SlotName);
		CandidatePaths.AddUnique(FString::Printf(TEXT("WidgetStyle.%s"), *SlotName));

		for (const FString& CandidatePath : CandidatePaths)
		{
			if (ResolveWidgetProperty(Widget, CandidatePath, OutResolved) && IsSlateBrushProperty(OutResolved.Property))
			{
				return true;
			}
		}

		return false;
	}

	UWidgetAnimation* FindAnimationByName(UWidgetBlueprint* WidgetBP, const FString& AnimationName)
	{
		if (!WidgetBP)
		{
			return nullptr;
		}

		for (UWidgetAnimation* Animation : WidgetBP->Animations)
		{
			if (!Animation)
			{
				continue;
			}

			if (Animation->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
			{
				return Animation;
			}

#if WITH_EDITOR
			if (Animation->GetDisplayLabel().Equals(AnimationName, ESearchCase::IgnoreCase))
			{
				return Animation;
			}
#endif
		}

		return nullptr;
	}

	int32 GetAnimationTrackCount(UWidgetAnimation* Animation)
	{
		if (!Animation || !Animation->GetMovieScene())
		{
			return 0;
		}

		int32 TrackCount = 0;
		const UMovieScene* MovieScene = Animation->GetMovieScene();
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			TrackCount += Binding.GetTracks().Num();
		}

		return TrackCount;
	}

	bool ResolveAnimationPropertyTarget(UWidget* DesignWidget, UUserWidget* PreviewWidget, const FString& PropertyName, FWidgetAnimationPropertyTarget& OutTarget)
	{
		OutTarget = {};
		if (!DesignWidget || !PreviewWidget)
		{
			return false;
		}

		UWidget* RuntimeWidget = FindRuntimeWidgetByName(PreviewWidget, DesignWidget->GetName());
		if (!RuntimeWidget)
		{
			return false;
		}

		if (PropertyName.Equals(TEXT("Position X"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Position Y"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Size X"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Size Y"), ESearchCase::IgnoreCase))
		{
			UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RuntimeWidget->Slot);
			if (!CanvasSlot)
			{
				return false;
			}

			OutTarget.AnimatedObject = CanvasSlot;
			OutTarget.TrackType = EWidgetAnimationTrackType::Margin;
			OutTarget.PropertyName = TEXT("Offsets");
			OutTarget.PropertyPath = TEXT("LayoutData.Offsets");

			if (PropertyName.Equals(TEXT("Position X"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 0;
			else if (PropertyName.Equals(TEXT("Position Y"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 1;
			else if (PropertyName.Equals(TEXT("Size X"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 2;
			else if (PropertyName.Equals(TEXT("Size Y"), ESearchCase::IgnoreCase)) OutTarget.MarginChannelIndex = 3;

			return true;
		}

		FResolvedWidgetProperty Resolved;
		if (!ResolveWidgetProperty(RuntimeWidget, PropertyName, Resolved))
		{
			return false;
		}

		OutTarget.AnimatedObject = Resolved.TargetObject;
		OutTarget.PropertyName = Resolved.Property->GetFName();
		OutTarget.PropertyPath = Resolved.ResolvedPath;

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Resolved.Property))
		{
			if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				OutTarget.TrackType = EWidgetAnimationTrackType::Color;
				return true;
			}

			if (StructProperty->Struct && StructProperty->Struct->GetFName() == TEXT("SlateColor"))
			{
				OutTarget.TrackType = EWidgetAnimationTrackType::Color;
				OutTarget.PropertyName = TEXT("SpecifiedColor");
				OutTarget.PropertyPath = Resolved.ResolvedPath + TEXT(".SpecifiedColor");
				return true;
			}
		}

		if (CastField<FFloatProperty>(Resolved.Property) || CastField<FDoubleProperty>(Resolved.Property))
		{
			OutTarget.TrackType = EWidgetAnimationTrackType::Float;
			return true;
		}

		return false;
	}

	FGuid EnsureAnimationBinding(UWidgetAnimation* Animation, UUserWidget* PreviewWidget, UObject* AnimatedObject)
	{
		if (!Animation || !Animation->GetMovieScene() || !PreviewWidget || !AnimatedObject)
		{
			return FGuid();
		}

		const UPanelSlot* SlotObject = Cast<UPanelSlot>(AnimatedObject);
		for (const FWidgetAnimationBinding& Binding : Animation->AnimationBindings)
		{
			if (SlotObject)
			{
				if (Binding.WidgetName == SlotObject->Content->GetFName() && Binding.SlotWidgetName == SlotObject->GetFName())
				{
					return Binding.AnimationGuid;
				}
			}
			else if (Binding.WidgetName == AnimatedObject->GetFName() && Binding.SlotWidgetName.IsNone())
			{
				return Binding.AnimationGuid;
			}
		}

		if (!Animation->CanPossessObject(*AnimatedObject, PreviewWidget))
		{
			return FGuid();
		}

		const FGuid BindingGuid = Animation->GetMovieScene()->AddPossessable(AnimatedObject->GetName(), AnimatedObject->GetClass());
		Animation->BindPossessableObject(BindingGuid, *AnimatedObject, PreviewWidget);
		return BindingGuid;
	}

	template<typename TrackType>
	TrackType* FindOrAddPropertyTrack(UMovieScene* MovieScene, const FGuid& BindingGuid, const FName& PropertyName, const FString& PropertyPath)
	{
		if (!MovieScene)
		{
			return nullptr;
		}

		for (UMovieSceneTrack* ExistingTrack : MovieScene->FindTracks(TrackType::StaticClass(), BindingGuid))
		{
			UMovieScenePropertyTrack* ExistingPropertyTrack = Cast<UMovieScenePropertyTrack>(ExistingTrack);
			if (ExistingPropertyTrack && ExistingPropertyTrack->GetPropertyName() == PropertyName && ExistingPropertyTrack->GetPropertyPath().ToString() == PropertyPath)
			{
				return Cast<TrackType>(ExistingTrack);
			}
		}

		TrackType* NewTrack = MovieScene->AddTrack<TrackType>(BindingGuid);
		if (UMovieScenePropertyTrack* NewPropertyTrack = Cast<UMovieScenePropertyTrack>(NewTrack))
		{
			NewPropertyTrack->SetPropertyNameAndPath(PropertyName, PropertyPath);
		}
		return NewTrack;
	}

	template<typename SectionType>
	SectionType* FindOrAddSection(UMovieScenePropertyTrack* Track, const TRange<FFrameNumber>& DefaultRange)
	{
		if (!Track)
		{
			return nullptr;
		}

		for (UMovieSceneSection* ExistingSection : Track->GetAllSections())
		{
			if (SectionType* TypedSection = Cast<SectionType>(ExistingSection))
			{
				return TypedSection;
			}
		}

		SectionType* NewSection = Cast<SectionType>(Track->CreateNewSection());
		if (!NewSection)
		{
			return nullptr;
		}

		NewSection->SetRange(DefaultRange);
		Track->AddSection(*NewSection);
		return NewSection;
	}

	void EnsurePlaybackRangeIncludesFrame(UMovieScene* MovieScene, FFrameNumber Frame)
	{
		if (!MovieScene)
		{
			return;
		}

		TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
		FFrameNumber Lower = PlaybackRange.HasLowerBound() ? PlaybackRange.GetLowerBoundValue() : FFrameNumber(0);
		FFrameNumber Upper = PlaybackRange.HasUpperBound() ? PlaybackRange.GetUpperBoundValue() : FFrameNumber(1);

		if (Frame < Lower)
		{
			Lower = Frame;
		}
		if (Frame >= Upper)
		{
			Upper = Frame + 1;
		}

		MovieScene->SetPlaybackRange(TRange<FFrameNumber>(Lower, Upper));
	}

	bool ExportFloatValue(void* ValuePtr, FProperty* Property, UObject* OwnerObject, float& OutValue)
	{
		FString ValueText;
		Property->ExportTextItem_Direct(ValueText, ValuePtr, nullptr, OwnerObject, PPF_None);
		return LexTryParseString(OutValue, *ValueText);
	}

	bool ExportLinearColorValue(void* ValuePtr, FProperty* Property, UObject* OwnerObject, FLinearColor& OutValue)
	{
		FString ValueText;
		Property->ExportTextItem_Direct(ValueText, ValuePtr, nullptr, OwnerObject, PPF_None);
		return ParseLinearColor(ValueText, OutValue);
	}
}

// =================================================================
// Helper Methods
// =================================================================

UWidgetBlueprint* UWidgetService::LoadWidgetBlueprint(const FString& WidgetPath)
{
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(WidgetPath));
	if (!WidgetBP)
	{
		// UEditorAssetLibrary::LoadAsset returns null during PIE (CheckIfInEditorAndPIE guard), so
		// every widget entry point silently reported "not found" mid-session (issue #557). Accept
		// either a package path (/Game/UI/WBP_X) or an object path (/Game/UI/WBP_X.WBP_X) and
		// resolve it directly, the same way SpawnWidgetInPIE always had to.
		FString ObjectPath = WidgetPath;
		if (!ObjectPath.Contains(TEXT(".")))
		{
			const FString AssetName = FPackageName::GetShortName(WidgetPath);
			ObjectPath = FString::Printf(TEXT("%s.%s"), *WidgetPath, *AssetName);
		}
		WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	}
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService: Failed to load Widget Blueprint: %s"), *WidgetPath);
	}
	return WidgetBP;
}

UWidget* UWidgetService::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& ComponentName)
{
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return nullptr;
	}

	// Search all widgets in the tree
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			return Widget;
		}
	}
	
	return nullptr;
}

// =================================================================
// Discovery Methods
// =================================================================





















FPIEWidgetHandle UWidgetService::SpawnWidgetInPIE(
	const FString& WidgetPath,
	int32 ZOrder)
{
	FPIEWidgetHandle Result;

	if (!GEditor || !GEditor->PlayWorld)
	{
		Result.ErrorMessage = TEXT("PIE is not running.");
		return Result;
	}

	// NOTE: LoadWidgetBlueprint() uses UEditorAssetLibrary::LoadAsset, which returns null
	// during PIE — so resolve the object path directly with LoadObject here, which works
	// while PIE is running. Accept either a package path (/Game/UI/WBP_X) or a full object
	// path (/Game/UI/WBP_X.WBP_X).
	FString ObjectPath = WidgetPath;
	if (!ObjectPath.Contains(TEXT(".")))
	{
		FString AssetName;
		if (WidgetPath.Split(TEXT("/"), nullptr, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			ObjectPath = FString::Printf(TEXT("%s.%s"), *WidgetPath, *AssetName);
		}
	}
	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint not found.");
		return Result;
	}

	if (!WidgetBP->GeneratedClass || WidgetBP->Status == BS_Dirty)
	{
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	}

	if (!WidgetBP->GeneratedClass || !WidgetBP->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
	{
		Result.ErrorMessage = TEXT("Widget Blueprint is not a valid UserWidget class.");
		return Result;
	}

	UWorld* PlayWorld = GEditor->PlayWorld.Get();
	UClass* WidgetClass = WidgetBP->GeneratedClass;
	if (!PlayWorld || !WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass()))
	{
		Result.ErrorMessage = TEXT("PIE world or widget class is invalid.");
		return Result;
	}

	const FName WidgetName = MakeUniqueObjectName(PlayWorld, WidgetClass, FName(*FString::Printf(TEXT("%s_PIE"), *WidgetBP->GetName())));
	UUserWidget* WidgetInstance = CreateWidget<UUserWidget>(PlayWorld, WidgetClass, WidgetName);
	if (!WidgetInstance)
	{
		Result.ErrorMessage = TEXT("Failed to create PIE widget instance.");
		return Result;
	}

	WidgetInstance->AddToViewport(ZOrder);

	Result.bValid = true;
	Result.InstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	GPIEWidgetInstances.Add(Result.InstanceId, WidgetInstance);
	return Result;
}


bool UWidgetService::RemoveWidgetFromPIE(const FPIEWidgetHandle& Handle)
{
	TWeakObjectPtr<UUserWidget>* WidgetPtr = GPIEWidgetInstances.Find(Handle.InstanceId);
	if (!WidgetPtr)
	{
		return false;
	}

	if (WidgetPtr->IsValid())
	{
		WidgetPtr->Get()->RemoveFromParent();
	}

	GPIEWidgetInstances.Remove(Handle.InstanceId);
	return true;
}

// =================================================================
// Event Handling
// =================================================================


bool UWidgetService::BindEvent(
	const FString& WidgetPath,
	const FString& WidgetName,
	const FString& EventName,
	const FString& FunctionName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	// 1. Resolve the widget component so we know its class (the delegate owner).
	UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName);
	if (!Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Widget '%s' not found in '%s'"), *WidgetName, *WidgetPath);
		return false;
	}
	UClass* WidgetClass = Widget->GetClass();

	// 2. Verify the event (a multicast delegate) actually exists on that widget class.
	const FName EventFName(*EventName);
	if (!FindFProperty<FMulticastDelegateProperty>(WidgetClass, EventFName))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Event '%s' does not exist on widget '%s' (%s). Use get_available_events to list valid events."),
			*EventName, *WidgetName, *WidgetClass->GetName());
		return false;
	}

	// 3. The widget must be a Blueprint variable for an event to bind to it. Promote it if needed.
	const FName WidgetFName(*WidgetName);
	auto FindWidgetVarProp = [WidgetBP, WidgetFName]() -> FObjectProperty*
	{
		UClass* SkelClass = WidgetBP->SkeletonGeneratedClass ? WidgetBP->SkeletonGeneratedClass : WidgetBP->GeneratedClass;
		return SkelClass ? FindFProperty<FObjectProperty>(SkelClass, WidgetFName) : nullptr;
	};
	FObjectProperty* VariableProperty = FindWidgetVarProp();
	if (!VariableProperty)
	{
		if (!Widget->bIsVariable)
		{
			Widget->bIsVariable = true;
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		VariableProperty = FindWidgetVarProp();
	}
	if (!VariableProperty)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Could not resolve widget variable '%s' on '%s' after promoting it to a variable"), *WidgetName, *WidgetPath);
		return false;
	}

	// 4. Create the component-bound event node (mirrors the UMG designer's "+" button), or reuse an existing one.
	const UK2Node_ComponentBoundEvent* BoundNode =
		FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, EventFName, VariableProperty->GetFName());
	if (!BoundNode)
	{
		FKismetEditorUtilities::CreateNewBoundEventForClass(WidgetClass, EventFName, WidgetBP, VariableProperty, /*bShouldJumpToNode=*/false);
		BoundNode = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, EventFName, VariableProperty->GetFName());
	}
	if (!BoundNode)
	{
		UE_LOG(LogTemp, Error, TEXT("UWidgetService::BindEvent: Failed to create bound event '%s' for widget '%s' in '%s'"), *EventName, *WidgetName, *WidgetPath);
		return false;
	}

	// 5. Optionally wire the event's exec output to a call to the named handler function.
	//    Best-effort: the bound event node is itself a valid handler, so a missing function is a warning, not a failure.
	if (!FunctionName.IsEmpty())
	{
		UEdGraph* Graph = BoundNode->GetGraph();
		UClass* SkelClass = WidgetBP->SkeletonGeneratedClass ? WidgetBP->SkeletonGeneratedClass : WidgetBP->GeneratedClass;
		UFunction* HandlerFunction = SkelClass ? SkelClass->FindFunctionByName(FName(*FunctionName)) : nullptr;
		UEdGraphPin* ThenPin = BoundNode->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);

		if (!HandlerFunction)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Bound event '%s' created, but handler function '%s' was not found on '%s' — create the function first to auto-wire it. The event node is still available as a handler."),
				*EventName, *FunctionName, *WidgetPath);
		}
		else if (Graph && ThenPin && ThenPin->LinkedTo.Num() == 0)
		{
			// Idempotent: only wire when the event's exec output is still unconnected.
			UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
			CallNode->SetFromFunction(HandlerFunction);
			Graph->AddNode(CallNode, false, false);
			CallNode->CreateNewGuid();
			CallNode->PostPlacedNewNode();
			CallNode->AllocateDefaultPins();
			CallNode->NodePosX = BoundNode->NodePosX + 400;
			CallNode->NodePosY = BoundNode->NodePosY;

			UEdGraphPin* CallExecPin = CallNode->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			if (CallExecPin && K2Schema && K2Schema->TryCreateConnection(ThenPin, CallExecPin))
			{
				UE_LOG(LogTemp, Log, TEXT("UWidgetService::BindEvent: Wired %s.%s -> %s()"), *WidgetName, *EventName, *FunctionName);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UWidgetService::BindEvent: Bound event '%s' created, but could not auto-wire exec to '%s' (exec pin missing or incompatible)"), *EventName, *FunctionName);
			}
		}
	}

	// 6. Persist and verify.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);

	const bool bBound = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, EventFName, VariableProperty->GetFName()) != nullptr;
	UE_LOG(LogTemp, Log, TEXT("UWidgetService::BindEvent: %s — Widget: %s, Event: %s -> Function: %s"),
		bBound ? TEXT("BOUND") : TEXT("FAILED"), *WidgetName, *EventName, *FunctionName);
	return bBound;
}

// =================================================================
// ViewModel Helper Methods
// =================================================================

// Helper to find an existing MVVM extension on a WidgetBlueprint (without creating one)
static UMVVMWidgetBlueprintExtension_View* FindMVVMExtension(UWidgetBlueprint* WidgetBP)
{
	if (!WidgetBP)
	{
		return nullptr;
	}

	for (UBlueprintExtension* Extension : WidgetBP->GetExtensions())
	{
		if (UMVVMWidgetBlueprintExtension_View* ViewExt = Cast<UMVVMWidgetBlueprintExtension_View>(Extension))
		{
			return ViewExt;
		}
	}
	return nullptr;
}

UClass* UWidgetService::FindViewModelClass(const FString& ClassName)
{
	// Normalize: strip trailing _C suffix if present (Blueprint class names end with _C)
	FString NormalizedName = ClassName;
	if (NormalizedName.EndsWith(TEXT("_C")))
	{
		NormalizedName = NormalizedName.LeftChop(2);
	}

	// 1. Try direct class lookup (C++ and loaded Blueprint ViewModel classes)
	// Build candidate names to match against UClass::GetName()
	FString NameWithU = NormalizedName;
	if (!NameWithU.StartsWith(TEXT("U")))
	{
		NameWithU = TEXT("U") + NormalizedName;
	}
	// Blueprint-generated classes have _C suffix
	FString NameWithC = NormalizedName + TEXT("_C");

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TestClass = *It;
		const FString& TestName = TestClass->GetName();
		if (TestName == NormalizedName || TestName == NameWithU || TestName == NameWithC)
		{
			// Accept classes that inherit from UMVVMViewModelBase
			if (TestClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
			{
				return TestClass;
			}
		}
	}

	// 2. Try Asset Registry for dedicated MVVMViewModelBlueprint assets
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/ModelViewViewModelBlueprint.MVVMViewModelBlueprint")));
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			FString AssetName = AssetData.AssetName.ToString();
			if (AssetName.Equals(NormalizedName, ESearchCase::IgnoreCase))
			{
				if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
				{
					if (BP->GeneratedClass)
					{
						return BP->GeneratedClass;
					}
				}
			}
		}
	}

	// 3. Try Asset Registry for regular Blueprint assets whose parent is a ViewModel class
	// This handles Blueprints created via BlueprintService.create_blueprint with MVVMViewModelBase parent
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine.Blueprint")));
		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssets(Filter, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			FString AssetName = AssetData.AssetName.ToString();
			if (AssetName.Equals(NormalizedName, ESearchCase::IgnoreCase))
			{
				if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
				{
					if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
					{
						return BP->GeneratedClass;
					}
				}
			}
		}
	}

	// 4. Try by full path (if user provides asset path like /Game/UI/Tests/VM_TestHUD)
	FString PathToLoad = NormalizedName;
	if (PathToLoad.StartsWith(TEXT("/")))
	{
		// Strip .ClassName suffix if present (e.g., /Game/UI/Tests/VM_TestHUD.VM_TestHUD)
		int32 DotIndex;
		if (PathToLoad.FindLastChar(TEXT('.'), DotIndex))
		{
			PathToLoad = PathToLoad.Left(DotIndex);
		}

		UObject* LoadedObj = UEditorAssetLibrary::LoadAsset(PathToLoad);
		if (UBlueprint* BP = Cast<UBlueprint>(LoadedObj))
		{
			if (BP->GeneratedClass && BP->GeneratedClass->IsChildOf(UMVVMViewModelBase::StaticClass()))
			{
				return BP->GeneratedClass;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UWidgetService::FindViewModelClass: Class '%s' not found. "
		"Tried: C++ classes ('%s', '%s', '%s'), MVVMViewModelBlueprint assets, regular Blueprint assets, "
		"and full path loading. Ensure the ViewModel inherits from MVVMViewModelBase and is compiled."),
		*ClassName, *NormalizedName, *NameWithU, *NameWithC);
	return nullptr;
}

UMVVMBlueprintView* UWidgetService::GetOrCreateMVVMView(UWidgetBlueprint* WidgetBP)
{
	if (!WidgetBP)
	{
		return nullptr;
	}

	// Try to find existing MVVM extension
	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);

	// Create extension if not found
	if (!ViewExtension)
	{
		ViewExtension = NewObject<UMVVMWidgetBlueprintExtension_View>(WidgetBP);
		if (!ViewExtension)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService: Failed to create MVVM extension"));
			return nullptr;
		}
		WidgetBP->AddExtension(ViewExtension);
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		ViewExtension->CreateBlueprintViewInstance();
		View = ViewExtension->GetBlueprintView();
	}

	return View;
}

FString UWidgetService::BindingModeToString(EMVVMBindingMode Mode)
{
	switch (Mode)
	{
	case EMVVMBindingMode::OneTimeToDestination: return TEXT("OneTimeToDestination");
	case EMVVMBindingMode::OneWayToDestination: return TEXT("OneWayToDestination");
	case EMVVMBindingMode::TwoWay: return TEXT("TwoWay");
	case EMVVMBindingMode::OneTimeToSource: return TEXT("OneTimeToSource");
	case EMVVMBindingMode::OneWayToSource: return TEXT("OneWayToSource");
	default: return TEXT("Unknown");
	}
}

EMVVMBindingMode UWidgetService::StringToBindingMode(const FString& ModeString)
{
	if (ModeString.Equals(TEXT("OneTimeToDestination"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneTimeToDestination;
	if (ModeString.Equals(TEXT("OneWayToDestination"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneWayToDestination;
	if (ModeString.Equals(TEXT("TwoWay"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::TwoWay;
	if (ModeString.Equals(TEXT("OneTimeToSource"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneTimeToSource;
	if (ModeString.Equals(TEXT("OneWayToSource"), ESearchCase::IgnoreCase))
		return EMVVMBindingMode::OneWayToSource;

	// Default
	return EMVVMBindingMode::OneWayToDestination;
}

// =================================================================
// ViewModel Management (MVVM)
// =================================================================



bool UWidgetService::RemoveViewModel(
	const FString& WidgetPath,
	const FString& ViewModelName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: No MVVM extension found"));
		return false;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: No MVVM view found"));
		return false;
	}

	// Find the ViewModel by name
	const FMVVMBlueprintViewModelContext* VMContext = View->FindViewModel(FName(*ViewModelName));
	if (!VMContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModel: ViewModel '%s' not found"), *ViewModelName);
		return false;
	}

	FGuid ViewModelId = VMContext->GetViewModelId();
	bool bRemoved = View->RemoveViewModel(ViewModelId);

	if (bRemoved)
	{
		WidgetBP->Modify();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
		UE_LOG(LogTemp, Log, TEXT("UWidgetService::RemoveViewModel: Removed ViewModel '%s' from '%s'"),
			*ViewModelName, *WidgetPath);
	}

	return bRemoved;
}




// =================================================================
// Existence Checks
// =================================================================



// =================================================================
// Snapshot API
// =================================================================

FWidgetSlotInfo UWidgetService::BuildSlotInfo(UWidget* Widget)
{
	FWidgetSlotInfo SlotInfo;

	if (!Widget || !Widget->Slot)
	{
		SlotInfo.SlotType = TEXT("None");
		return SlotInfo;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("Canvas");
		FAnchorData Layout = CanvasSlot->GetLayout();
		SlotInfo.AnchorMin = Layout.Anchors.Minimum;
		SlotInfo.AnchorMax = Layout.Anchors.Maximum;
		SlotInfo.Offsets = Layout.Offsets;
		SlotInfo.Alignment = Layout.Alignment;
		SlotInfo.ZOrder = CanvasSlot->GetZOrder();
		SlotInfo.bAutoSize = CanvasSlot->GetAutoSize();
	}
	else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("VerticalBox");
		FSlateChildSize Size = VBoxSlot->GetSize();
		SlotInfo.SizeRule = (Size.SizeRule == ESlateSizeRule::Fill) ? TEXT("Fill") : TEXT("Automatic");
		SlotInfo.SizeValue = Size.Value;
		SlotInfo.Padding = VBoxSlot->GetPadding();
		SlotInfo.HorizontalAlignment = VBoxSlot->GetHorizontalAlignment();
		SlotInfo.VerticalAlignment = VBoxSlot->GetVerticalAlignment();
	}
	else if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("HorizontalBox");
		FSlateChildSize Size = HBoxSlot->GetSize();
		SlotInfo.SizeRule = (Size.SizeRule == ESlateSizeRule::Fill) ? TEXT("Fill") : TEXT("Automatic");
		SlotInfo.SizeValue = Size.Value;
		SlotInfo.Padding = HBoxSlot->GetPadding();
		SlotInfo.HorizontalAlignment = HBoxSlot->GetHorizontalAlignment();
		SlotInfo.VerticalAlignment = HBoxSlot->GetVerticalAlignment();
	}
	else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Widget->Slot))
	{
		SlotInfo.SlotType = TEXT("Overlay");
		SlotInfo.Padding = OverlaySlot->GetPadding();
		SlotInfo.HorizontalAlignment = OverlaySlot->GetHorizontalAlignment();
		SlotInfo.VerticalAlignment = OverlaySlot->GetVerticalAlignment();
	}
	else
	{
		// Unknown slot type — store class name so caller knows what it is
		SlotInfo.SlotType = Widget->Slot->GetClass()->GetName();
	}

	return SlotInfo;
}

TArray<FWidgetPropertyInfo> UWidgetService::CollectWidgetProperties(UWidget* Widget)
{
	TArray<FWidgetPropertyInfo> Properties;

	for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		FWidgetPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
		Info.bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
		Info.Category = Property->GetMetaData(TEXT("Category"));

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
		Property->ExportTextItem_Direct(Info.CurrentValue, ValuePtr, nullptr, Widget, PPF_None);

		Properties.Add(Info);
	}

	return Properties;
}


