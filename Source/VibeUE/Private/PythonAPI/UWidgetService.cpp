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

		return false;
	}

	bool ResolveWidgetProperty(UWidget* Widget, const FString& PropertyName, FResolvedWidgetProperty& OutResolved)
	{
		OutResolved = {};
		if (!Widget)
		{
			return false;
		}

		OutResolved.TargetObject = Widget;
		OutResolved.ResolvedPath = PropertyName;
		ResolveSpecialWidgetProperty(Widget, PropertyName, OutResolved.TargetObject, OutResolved.ResolvedPath);

		if (!ResolvePropertyPath(OutResolved.TargetObject->GetClass(), OutResolved.TargetObject, OutResolved.ResolvedPath, OutResolved.Property, OutResolved.ValuePtr))
		{
			return false;
		}

		return true;
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

TSubclassOf<UWidget> UWidgetService::FindWidgetClass(const FString& TypeName)
{
	// Map type names to widget classes
	static TMap<FString, TSubclassOf<UWidget>> WidgetClassMap;
	
	if (WidgetClassMap.Num() == 0)
	{
		WidgetClassMap.Add(TEXT("TextBlock"), UTextBlock::StaticClass());
		WidgetClassMap.Add(TEXT("Button"), UButton::StaticClass());
		WidgetClassMap.Add(TEXT("Image"), UImage::StaticClass());
		WidgetClassMap.Add(TEXT("EditableText"), UEditableText::StaticClass());
		WidgetClassMap.Add(TEXT("EditableTextBox"), UEditableTextBox::StaticClass());
		WidgetClassMap.Add(TEXT("CheckBox"), UCheckBox::StaticClass());
		WidgetClassMap.Add(TEXT("Slider"), USlider::StaticClass());
		WidgetClassMap.Add(TEXT("ProgressBar"), UProgressBar::StaticClass());
		WidgetClassMap.Add(TEXT("Spacer"), USpacer::StaticClass());
		WidgetClassMap.Add(TEXT("CanvasPanel"), UCanvasPanel::StaticClass());
		WidgetClassMap.Add(TEXT("Overlay"), UOverlay::StaticClass());
		WidgetClassMap.Add(TEXT("HorizontalBox"), UHorizontalBox::StaticClass());
		WidgetClassMap.Add(TEXT("VerticalBox"), UVerticalBox::StaticClass());
		WidgetClassMap.Add(TEXT("ScrollBox"), UScrollBox::StaticClass());
		WidgetClassMap.Add(TEXT("GridPanel"), UGridPanel::StaticClass());
		WidgetClassMap.Add(TEXT("WidgetSwitcher"), UWidgetSwitcher::StaticClass());
		WidgetClassMap.Add(TEXT("RichTextBlock"), URichTextBlock::StaticClass());
		WidgetClassMap.Add(TEXT("ComboBoxString"), UComboBoxString::StaticClass());
		WidgetClassMap.Add(TEXT("SpinBox"), USpinBox::StaticClass());
		WidgetClassMap.Add(TEXT("MultiLineEditableText"), UMultiLineEditableText::StaticClass());
		WidgetClassMap.Add(TEXT("MultiLineEditableTextBox"), UMultiLineEditableTextBox::StaticClass());
		WidgetClassMap.Add(TEXT("InputKeySelector"), UInputKeySelector::StaticClass());
		WidgetClassMap.Add(TEXT("Border"), UBorder::StaticClass());
		WidgetClassMap.Add(TEXT("SizeBox"), USizeBox::StaticClass());
		WidgetClassMap.Add(TEXT("ScaleBox"), UScaleBox::StaticClass());
		WidgetClassMap.Add(TEXT("BackgroundBlur"), UBackgroundBlur::StaticClass());
		WidgetClassMap.Add(TEXT("SafeZone"), USafeZone::StaticClass());
		WidgetClassMap.Add(TEXT("UniformGridPanel"), UUniformGridPanel::StaticClass());
		WidgetClassMap.Add(TEXT("WrapBox"), UWrapBox::StaticClass());
		WidgetClassMap.Add(TEXT("InvalidationBox"), UInvalidationBox::StaticClass());
		WidgetClassMap.Add(TEXT("RetainerBox"), URetainerBox::StaticClass());
		WidgetClassMap.Add(TEXT("Throbber"), UThrobber::StaticClass());
		WidgetClassMap.Add(TEXT("CircularThrobber"), UCircularThrobber::StaticClass());
		WidgetClassMap.Add(TEXT("ListView"), UListView::StaticClass());
		WidgetClassMap.Add(TEXT("TreeView"), UTreeView::StaticClass());
		WidgetClassMap.Add(TEXT("TileView"), UTileView::StaticClass());
		WidgetClassMap.Add(TEXT("ExpandableArea"), UExpandableArea::StaticClass());
		WidgetClassMap.Add(TEXT("MenuAnchor"), UMenuAnchor::StaticClass());
		WidgetClassMap.Add(TEXT("NativeWidgetHost"), UNativeWidgetHost::StaticClass());
	}
	
	if (TSubclassOf<UWidget>* Found = WidgetClassMap.Find(TypeName))
	{
		return *Found;
	}
	
	// Try case-insensitive search
	for (const auto& Pair : WidgetClassMap)
	{
		if (Pair.Key.Equals(TypeName, ESearchCase::IgnoreCase))
		{
			return Pair.Value;
		}
	}

	// Fallback: search Asset Registry for a custom Widget Blueprint by name
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMGEditor.WidgetBlueprint")));
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (AssetData.AssetName.ToString().Equals(TypeName, ESearchCase::IgnoreCase))
		{
			if (UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(AssetData.GetAsset()))
			{
				// Ensure the WBP is compiled before using its GeneratedClass
				if (!WBP->GeneratedClass || WBP->Status == BS_Dirty)
				{
					UE_LOG(LogTemp, Log, TEXT("UWidgetService: Compiling WBP '%s' before resolving class"), *TypeName);
					FKismetEditorUtilities::CompileBlueprint(WBP);
				}

				if (WBP->GeneratedClass && WBP->GeneratedClass->IsChildOf(UWidget::StaticClass()))
				{
					TSubclassOf<UWidget> WidgetSubclass(*WBP->GeneratedClass);
					WidgetClassMap.Add(TypeName, WidgetSubclass);
					return WidgetSubclass;
				}
			}
		}
	}

	return nullptr;
}

// =================================================================
// Discovery Methods
// =================================================================

TArray<FString> UWidgetService::ListWidgetBlueprints(const FString& PathFilter)
{
	TArray<FString> WidgetPaths;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMGEditor.WidgetBlueprint")));

	if (!PathFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*PathFilter));
		Filter.bRecursivePaths = true;
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		WidgetPaths.Add(AssetData.GetObjectPathString());
	}

	return WidgetPaths;
}

TArray<FWidgetInfo> UWidgetService::GetHierarchy(const FString& WidgetPath)
{
	TArray<FWidgetInfo> Hierarchy;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Hierarchy;
	}

	UWidget* RootWidget = WidgetBP->WidgetTree->RootWidget;
	if (!RootWidget)
	{
		return Hierarchy;
	}

	// Build hierarchy recursively
	TFunction<void(UWidget*, const FString&)> AddWidgetToHierarchy;
	AddWidgetToHierarchy = [&](UWidget* Widget, const FString& ParentName)
	{
		if (!Widget)
		{
			return;
		}

		FWidgetInfo Info;
		Info.WidgetName = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.ParentWidget = ParentName;
		Info.bIsRootWidget = (Widget == RootWidget);
		Info.bIsVariable = Widget->bIsVariable;

		// If it's a panel widget, collect children
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
			{
				if (UWidget* Child = PanelWidget->GetChildAt(i))
				{
					Info.Children.Add(Child->GetName());
				}
			}
		}

		Hierarchy.Add(Info);

		// Recurse into children
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
			{
				UWidget* Child = PanelWidget->GetChildAt(i);
				AddWidgetToHierarchy(Child, Info.WidgetName);
			}
		}
	};

	AddWidgetToHierarchy(RootWidget, FString());

	return Hierarchy;
}

FString UWidgetService::GetRootWidget(const FString& WidgetPath)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree || !WidgetBP->WidgetTree->RootWidget)
	{
		return FString();
	}

	return WidgetBP->WidgetTree->RootWidget->GetName();
}

TArray<FWidgetInfo> UWidgetService::ListComponents(const FString& WidgetPath)
{
	TArray<FWidgetInfo> Components;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Components;
	}

	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);

	for (UWidget* Widget : AllWidgets)
	{
		if (!Widget)
		{
			continue;
		}

		FWidgetInfo Info;
		Info.WidgetName = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.bIsVariable = Widget->bIsVariable;
		Info.bIsRootWidget = (Widget == WidgetBP->WidgetTree->RootWidget);

		// Get parent
		if (UPanelWidget* Parent = Widget->GetParent())
		{
			Info.ParentWidget = Parent->GetName();
		}

		// Get children if panel
		if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < PanelWidget->GetChildrenCount(); ++i)
			{
				if (UWidget* Child = PanelWidget->GetChildAt(i))
				{
					Info.Children.Add(Child->GetName());
				}
			}
		}

		Components.Add(Info);
	}

	return Components;
}

TArray<FString> UWidgetService::SearchTypes(const FString& FilterText)
{
	TArray<FString> Results;
	
	// Built-in native widget types
	for (const FString& Type : GAvailableWidgetTypes)
	{
		if (FilterText.IsEmpty() || Type.Contains(FilterText, ESearchCase::IgnoreCase))
		{
			Results.Add(Type);
		}
	}

	// Also list custom Widget Blueprints from the Asset Registry
	// (Note: these CANNOT be added via add_component - they are listed for reference only)
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMGEditor.WidgetBlueprint")));
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetName = AssetData.AssetName.ToString();
		if (FilterText.IsEmpty() || AssetName.Contains(FilterText, ESearchCase::IgnoreCase))
		{
			// Prefix with [WBP] to distinguish from native types
			Results.Add(FString::Printf(TEXT("[WBP] %s (%s)"), *AssetName, *AssetData.GetObjectPathString()));
		}
	}
	
	return Results;
}

TArray<FWidgetPropertyInfo> UWidgetService::GetComponentProperties(const FString& WidgetPath, const FString& ComponentName)
{
	return ListProperties(WidgetPath, ComponentName, false);
}

// =================================================================
// Component Management
// =================================================================

FWidgetAddComponentResult UWidgetService::AddComponent(
	const FString& WidgetPath,
	const FString& ComponentType,
	const FString& ComponentName,
	const FString& ParentName,
	bool bIsVariable)
{
	FWidgetAddComponentResult Result;
	
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath);
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint has no WidgetTree");
		return Result;
	}

	// Find the widget class
	TSubclassOf<UWidget> WidgetClass = FindWidgetClass(ComponentType);
	if (!WidgetClass)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown widget type '%s'. Use search_types() to get available types, or list_widget_blueprints() for custom WBPs."), *ComponentType);
		return Result;
	}

	// For UserWidget subclasses (custom WBPs), do additional safety checks
	const bool bIsUserWidget = WidgetClass->IsChildOf(UUserWidget::StaticClass());
	if (bIsUserWidget)
	{
		// Prevent circular references: a WBP cannot contain itself as a child
		if (WidgetBP->GeneratedClass && (WidgetClass == WidgetBP->GeneratedClass ||
			WidgetClass->IsChildOf(WidgetBP->GeneratedClass)))
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("Cannot add '%s': circular reference detected. A Widget Blueprint cannot contain itself as a child component."),
				*ComponentType);
			return Result;
		}

		UE_LOG(LogTemp, Log, TEXT("UWidgetService: Adding custom WBP '%s' as component '%s'"), *ComponentType, *ComponentName);
	}

	// Find parent panel (or use root)
	UPanelWidget* ParentPanel = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* ParentWidget = FindWidgetByName(WidgetBP, ParentName);
		ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Parent '%s' not found or is not a panel widget"), *ParentName);
			return Result;
		}
	}
	else
	{
		// Use root as parent if it's a panel
		ParentPanel = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
	}

	// Create the new widget
	UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*ComponentName));
	if (!NewWidget)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create widget of type '%s'"), *ComponentType);
		return Result;
	}

	NewWidget->bIsVariable = bIsVariable;

	// Register widget with GUID map (required for UMG compilation)
	const FName WidgetFName = NewWidget->GetFName();
	if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(WidgetFName))
	{
		WidgetBP->WidgetVariableNameToGuidMap.Add(WidgetFName, FGuid::NewGuid());
	}

	// Add to parent or set as root
	if (ParentPanel)
	{
		UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
		if (!Slot)
		{
			Result.ErrorMessage = TEXT("Failed to add widget to parent panel");
			return Result;
		}
		Result.ParentName = ParentPanel->GetName();
	}
	else if (!WidgetBP->WidgetTree->RootWidget)
	{
		// Set as root if no root exists
		WidgetBP->WidgetTree->RootWidget = NewWidget;
		Result.ParentName = TEXT("(root)");
	}
	else
	{
		Result.ErrorMessage = TEXT("Cannot add widget: no parent specified and root already exists");
		return Result;
	}

	// Mark blueprint as modified and compile
	WidgetBP->Modify();

	if (bIsUserWidget)
	{
		// For UserWidget subclasses, do a synchronous compile immediately.
		// This ensures the parent WBP is in a valid compiled state before
		// any deferred operations (like auto-save) try to process it,
		// which would otherwise crash in Slate prepass with stack overflow.
		UE_LOG(LogTemp, Log, TEXT("UWidgetService: Compiling parent WBP after adding UserWidget child '%s'"), *ComponentName);
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
	}

	Result.bSuccess = true;
	Result.ComponentName = NewWidget->GetName();
	Result.ComponentType = ComponentType;
	Result.bIsVariable = bIsVariable;

	return Result;
}

FWidgetRemoveComponentResult UWidgetService::RemoveComponent(
	const FString& WidgetPath,
	const FString& ComponentName,
	bool bRemoveChildren)
{
	FWidgetRemoveComponentResult Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath);
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint has no WidgetTree");
		return Result;
	}

	UWidget* WidgetToRemove = FindWidgetByName(WidgetBP, ComponentName);
	if (!WidgetToRemove)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName);
		return Result;
	}

	// Can't remove root
	if (WidgetToRemove == WidgetBP->WidgetTree->RootWidget)
	{
		Result.ErrorMessage = TEXT("Cannot remove root widget");
		return Result;
	}

	// Collect children if needed
	if (UPanelWidget* Panel = Cast<UPanelWidget>(WidgetToRemove))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = Panel->GetChildAt(i))
			{
				if (bRemoveChildren)
				{
					Result.RemovedComponents.Add(Child->GetName());
				}
				else
				{
					Result.OrphanedChildren.Add(Child->GetName());
				}
			}
		}
	}

	// Remove from parent
	if (UPanelWidget* Parent = WidgetToRemove->GetParent())
	{
		Parent->RemoveChild(WidgetToRemove);
	}

	// Remove from widget tree
	WidgetBP->WidgetTree->RemoveWidget(WidgetToRemove);
	Result.RemovedComponents.Add(ComponentName);

	// Mark blueprint as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	Result.bSuccess = true;
	return Result;
}

// =================================================================
// Validation
// =================================================================

FWidgetValidationResult UWidgetService::Validate(const FString& WidgetPath)
{
	FWidgetValidationResult Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.bIsValid = false;
		Result.Errors.Add(FString::Printf(TEXT("Widget Blueprint '%s' not found"), *WidgetPath));
		Result.ValidationMessage = Result.Errors[0];
		return Result;
	}

	if (!WidgetBP->WidgetTree)
	{
		Result.bIsValid = false;
		Result.Errors.Add(TEXT("Widget Blueprint has no WidgetTree"));
		Result.ValidationMessage = Result.Errors[0];
		return Result;
	}

	// Check for root widget
	if (!WidgetBP->WidgetTree->RootWidget)
	{
		Result.bIsValid = false;
		Result.Errors.Add(TEXT("Widget Blueprint has no root widget"));
	}

	// Check for duplicate names
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	
	TSet<FString> WidgetNames;
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget)
		{
			FString Name = Widget->GetName();
			if (WidgetNames.Contains(Name))
			{
				Result.Errors.Add(FString::Printf(TEXT("Duplicate widget name: %s"), *Name));
			}
			WidgetNames.Add(Name);
		}
	}

	// Check for orphaned widgets (not attached to hierarchy)
	if (UWidget* Root = WidgetBP->WidgetTree->RootWidget)
	{
		TSet<UWidget*> ReachableWidgets;
		TFunction<void(UWidget*)> CollectReachable = [&](UWidget* Widget)
		{
			if (!Widget || ReachableWidgets.Contains(Widget))
			{
				return;
			}
			ReachableWidgets.Add(Widget);
			if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
				{
					CollectReachable(Panel->GetChildAt(i));
				}
			}
		};
		CollectReachable(Root);

		for (UWidget* Widget : AllWidgets)
		{
			if (Widget && !ReachableWidgets.Contains(Widget))
			{
				Result.Errors.Add(FString::Printf(TEXT("Orphaned widget not in hierarchy: %s"), *Widget->GetName()));
			}
		}
	}

	Result.bIsValid = (Result.Errors.Num() == 0);
	Result.ValidationMessage = Result.bIsValid 
		? TEXT("Widget hierarchy is valid") 
		: Result.Errors[0];

	return Result;
}

// =================================================================
// Property Access
// =================================================================

FString UWidgetService::GetProperty(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return FString();
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return FString();
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveWidgetProperty(Widget, PropertyName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::GetProperty: Property '%s' not found on widget '%s'"), *PropertyName, *ComponentName);
		return FString();
	}

	FString ValueStr;
	Resolved.Property->ExportTextItem_Direct(ValueStr, Resolved.ValuePtr, nullptr, Resolved.TargetObject, PPF_None);

	return ValueStr;
}

bool UWidgetService::SetProperty(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName,
	const FString& PropertyValue)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return false;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveWidgetProperty(Widget, PropertyName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: Property '%s' not found on widget '%s'"), *PropertyName, *ComponentName);
		return false;
	}

	if (Resolved.Property->ImportText_Direct(*PropertyValue, Resolved.ValuePtr, Resolved.TargetObject, PPF_None) == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetProperty: Failed to parse value '%s' for property '%s'"), *PropertyValue, *PropertyName);
		return false;
	}

	Resolved.TargetObject->Modify();
	MarkWidgetBlueprintModified(WidgetBP);

	return true;
}

TArray<FWidgetPropertyInfo> UWidgetService::ListProperties(
	const FString& WidgetPath,
	const FString& ComponentName,
	bool bEditableOnly)
{
	TArray<FWidgetPropertyInfo> Properties;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Properties;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Properties;
	}

	// Iterate all properties
	for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		// Skip non-blueprint visible properties if filtering
		bool bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);
		bool bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);

		if (bEditableOnly && !bIsEditable)
		{
			continue;
		}

		FWidgetPropertyInfo Info;
		Info.PropertyName = Property->GetName();
		Info.PropertyType = Property->GetCPPType();
		Info.bIsEditable = bIsEditable;
		Info.bIsBlueprintVisible = bIsBlueprintVisible;

		// Get category
		Info.Category = Property->GetMetaData(TEXT("Category"));

		// Get current value
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
		Property->ExportTextItem_Direct(Info.CurrentValue, ValuePtr, nullptr, Widget, PPF_None);

		Properties.Add(Info);
	}

	return Properties;
}

bool UWidgetService::SetFont(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FWidgetFontInfo& FontInfo,
	const FString& PropertyName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return false;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveFontProperty(Widget, PropertyName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetFont: Font property '%s' not found on widget '%s'"), *PropertyName, *ComponentName);
		return false;
	}

	FSlateFontInfo& SlateFont = *reinterpret_cast<FSlateFontInfo*>(Resolved.ValuePtr);
	if (!FontInfo.FontFamily.IsEmpty())
	{
		UObject* FontAsset = UEditorAssetLibrary::LoadAsset(FontInfo.FontFamily);
		if (!FontAsset)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetFont: Failed to load font asset '%s'"), *FontInfo.FontFamily);
			return false;
		}

		SlateFont.FontObject = FontAsset;
	}

	SlateFont.TypefaceFontName = FontInfo.Typeface.IsEmpty() ? FName(TEXT("Regular")) : FName(*FontInfo.Typeface);
	SlateFont.Size = FontInfo.Size;
	SlateFont.LetterSpacing = FontInfo.LetterSpacing;

	FLinearColor OutlineColor = FLinearColor::Black;
	if (ParseLinearColor(FontInfo.OutlineColor, OutlineColor))
		{
		SlateFont.OutlineSettings.OutlineColor = OutlineColor;
		}
	SlateFont.OutlineSettings.OutlineSize = FontInfo.OutlineSize;

	Resolved.TargetObject->Modify();
	Widget->Modify();

	TrySetPropertyText(Widget, TEXT("ColorAndOpacity"), FontInfo.Color);
	TrySetPropertyText(Widget, TEXT("ShadowOffset"), FontInfo.ShadowOffset);
	TrySetPropertyText(Widget, TEXT("ShadowColorAndOpacity"), FontInfo.ShadowColor);

	MarkWidgetBlueprintModified(WidgetBP);
	return true;
}

FWidgetFontInfo UWidgetService::GetFont(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& PropertyName)
{
	FWidgetFontInfo Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Result;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Result;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveFontProperty(Widget, PropertyName, Resolved))
	{
		return Result;
	}

	const FSlateFontInfo& SlateFont = *reinterpret_cast<const FSlateFontInfo*>(Resolved.ValuePtr);
	Result.FontFamily = SlateFont.FontObject ? SlateFont.FontObject->GetPathName() : FString();
	Result.Typeface = SlateFont.TypefaceFontName.IsNone() ? TEXT("Regular") : SlateFont.TypefaceFontName.ToString();
	Result.Size = SlateFont.Size;
	Result.LetterSpacing = SlateFont.LetterSpacing;
	Result.OutlineSize = SlateFont.OutlineSettings.OutlineSize;
	Result.OutlineColor = SlateFont.OutlineSettings.OutlineColor.ToString();

	TryGetPropertyText(Widget, TEXT("ColorAndOpacity"), Result.Color);
	TryGetPropertyText(Widget, TEXT("ShadowOffset"), Result.ShadowOffset);
	TryGetPropertyText(Widget, TEXT("ShadowColorAndOpacity"), Result.ShadowColor);

	return Result;
}

bool UWidgetService::SetBrush(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& SlotName,
	const FWidgetBrushInfo& BrushInfo)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return false;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveBrushProperty(Widget, SlotName, Resolved))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetBrush: Brush slot '%s' not found on widget '%s'"), *SlotName, *ComponentName);
		return false;
	}

	FSlateBrush& SlateBrush = *reinterpret_cast<FSlateBrush*>(Resolved.ValuePtr);
	SlateBrush.SetResourceObject(nullptr);
	SlateBrush.ImageType = BrushInfo.ResourcePath.IsEmpty() ? ESlateBrushImageType::NoImage : ESlateBrushImageType::FullColor;
	SlateBrush.DrawAs = StringToBrushDrawType(BrushInfo.DrawAs);

	FLinearColor TintColor = FLinearColor::White;
	if (ParseLinearColor(BrushInfo.TintColor, TintColor))
	{
		SlateBrush.TintColor = FSlateColor(TintColor);
	}

	FVector2D ImageSize = FVector2D::ZeroVector;
	if (ParseVector2D(BrushInfo.ImageSize, ImageSize))
	{
		SlateBrush.SetImageSize(ImageSize);
	}

	FMargin Margin;
	if (ParseMargin(BrushInfo.Margin, Margin))
	{
		SlateBrush.Margin = Margin;
	}

	if (SlateBrush.DrawAs == ESlateBrushDrawType::RoundedBox)
	{
		FVector4 CornerRadius;
		if (ParseCornerRadius(BrushInfo.CornerRadius, CornerRadius))
		{
			SlateBrush.OutlineSettings.CornerRadii = CornerRadius;
			SlateBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		}
	}

	if (!BrushInfo.ResourcePath.IsEmpty())
	{
		UObject* ResourceObject = UEditorAssetLibrary::LoadAsset(BrushInfo.ResourcePath);
		if (!ResourceObject)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::SetBrush: Failed to load resource '%s'"), *BrushInfo.ResourcePath);
			return false;
		}

		SlateBrush.SetResourceObject(ResourceObject);
	}

	Resolved.TargetObject->Modify();
	MarkWidgetBlueprintModified(WidgetBP);
	return true;
}

FWidgetBrushInfo UWidgetService::GetBrush(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& SlotName)
{
	FWidgetBrushInfo Result;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Result;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Result;
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveBrushProperty(Widget, SlotName, Resolved))
	{
		return Result;
	}

	const FSlateBrush& SlateBrush = *reinterpret_cast<const FSlateBrush*>(Resolved.ValuePtr);
	Result.ResourcePath = SlateBrush.GetResourceObject() ? SlateBrush.GetResourceObject()->GetPathName() : FString();
	Result.TintColor = SlateBrush.TintColor.GetSpecifiedColor().ToString();
	Result.DrawAs = BrushDrawTypeToString(SlateBrush.DrawAs);
	const FVector2D ImageSize = SlateBrush.GetImageSize();
	Result.ImageSize = FString::Printf(TEXT("(X=%0.6f,Y=%0.6f)"), ImageSize.X, ImageSize.Y);
	Result.Margin = FString::Printf(TEXT("(Left=%0.6f,Top=%0.6f,Right=%0.6f,Bottom=%0.6f)"), SlateBrush.Margin.Left, SlateBrush.Margin.Top, SlateBrush.Margin.Right, SlateBrush.Margin.Bottom);
	Result.CornerRadius = FString::Printf(TEXT("(TopLeft=%0.6f,TopRight=%0.6f,BottomRight=%0.6f,BottomLeft=%0.6f)"), SlateBrush.OutlineSettings.CornerRadii.X, SlateBrush.OutlineSettings.CornerRadii.Y, SlateBrush.OutlineSettings.CornerRadii.Z, SlateBrush.OutlineSettings.CornerRadii.W);

	return Result;
}

TArray<FWidgetAnimInfo> UWidgetService::ListAnimations(const FString& WidgetPath)
{
	TArray<FWidgetAnimInfo> Results;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Results;
	}

	for (UWidgetAnimation* Animation : WidgetBP->Animations)
	{
		if (!Animation)
		{
			continue;
		}

		FWidgetAnimInfo Info;
#if WITH_EDITOR
		Info.AnimationName = Animation->GetDisplayLabel().IsEmpty() ? Animation->GetName() : Animation->GetDisplayLabel();
#else
		Info.AnimationName = Animation->GetName();
#endif
		Info.Duration = Animation->GetEndTime() - Animation->GetStartTime();
		Info.TrackCount = GetAnimationTrackCount(Animation);
		Results.Add(Info);
	}

	return Results;
}

bool UWidgetService::CreateAnimation(
	const FString& WidgetPath,
	const FString& AnimationName,
	float Duration)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	if (AnimationName.IsEmpty() || FindAnimationByName(WidgetBP, AnimationName) != nullptr)
	{
		return false;
	}

	WidgetBP->Modify();

	UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(WidgetBP, FName(*AnimationName), RF_Transactional);
	if (!NewAnimation)
	{
		return false;
	}

#if WITH_EDITOR
	NewAnimation->SetDisplayLabel(AnimationName);
#endif
	NewAnimation->Rename(*AnimationName, WidgetBP);
	NewAnimation->MovieScene = NewObject<UMovieScene>(NewAnimation, FName(*AnimationName), RF_Transactional);
	if (!NewAnimation->MovieScene)
	{
		return false;
	}

	NewAnimation->MovieScene->SetDisplayRate(FFrameRate(20, 1));
	const FFrameTime OutFrame = FMath::Max(Duration, 0.0f) * NewAnimation->MovieScene->GetTickResolution();
	NewAnimation->MovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, OutFrame.FrameNumber + 1));
	NewAnimation->MovieScene->GetEditorData().WorkStart = 0.0f;
	NewAnimation->MovieScene->GetEditorData().WorkEnd = FMath::Max(Duration, 0.0f);

	WidgetBP->Animations.Add(NewAnimation);
	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

bool UWidgetService::RemoveAnimation(
	const FString& WidgetPath,
	const FString& AnimationName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidgetAnimation* Animation = FindAnimationByName(WidgetBP, AnimationName);
	if (!Animation)
	{
		return false;
	}

	WidgetBP->Modify();
	WidgetBP->Animations.Remove(Animation);
	Animation->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

bool UWidgetService::AddAnimationTrack(
	const FString& WidgetPath,
	const FString& AnimationName,
	const FString& ComponentName,
	const FString& PropertyName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* DesignWidget = FindWidgetByName(WidgetBP, ComponentName);
	UWidgetAnimation* Animation = FindAnimationByName(WidgetBP, AnimationName);
	UUserWidget* PreviewWidget = CreateWidgetInstanceForBlueprint(WidgetBP);
	if (!DesignWidget || !Animation || !PreviewWidget || !Animation->GetMovieScene())
	{
		return false;
	}

	FWidgetAnimationPropertyTarget Target;
	if (!ResolveAnimationPropertyTarget(DesignWidget, PreviewWidget, PropertyName, Target))
	{
		return false;
	}

	const FGuid BindingGuid = EnsureAnimationBinding(Animation, PreviewWidget, Target.AnimatedObject);
	if (!BindingGuid.IsValid())
	{
		return false;
	}

	if (Target.TrackType == EWidgetAnimationTrackType::Float)
	{
		UMovieSceneFloatTrack* Track = FindOrAddPropertyTrack<UMovieSceneFloatTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneFloatSection* Section = FindOrAddSection<UMovieSceneFloatSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		float CurrentValue = 0.0f;
		if (ResolvePropertyPath(Target.AnimatedObject->GetClass(), Target.AnimatedObject, Target.PropertyPath, Property, ValuePtr) && ExportFloatValue(ValuePtr, Property, Target.AnimatedObject, CurrentValue))
		{
			Section->GetChannel().SetDefault(CurrentValue);
		}
	}
	else if (Target.TrackType == EWidgetAnimationTrackType::Color)
	{
		UMovieSceneColorTrack* Track = FindOrAddPropertyTrack<UMovieSceneColorTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneColorSection* Section = FindOrAddSection<UMovieSceneColorSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FLinearColor CurrentColor = FLinearColor::White;
		if (ResolvePropertyPath(Target.AnimatedObject->GetClass(), Target.AnimatedObject, Target.PropertyPath, Property, ValuePtr) && ExportLinearColorValue(ValuePtr, Property, Target.AnimatedObject, CurrentColor))
		{
			Section->GetRedChannel().SetDefault(CurrentColor.R);
			Section->GetGreenChannel().SetDefault(CurrentColor.G);
			Section->GetBlueChannel().SetDefault(CurrentColor.B);
			Section->GetAlphaChannel().SetDefault(CurrentColor.A);
		}
	}
	else
	{
		UMovieSceneMarginTrack* Track = FindOrAddPropertyTrack<UMovieSceneMarginTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneMarginSection* Section = FindOrAddSection<UMovieSceneMarginSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Target.AnimatedObject);
		if (!Track || !Section || !CanvasSlot)
		{
			return false;
		}

		const FMargin Offsets = CanvasSlot->GetLayout().Offsets;
		Section->LeftCurve.SetDefault(Offsets.Left);
		Section->TopCurve.SetDefault(Offsets.Top);
		Section->RightCurve.SetDefault(Offsets.Right);
		Section->BottomCurve.SetDefault(Offsets.Bottom);
	}

	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

bool UWidgetService::AddKeyframe(
	const FString& WidgetPath,
	const FString& AnimationName,
	const FString& ComponentName,
	const FString& PropertyName,
	const FWidgetAnimKeyframe& Keyframe)
{
	if (!AddAnimationTrack(WidgetPath, AnimationName, ComponentName, PropertyName))
	{
		return false;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	UWidget* DesignWidget = FindWidgetByName(WidgetBP, ComponentName);
	UWidgetAnimation* Animation = FindAnimationByName(WidgetBP, AnimationName);
	UUserWidget* PreviewWidget = CreateWidgetInstanceForBlueprint(WidgetBP);
	if (!DesignWidget || !Animation || !PreviewWidget || !Animation->GetMovieScene())
	{
		return false;
	}

	FWidgetAnimationPropertyTarget Target;
	if (!ResolveAnimationPropertyTarget(DesignWidget, PreviewWidget, PropertyName, Target))
	{
		return false;
	}

	const FGuid BindingGuid = EnsureAnimationBinding(Animation, PreviewWidget, Target.AnimatedObject);
	if (!BindingGuid.IsValid())
	{
		return false;
	}

	const FFrameNumber FrameNumber = (Keyframe.Time * Animation->GetMovieScene()->GetTickResolution()).FrameNumber;
	EnsurePlaybackRangeIncludesFrame(Animation->GetMovieScene(), FrameNumber);
	const EMovieSceneKeyInterpolation Interpolation = StringToKeyInterpolation(Keyframe.Interpolation);

	if (Target.TrackType == EWidgetAnimationTrackType::Float)
	{
		float ParsedValue = 0.0f;
		if (!LexTryParseString(ParsedValue, *Keyframe.Value))
		{
			return false;
		}

		UMovieSceneFloatTrack* Track = FindOrAddPropertyTrack<UMovieSceneFloatTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneFloatSection* Section = FindOrAddSection<UMovieSceneFloatSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		AddKeyToChannel(&Section->GetChannel(), FrameNumber, ParsedValue, Interpolation);
	}
	else if (Target.TrackType == EWidgetAnimationTrackType::Color)
	{
		FLinearColor ParsedValue = FLinearColor::White;
		if (!ParseLinearColor(Keyframe.Value, ParsedValue))
		{
			return false;
		}

		UMovieSceneColorTrack* Track = FindOrAddPropertyTrack<UMovieSceneColorTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneColorSection* Section = FindOrAddSection<UMovieSceneColorSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		AddKeyToChannel(&Section->GetRedChannel(), FrameNumber, ParsedValue.R, Interpolation);
		AddKeyToChannel(&Section->GetGreenChannel(), FrameNumber, ParsedValue.G, Interpolation);
		AddKeyToChannel(&Section->GetBlueChannel(), FrameNumber, ParsedValue.B, Interpolation);
		AddKeyToChannel(&Section->GetAlphaChannel(), FrameNumber, ParsedValue.A, Interpolation);
	}
	else
	{
		float ParsedValue = 0.0f;
		if (!LexTryParseString(ParsedValue, *Keyframe.Value))
		{
			return false;
		}

		UMovieSceneMarginTrack* Track = FindOrAddPropertyTrack<UMovieSceneMarginTrack>(Animation->GetMovieScene(), BindingGuid, Target.PropertyName, Target.PropertyPath);
		UMovieSceneMarginSection* Section = FindOrAddSection<UMovieSceneMarginSection>(Track, Animation->GetMovieScene()->GetPlaybackRange());
		if (!Track || !Section)
		{
			return false;
		}

		switch (Target.MarginChannelIndex)
		{
		case 0:
			AddKeyToChannel(&Section->LeftCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		case 1:
			AddKeyToChannel(&Section->TopCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		case 2:
			AddKeyToChannel(&Section->RightCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		case 3:
			AddKeyToChannel(&Section->BottomCurve, FrameNumber, ParsedValue, Interpolation);
			break;
		default:
			return false;
		}
	}

	MarkWidgetBlueprintModified(WidgetBP, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	return true;
}

FWidgetPreviewResult UWidgetService::CapturePreview(
	const FString& WidgetPath,
	int32 Width,
	int32 Height)
{
	FWidgetPreviewResult Result;
	Result.Width = Width;
	Result.Height = Height;

	if (Width <= 0 || Height <= 0)
	{
		Result.ErrorMessage = TEXT("Width and Height must be greater than zero.");
		return Result;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		Result.ErrorMessage = TEXT("Widget Blueprint not found.");
		return Result;
	}

	UUserWidget* WidgetInstance = CreateWidgetInstanceForBlueprint(WidgetBP);
	if (!WidgetInstance)
	{
		Result.ErrorMessage = TEXT("Failed to create widget preview instance.");
		return Result;
	}

	TSharedRef<SWidget> SlateWidget = WidgetInstance->TakeWidget();
	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(FVector2D(Width, Height), TF_Bilinear, true);
	if (!RenderTarget)
	{
		Result.ErrorMessage = TEXT("Failed to create preview render target.");
		return Result;
	}

	FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(true);
	WidgetRenderer->SetIsPrepassNeeded(false);
	WidgetRenderer->DrawWidget(RenderTarget, SlateWidget, FVector2D(Width, Height), 0.0f);
	BeginCleanup(WidgetRenderer);

	const FString OutputDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WidgetPreviews")));
	IFileManager::Get().MakeDirectory(*OutputDir, true);
	const FString OutputPath = FPaths::Combine(OutputDir, WidgetBP->GetName() + TEXT(".png"));

	TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(*OutputPath));
	if (!Archive)
	{
		Result.ErrorMessage = TEXT("Failed to create preview output file.");
		return Result;
	}

	if (!FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, *Archive))
	{
		Result.ErrorMessage = TEXT("Failed to export preview PNG.");
		return Result;
	}

	Archive.Reset();
	Result.bSuccess = true;
	Result.OutputPath = OutputPath;
	return Result;
}

bool UWidgetService::StartPIE()
{
	if (!GEditor)
	{
		return false;
	}

	if (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
	{
		return true;
	}

	FRequestPlaySessionParams SessionParams;
	GEditor->RequestPlaySession(SessionParams);
	return true;
}

bool UWidgetService::StopPIE()
{
	if (!GEditor)
	{
		return false;
	}

	if (GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
	{
		for (TPair<FString, TWeakObjectPtr<UUserWidget>>& Pair : GPIEWidgetInstances)
		{
			if (Pair.Value.IsValid())
			{
				Pair.Value->RemoveFromParent();
			}
		}
		GPIEWidgetInstances.Empty();
		GEditor->RequestEndPlayMap();
	}

	return true;
}

bool UWidgetService::IsPIERunning()
{
	return GEditor && (GEditor->PlayWorld != nullptr || GEditor->bIsSimulatingInEditor);
}

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

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
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

FString UWidgetService::GetLiveProperty(
	const FPIEWidgetHandle& Handle,
	const FString& ComponentName,
	const FString& PropertyName)
{
	const TWeakObjectPtr<UUserWidget>* WidgetPtr = GPIEWidgetInstances.Find(Handle.InstanceId);
	if (!WidgetPtr || !WidgetPtr->IsValid())
	{
		return FString();
	}

	UWidget* Widget = FindRuntimeWidgetByName(WidgetPtr->Get(), ComponentName);
	if (!Widget)
	{
		return FString();
	}

	FResolvedWidgetProperty Resolved;
	if (!ResolveWidgetProperty(Widget, PropertyName, Resolved))
	{
		return FString();
	}

	FString ValueText;
	Resolved.Property->ExportTextItem_Direct(ValueText, Resolved.ValuePtr, nullptr, Resolved.TargetObject, PPF_None);
	return ValueText;
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

TArray<FWidgetEventInfo> UWidgetService::GetAvailableEvents(
	const FString& WidgetPath,
	const FString& ComponentName,
	const FString& WidgetType)
{
	TArray<FWidgetEventInfo> Events;

	// Get the class to inspect
	UClass* WidgetClass = nullptr;

	if (!WidgetType.IsEmpty())
	{
		TSubclassOf<UWidget> TypeClass = FindWidgetClass(WidgetType);
		if (TypeClass)
		{
			WidgetClass = *TypeClass;
		}
	}
	else if (!ComponentName.IsEmpty() && !WidgetPath.IsEmpty())
	{
		UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
		if (WidgetBP)
		{
			UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
			if (Widget)
			{
				WidgetClass = Widget->GetClass();
			}
		}
	}

	if (!WidgetClass)
	{
		// Default to base widget events
		WidgetClass = UWidget::StaticClass();
	}

	// Find multicast delegate properties (these are events)
	for (TFieldIterator<FMulticastDelegateProperty> It(WidgetClass); It; ++It)
	{
		FMulticastDelegateProperty* DelegateProp = *It;
		if (!DelegateProp)
		{
			continue;
		}

		FWidgetEventInfo Info;
		Info.EventName = DelegateProp->GetName();
		Info.EventType = TEXT("MulticastDelegate");
		
		// Get description from metadata
		Info.Description = DelegateProp->GetMetaData(TEXT("ToolTip"));
		if (Info.Description.IsEmpty())
		{
			Info.Description = FString::Printf(TEXT("Event: %s"), *Info.EventName);
		}

		Events.Add(Info);
	}

	// Add common widget events manually if not found
	if (Events.Num() == 0)
	{
		// Button-specific events
		if (WidgetType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnClicked"), TEXT("MulticastDelegate"), TEXT("Called when the button is clicked") });
			Events.Add({ TEXT("OnPressed"), TEXT("MulticastDelegate"), TEXT("Called when the button is pressed") });
			Events.Add({ TEXT("OnReleased"), TEXT("MulticastDelegate"), TEXT("Called when the button is released") });
			Events.Add({ TEXT("OnHovered"), TEXT("MulticastDelegate"), TEXT("Called when the button is hovered") });
			Events.Add({ TEXT("OnUnhovered"), TEXT("MulticastDelegate"), TEXT("Called when hover ends") });
		}
		// Slider events
		else if (WidgetType.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnValueChanged"), TEXT("MulticastDelegate"), TEXT("Called when the slider value changes") });
		}
		// CheckBox events
		else if (WidgetType.Equals(TEXT("CheckBox"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnCheckStateChanged"), TEXT("MulticastDelegate"), TEXT("Called when check state changes") });
		}
		else if (WidgetType.Equals(TEXT("ComboBoxString"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnSelectionChanged"), TEXT("MulticastDelegate"), TEXT("Called when selection changes") });
			Events.Add({ TEXT("OnOpening"), TEXT("MulticastDelegate"), TEXT("Called when the combo box opens") });
		}
		else if (WidgetType.Equals(TEXT("SpinBox"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnValueChanged"), TEXT("MulticastDelegate"), TEXT("Called when the spin box value changes") });
			Events.Add({ TEXT("OnValueCommitted"), TEXT("MulticastDelegate"), TEXT("Called when the value is committed") });
		}
		else if (WidgetType.Equals(TEXT("ExpandableArea"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnExpansionChanged"), TEXT("MulticastDelegate"), TEXT("Called when expansion state changes") });
		}
		else if (WidgetType.Equals(TEXT("InputKeySelector"), ESearchCase::IgnoreCase))
		{
			Events.Add({ TEXT("OnKeySelected"), TEXT("MulticastDelegate"), TEXT("Called when a key is selected") });
			Events.Add({ TEXT("OnIsSelectingKeyChanged"), TEXT("MulticastDelegate"), TEXT("Called when key selection mode changes") });
		}
	}

	return Events;
}

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

	// Note: Full event binding requires complex Blueprint graph manipulation
	// This is a simplified implementation that logs the binding request
	// For full implementation, use the Blueprint function service

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::BindEvent: Binding request - Widget: %s, Event: %s -> Function: %s"), *WidgetName, *EventName, *FunctionName);

	// Mark as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);

	return true;
}

bool UWidgetService::RenameWidget(
	const FString& WidgetPath,
	const FString& OldName,
	const FString& NewName)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	if (NewName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: NewName must not be empty"));
		return false;
	}

	// Find the widget by old name
	UWidget* TargetWidget = nullptr;
	TArray<UWidget*> AllWidgets;
	WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget->GetName() == OldName)
		{
			TargetWidget = Widget;
			break;
		}
	}

	if (!TargetWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: Widget '%s' not found in '%s'"), *OldName, *WidgetPath);
		return false;
	}

	// Check for name collision
	for (UWidget* Widget : AllWidgets)
	{
		if (Widget && Widget != TargetWidget && Widget->GetName() == NewName)
		{
			UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RenameWidget: A widget named '%s' already exists in '%s'"), *NewName, *WidgetPath);
			return false;
		}
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->Modify();
	TargetWidget->Modify();

	const FName NewFName(*NewName);
	TargetWidget->Rename(*NewName, TargetWidget->GetOuter(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::RenameWidget: Renamed '%s' to '%s' in '%s'"), *OldName, *NewName, *WidgetPath);
	return true;
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

TArray<FWidgetViewModelInfo> UWidgetService::ListViewModels(const FString& WidgetPath)
{
	TArray<FWidgetViewModelInfo> Results;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Results;
	}

	// Get the MVVM extension (don't create if not present)
	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		return Results;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		return Results;
	}

	for (const FMVVMBlueprintViewModelContext& VMContext : View->GetViewModels())
	{
		FWidgetViewModelInfo Info;
		Info.ViewModelName = VMContext.GetViewModelName().ToString();
		Info.ViewModelId = VMContext.GetViewModelId().ToString();

		if (UClass* VMClass = VMContext.GetViewModelClass())
		{
			Info.ViewModelClassName = VMClass->GetName();
		}

		switch (VMContext.CreationType)
		{
		case EMVVMBlueprintViewModelContextCreationType::Manual:
			Info.CreationType = TEXT("Manual");
			break;
		case EMVVMBlueprintViewModelContextCreationType::CreateInstance:
			Info.CreationType = TEXT("CreateInstance");
			break;
		case EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection:
			Info.CreationType = TEXT("GlobalViewModelCollection");
			break;
		case EMVVMBlueprintViewModelContextCreationType::PropertyPath:
			Info.CreationType = TEXT("PropertyPath");
			break;
		case EMVVMBlueprintViewModelContextCreationType::Resolver:
			Info.CreationType = TEXT("Resolver");
			break;
		}

		Results.Add(Info);
	}

	return Results;
}

bool UWidgetService::AddViewModel(
	const FString& WidgetPath,
	const FString& ViewModelClassName,
	const FString& ViewModelName,
	const FString& CreationType)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	// Find the ViewModel class
	UClass* VMClass = FindViewModelClass(ViewModelClassName);
	if (!VMClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: ViewModel class '%s' not found"), *ViewModelClassName);
		return false;
	}

	// Get or create the MVVM view
	UMVVMBlueprintView* View = GetOrCreateMVVMView(WidgetBP);
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: Failed to get/create MVVM view"));
		return false;
	}

	// Check if a ViewModel with this name already exists
	if (View->FindViewModel(FName(*ViewModelName)) != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModel: ViewModel '%s' already exists"), *ViewModelName);
		return false;
	}

	// Create the ViewModel context
	FMVVMBlueprintViewModelContext NewContext(VMClass, FName(*ViewModelName));

	// Set creation type
	if (CreationType.Equals(TEXT("Manual"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::Manual;
	}
	else if (CreationType.Equals(TEXT("CreateInstance"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;
	}
	else if (CreationType.Equals(TEXT("GlobalViewModelCollection"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection;
	}
	else if (CreationType.Equals(TEXT("PropertyPath"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::PropertyPath;
	}
	else if (CreationType.Equals(TEXT("Resolver"), ESearchCase::IgnoreCase))
	{
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::Resolver;
	}
	else
	{
		// Default to CreateInstance
		NewContext.CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;
	}

	View->AddViewModel(NewContext);

	// Mark as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::AddViewModel: Added ViewModel '%s' (class: %s) to '%s'"),
		*ViewModelName, *ViewModelClassName, *WidgetPath);

	return true;
}

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

TArray<FWidgetViewModelBindingInfo> UWidgetService::ListViewModelBindings(const FString& WidgetPath)
{
	TArray<FWidgetViewModelBindingInfo> Results;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return Results;
	}

	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		return Results;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		return Results;
	}

	UClass* SelfContext = WidgetBP->GeneratedClass;

	for (int32 i = 0; i < View->GetNumBindings(); ++i)
	{
		FMVVMBlueprintViewBinding* Binding = View->GetBindingAt(i);
		if (!Binding)
		{
			continue;
		}

		FWidgetViewModelBindingInfo Info;
		Info.BindingIndex = i;
		Info.BindingMode = BindingModeToString(Binding->BindingType);
		Info.bEnabled = Binding->bEnabled;
		Info.BindingId = Binding->BindingId.ToString();

		// Get display strings for source and destination paths
		Info.SourcePath = Binding->SourcePath.GetPropertyPath(SelfContext);
		Info.DestinationPath = Binding->DestinationPath.GetPropertyPath(SelfContext);

		Results.Add(Info);
	}

	return Results;
}

bool UWidgetService::AddViewModelBinding(
	const FString& WidgetPath,
	const FString& ViewModelName,
	const FString& ViewModelProperty,
	const FString& WidgetName,
	const FString& WidgetProperty,
	const FString& BindingMode)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UMVVMBlueprintView* View = GetOrCreateMVVMView(WidgetBP);
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Failed to get/create MVVM view"));
		return false;
	}

	// Find the ViewModel context
	const FMVVMBlueprintViewModelContext* VMContext = View->FindViewModel(FName(*ViewModelName));
	if (!VMContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: ViewModel '%s' not found. Add it first with add_view_model."), *ViewModelName);
		return false;
	}

	UClass* VMClass = VMContext->GetViewModelClass();
	if (!VMClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: ViewModel class is invalid"));
		return false;
	}

	// Find the ViewModel property
	FProperty* VMProp = VMClass->FindPropertyByName(FName(*ViewModelProperty));
	if (!VMProp)
	{
		// Try case-insensitive
		for (TFieldIterator<FProperty> It(VMClass); It; ++It)
		{
			if (It->GetName().Equals(ViewModelProperty, ESearchCase::IgnoreCase))
			{
				VMProp = *It;
				break;
			}
		}
	}
	if (!VMProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Property '%s' not found on ViewModel class '%s'"),
			*ViewModelProperty, *VMClass->GetName());
		return false;
	}

	// Find the widget and its property
	UWidget* TargetWidget = FindWidgetByName(WidgetBP, WidgetName);
	if (!TargetWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Widget '%s' not found"), *WidgetName);
		return false;
	}

	FProperty* WidgetProp = TargetWidget->GetClass()->FindPropertyByName(FName(*WidgetProperty));
	if (!WidgetProp)
	{
		// Try case-insensitive
		for (TFieldIterator<FProperty> It(TargetWidget->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(WidgetProperty, ESearchCase::IgnoreCase))
			{
				WidgetProp = *It;
				break;
			}
		}
	}
	if (!WidgetProp)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::AddViewModelBinding: Property '%s' not found on widget '%s'"),
			*WidgetProperty, *WidgetName);
		return false;
	}

	// Create a new binding
	FMVVMBlueprintViewBinding& NewBinding = View->AddDefaultBinding();

	// Set binding mode
	NewBinding.BindingType = StringToBindingMode(BindingMode);

	// Set up source path (ViewModel property)
	NewBinding.SourcePath.SetViewModelId(VMContext->GetViewModelId());
	UE::MVVM::FMVVMConstFieldVariant SourceField(VMProp);
	NewBinding.SourcePath.SetPropertyPath(WidgetBP, SourceField);

	// Set up destination path (widget property)
	NewBinding.DestinationPath.SetWidgetName(FName(*WidgetName));
	UE::MVVM::FMVVMConstFieldVariant DestField(WidgetProp);
	NewBinding.DestinationPath.SetPropertyPath(WidgetBP, DestField);

	// Mark as modified
	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::AddViewModelBinding: Created binding %s.%s -> %s.%s (%s)"),
		*ViewModelName, *ViewModelProperty, *WidgetName, *WidgetProperty, *BindingMode);

	return true;
}

bool UWidgetService::RemoveViewModelBinding(
	const FString& WidgetPath,
	int32 BindingIndex)
{
	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: Widget Blueprint '%s' not found"), *WidgetPath);
		return false;
	}

	UMVVMWidgetBlueprintExtension_View* ViewExtension = FindMVVMExtension(WidgetBP);
	if (!ViewExtension)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: No MVVM extension found"));
		return false;
	}

	UMVVMBlueprintView* View = ViewExtension->GetBlueprintView();
	if (!View)
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: No MVVM view found"));
		return false;
	}

	if (BindingIndex < 0 || BindingIndex >= View->GetNumBindings())
	{
		UE_LOG(LogTemp, Warning, TEXT("UWidgetService::RemoveViewModelBinding: Index %d out of range (0-%d)"),
			BindingIndex, View->GetNumBindings() - 1);
		return false;
	}

	View->RemoveBindingAt(BindingIndex);

	WidgetBP->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

	UE_LOG(LogTemp, Log, TEXT("UWidgetService::RemoveViewModelBinding: Removed binding at index %d"), BindingIndex);

	return true;
}

// =================================================================
// Existence Checks
// =================================================================

bool UWidgetService::WidgetBlueprintExists(const FString& WidgetPath)
{
	if (WidgetPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(WidgetPath);
}

bool UWidgetService::WidgetExists(const FString& WidgetPath, const FString& ComponentName)
{
	if (WidgetPath.IsEmpty() || ComponentName.IsEmpty())
	{
		return false;
	}

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP)
	{
		return false;
	}

	return FindWidgetByName(WidgetBP, ComponentName) != nullptr;
}

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

TArray<FWidgetComponentSnapshot> UWidgetService::GetWidgetSnapshot(const FString& WidgetPath)
{
	TArray<FWidgetComponentSnapshot> Snapshots;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Snapshots;
	}

	UWidget* RootWidget = WidgetBP->WidgetTree->RootWidget;
	if (!RootWidget)
	{
		return Snapshots;
	}

	TFunction<void(UWidget*, const FString&)> BuildSnapshot;
	BuildSnapshot = [&](UWidget* Widget, const FString& ParentName)
	{
		if (!Widget)
		{
			return;
		}

		FWidgetComponentSnapshot Snapshot;
		Snapshot.WidgetName = Widget->GetName();
		Snapshot.WidgetClass = Widget->GetClass()->GetName();
		Snapshot.ParentWidget = ParentName;
		Snapshot.bIsRootWidget = (Widget == RootWidget);
		Snapshot.bIsVariable = Widget->bIsVariable;
		Snapshot.SlotInfo = BuildSlotInfo(Widget);
		Snapshot.Properties = CollectWidgetProperties(Widget);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				if (UWidget* Child = Panel->GetChildAt(i))
				{
					Snapshot.Children.Add(Child->GetName());
				}
			}
		}

		Snapshots.Add(Snapshot);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				BuildSnapshot(Panel->GetChildAt(i), Snapshot.WidgetName);
			}
		}
	};

	BuildSnapshot(RootWidget, FString());

	return Snapshots;
}

FWidgetComponentSnapshot UWidgetService::GetComponentSnapshot(const FString& WidgetPath, const FString& ComponentName)
{
	FWidgetComponentSnapshot Snapshot;

	UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return Snapshot;
	}

	UWidget* Widget = FindWidgetByName(WidgetBP, ComponentName);
	if (!Widget)
	{
		return Snapshot;
	}

	UWidget* RootWidget = WidgetBP->WidgetTree->RootWidget;
	Snapshot.WidgetName = Widget->GetName();
	Snapshot.WidgetClass = Widget->GetClass()->GetName();
	Snapshot.bIsRootWidget = (Widget == RootWidget);
	Snapshot.bIsVariable = Widget->bIsVariable;
	Snapshot.SlotInfo = BuildSlotInfo(Widget);
	Snapshot.Properties = CollectWidgetProperties(Widget);

	if (Widget->Slot && Widget->Slot->Parent)
	{
		Snapshot.ParentWidget = Widget->Slot->Parent->GetName();
	}

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = Panel->GetChildAt(i))
			{
				Snapshot.Children.Add(Child->GetName());
			}
		}
	}

	return Snapshot;
}