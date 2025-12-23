// Copyright Kevin Buckley 2025 All Rights Reserved.

/**
 * @file WidgetPropertyService.cpp
 * @brief Implementation of widget property management functionality
 * 
 * This service provides property get/set operations for widgets,
 * extracted from UMGCommands.cpp as part of Phase 4 refactoring.
 */

#include "Services/UMG/WidgetPropertyService.h"
#include "Core/ErrorCodes.h"
#include "Core/JsonValueHelper.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "EditorSubsystem.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Core/ServiceContext.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetPropertyService, Log, All);

namespace
{
struct FPathSegment
{
    FString Name;
    bool bHasIndex = false;
    int32 Index = INDEX_NONE;
    bool bHasKey = false;
    FString Key;
};

static bool ParsePropertyPath(const FString& InPath, bool& bSlotRoot, TArray<FPathSegment>& Out)
{
    bSlotRoot = false;
    Out.Reset();

    TArray<FString> Parts;
    InPath.ParseIntoArray(Parts, TEXT("."), true);
    if (Parts.Num() == 0)
    {
        return false;
    }

    int32 Start = 0;
    if (Parts[0].Equals(TEXT("Slot"), ESearchCase::IgnoreCase))
    {
        bSlotRoot = true;
        Start = 1;
        if (Parts.Num() == 1)
        {
            return false;
        }
    }

    for (int32 Index = Start; Index < Parts.Num(); ++Index)
    {
        FPathSegment Segment;
        const FString& Token = Parts[Index];

        int32 BracketIdx;
        if (Token.FindChar('[', BracketIdx) && Token.EndsWith("]"))
        {
            Segment.Name = Token.Left(BracketIdx);
            const FString Inside = Token.Mid(BracketIdx + 1, Token.Len() - BracketIdx - 2);
            if (Inside.IsNumeric())
            {
                Segment.bHasIndex = true;
                Segment.Index = FCString::Atoi(*Inside);
            }
            else
            {
                Segment.bHasKey = true;
                Segment.Key = Inside;
            }
        }
        else
        {
            Segment.Name = Token;
        }

        Out.Add(Segment);
    }

    return Out.Num() > 0;
}

struct FResolvedTarget
{
    UObject* RootObject = nullptr;
    void* ContainerPtr = nullptr;
    FProperty* Property = nullptr;
    bool bIsSyntheticChildOrder = false;
    
    // Canvas slot virtual properties - these use getter/setter methods, not UPROPERTYs
    bool bIsCanvasSlotVirtualProperty = false;
    FString CanvasSlotVirtualPropertyName;
};

// Check if the property name is a canvas slot virtual property (uses getter/setter, not reflection)
static bool IsCanvasSlotVirtualProperty(const FString& PropertyName)
{
    static const TArray<FString> VirtualProperties = {
        TEXT("alignment"),
        TEXT("anchors"),
        TEXT("position"),
        TEXT("size"),
        TEXT("auto_size"),
        TEXT("autosize"),
        TEXT("z_order"),
        TEXT("zorder")
    };
    
    for (const FString& VP : VirtualProperties)
    {
        if (PropertyName.Equals(VP, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }
    return false;
}

static bool ResolvePath(UWidget* Widget, const TArray<FPathSegment>& Segments, bool bSlotRoot, FResolvedTarget& Out, FString& Error)
{
    UObject* CurrentObject = bSlotRoot ? static_cast<UObject*>(Widget->Slot) : static_cast<UObject*>(Widget);
    void* CurrentPtr = CurrentObject;
    FProperty* CurrentProperty = nullptr;

    if (!CurrentObject)
    {
        Error = TEXT("Slot is null for this widget (no parent panel)");
        return false;
    }

    for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
    {
        const FPathSegment& Segment = Segments[SegmentIndex];

        if (SegmentIndex == Segments.Num() - 1 && bSlotRoot && Segment.Name.Equals(TEXT("ChildOrder"), ESearchCase::IgnoreCase))
        {
            Out.RootObject = CurrentObject;
            Out.ContainerPtr = CurrentPtr;
            Out.Property = nullptr;
            Out.bIsSyntheticChildOrder = true;
            return true;
        }

        if (UObject* AsObject = Cast<UObject>(CurrentObject))
        {
            CurrentProperty = AsObject->GetClass()->FindPropertyByName(*Segment.Name);
        }
        else if (CurrentProperty && CurrentProperty->IsA<FStructProperty>())
        {
            FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(CurrentProperty);
            CurrentProperty = StructProperty->Struct->FindPropertyByName(*Segment.Name);
        }
        else
        {
            return false;
        }

        if (!CurrentProperty)
        {
            if (!bSlotRoot && Segment.Name.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
            {
                CurrentProperty = Widget->GetClass()->FindPropertyByName(TEXT("bIsVariable"));
            }
        }

        // Check for canvas slot virtual properties (alignment, anchors, position, size, etc.)
        // These are accessed via getter/setter methods, not as UPROPERTYs
        if (!CurrentProperty && bSlotRoot && SegmentIndex == 0 && Widget->Slot)
        {
            UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
            if (CanvasSlot && IsCanvasSlotVirtualProperty(Segment.Name))
            {
                // Mark this as a canvas slot virtual property for special handling
                Out.RootObject = Widget->Slot;
                Out.ContainerPtr = Widget->Slot;
                Out.Property = nullptr;
                Out.bIsCanvasSlotVirtualProperty = true;
                Out.CanvasSlotVirtualPropertyName = Segment.Name;
                return true;
            }
        }

        if (!CurrentProperty)
        {
            // Provide more helpful error for slot properties that vary by slot type
            if (bSlotRoot && SegmentIndex == 0 && Widget->Slot)
            {
                const FString SlotClassName = Widget->Slot->GetClass()->GetName();
                
                // Check for common alignment property mistakes
                if (Segment.Name.Equals(TEXT("HorizontalAlignment"), ESearchCase::IgnoreCase) ||
                    Segment.Name.Equals(TEXT("VerticalAlignment"), ESearchCase::IgnoreCase))
                {
                    if (SlotClassName.Contains(TEXT("CanvasPanelSlot")))
                    {
                        Error = FString::Printf(TEXT("Property '%s' not found on %s. For CanvasPanel slot properties, use Slot.alignment ([x,y] or 'center'), Slot.anchors ('fill' or object), Slot.position, Slot.size."), 
                            *Segment.Name, *SlotClassName);
                    }
                    else
                    {
                        Error = FString::Printf(TEXT("Property '%s' not found on %s. Use Slot.horizontal_alignment or Slot.vertical_alignment for Box/Overlay panels."), 
                            *Segment.Name, *SlotClassName);
                    }
                }
                else
                {
                    Error = FString::Printf(TEXT("Property '%s' not found on slot type %s. For CanvasPanel use: Slot.alignment, Slot.anchors, Slot.position, Slot.size. For Box/Overlay use: Slot.horizontal_alignment, Slot.vertical_alignment."), *Segment.Name, *SlotClassName);
                }
            }
            else
            {
                Error = FString::Printf(TEXT("Property '%s' not found"), *Segment.Name);
            }
            return false;
        }

        if (FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty))
        {
            CurrentPtr = StructProperty->ContainerPtrToValuePtr<void>(CurrentPtr);
            CurrentObject = nullptr;
        }
        else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(CurrentProperty))
        {
            UObject* const* ObjectPtr = ObjectProperty->ContainerPtrToValuePtr<UObject*>(CurrentPtr);
            CurrentObject = *ObjectPtr;
            CurrentPtr = CurrentObject;
        }
        else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
        {
            void* ArrayPtr = ArrayProperty->ContainerPtrToValuePtr<void>(CurrentPtr);
            FScriptArrayHelper Helper(ArrayProperty, ArrayPtr);

            if (Segment.bHasIndex)
            {
                if (!Helper.IsValidIndex(Segment.Index))
                {
                    Error = FString::Printf(TEXT("Array index out of bounds: %d (len=%d)"), Segment.Index, Helper.Num());
                    return false;
                }

                CurrentPtr = Helper.GetRawPtr(Segment.Index);
                CurrentProperty = ArrayProperty->Inner;
            }
            else
            {
                if (SegmentIndex != Segments.Num() - 1)
                {
                    Error = TEXT("Array path must specify index to access elements");
                    return false;
                }
            }
        }
        else if (FMapProperty* MapProperty = CastField<FMapProperty>(CurrentProperty))
        {
            void* MapPtr = MapProperty->ContainerPtrToValuePtr<void>(CurrentPtr);
            FScriptMapHelper Helper(MapProperty, MapPtr);

            if (Segment.bHasKey)
            {
                TArray<uint8> KeyStorage;
                KeyStorage.SetNumUninitialized(MapProperty->KeyProp->GetSize());
                MapProperty->KeyProp->InitializeValue(KeyStorage.GetData());

                if (FNameProperty* NameKey = CastField<FNameProperty>(MapProperty->KeyProp))
                {
                    const FName ValueName(*Segment.Key);
                    NameKey->CopyCompleteValue(KeyStorage.GetData(), &ValueName);
                }
                else if (FStrProperty* StringKey = CastField<FStrProperty>(MapProperty->KeyProp))
                {
                    const FString ValueString = Segment.Key;
                    StringKey->CopyCompleteValue(KeyStorage.GetData(), &ValueString);
                }
                else if (FIntProperty* IntKey = CastField<FIntProperty>(MapProperty->KeyProp))
                {
                    const int32 ValueInt = FCString::Atoi(*Segment.Key);
                    IntKey->CopyCompleteValue(KeyStorage.GetData(), &ValueInt);
                }
                else if (FByteProperty* ByteKey = CastField<FByteProperty>(MapProperty->KeyProp))
                {
                    uint8 ValueByte = 0;
                    if (ByteKey->Enum)
                    {
                        const int64 EnumVal = ByteKey->Enum->GetValueByNameString(Segment.Key);
                        ValueByte = EnumVal == INDEX_NONE ? static_cast<uint8>(FCString::Atoi(*Segment.Key)) : static_cast<uint8>(EnumVal);
                    }
                    else
                    {
                        ValueByte = static_cast<uint8>(FCString::Atoi(*Segment.Key));
                    }
                    ByteKey->CopyCompleteValue(KeyStorage.GetData(), &ValueByte);
                }
                else
                {
                    Error = TEXT("Unsupported map key type");
                    return false;
                }

                int32 FoundIndex = INDEX_NONE;
                for (int32 It = 0; It < Helper.GetMaxIndex(); ++It)
                {
                    if (!Helper.IsValidIndex(It))
                    {
                        continue;
                    }

                    uint8* Pair = static_cast<uint8*>(Helper.GetPairPtr(It));
                    void* ExistingKeyPtr = Pair;
                    if (MapProperty->KeyProp->Identical(KeyStorage.GetData(), ExistingKeyPtr))
                    {
                        FoundIndex = It;
                        break;
                    }
                }

                if (FoundIndex == INDEX_NONE)
                {
                    Error = TEXT("Map key not found");
                    return false;
                }

                CurrentPtr = Helper.GetPairPtr(FoundIndex) + MapProperty->MapLayout.ValueOffset;
                CurrentProperty = MapProperty->ValueProp;
            }
            else if (SegmentIndex != Segments.Num() - 1)
            {
                Error = TEXT("Map path must specify [Key] to access value");
                return false;
            }
        }
        else if (FSetProperty* SetProperty = CastField<FSetProperty>(CurrentProperty))
        {
            if (SegmentIndex != Segments.Num() - 1)
            {
                Error = TEXT("Set path cannot traverse into elements; use collection_op");
                return false;
            }
        }
        else
        {
            if (SegmentIndex != Segments.Num() - 1)
            {
                Error = FString::Printf(TEXT("Cannot traverse into non-composite property '%s'"), *Segment.Name);
                return false;
            }
        }
    }

    Out.RootObject = Cast<UObject>(CurrentObject) ? Cast<UObject>(CurrentObject) : Widget;
    Out.ContainerPtr = CurrentPtr;
    Out.Property = CurrentProperty;
    return true;
}

static bool ParseComplexPropertyValue(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, UWidget* Widget, FString& ErrorMessage)
{
    if (!JsonValue.IsValid() || !Property || !Widget)
    {
        ErrorMessage = TEXT("Invalid parameters for property parsing");
        return false;
    }

    if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
        {
            FLinearColor ColorValue;

            // Use FJsonValueHelper to handle all formats (object, array, string-encoded, hex, named colors)
            if (FJsonValueHelper::TryGetLinearColor(JsonValue, ColorValue))
            {
                Property->SetValue_InContainer(Widget, &ColorValue);
                return true;
            }
            else
            {
                ErrorMessage = TEXT("LinearColor must be object {R,G,B,A}, array [R,G,B,A], hex string #RRGGBB, or color name");
                return false;
            }
        }
        else if (StructProperty->Struct == TBaseStructure<FSlateColor>::Get())
        {
            FLinearColor LinearColor;

            // Use FJsonValueHelper to handle all formats
            if (FJsonValueHelper::TryGetLinearColor(JsonValue, LinearColor))
            {
                FSlateColor SlateColorValue = FSlateColor(LinearColor);
                Property->SetValue_InContainer(Widget, &SlateColorValue);
                return true;
            }
            // If no explicit color provided, leave as default
            return true;
        }
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
        else if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
        {
            FVector2D VectorValue;

            // Use FJsonValueHelper to handle all formats (object, array, string-encoded)
            if (FJsonValueHelper::TryGetVector2D(JsonValue, VectorValue))
            {
                Property->SetValue_InContainer(Widget, &VectorValue);
                return true;
            }
            else
            {
                ErrorMessage = TEXT("Vector2D must be object {X, Y} or array [X, Y]");
                return false;
            }
        }
        else if (StructProperty->Struct->GetName().Contains(TEXT("SlateBrush")))
        {
            if (JsonValue->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject> BrushObj = JsonValue->AsObject();
                void* BrushPtr = StructProperty->ContainerPtrToValuePtr<void>(Widget);
                if (!BrushPtr)
                {
                    ErrorMessage = TEXT("Unable to access SlateBrush instance");
                    return false;
                }

                FSlateBrush* SlateBrush = static_cast<FSlateBrush*>(BrushPtr);
                bool bModified = false;

                if (BrushObj->HasField(TEXT("ResourceObject")))
                {
                    const FString ResourcePath = BrushObj->GetStringField(TEXT("ResourceObject"));
                    if (!ResourcePath.IsEmpty())
                    {
                        if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ResourcePath))
                        {
                            SlateBrush->SetResourceObject(Texture);
                            bModified = true;
                        }
                        else
                        {
                            UE_LOG(LogWidgetPropertyService, Warning, TEXT("Failed to load texture '%s' for SlateBrush"), *ResourcePath);
                        }
                    }
                }

                if (BrushObj->HasField(TEXT("DrawAs")))
                {
                    const FString DrawAsStr = BrushObj->GetStringField(TEXT("DrawAs"));
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

                if (BrushObj->HasField(TEXT("Tiling")))
                {
                    const FString TilingStr = BrushObj->GetStringField(TEXT("Tiling"));
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

                if (BrushObj->HasField(TEXT("TintColor")))
                {
                    FLinearColor TintColor;
                    // Use FJsonValueHelper for robust color parsing (arrays, objects, hex, named colors)
                    if (FJsonValueHelper::TryGetLinearColorField(BrushObj, TEXT("TintColor"), TintColor))
                    {
                        SlateBrush->TintColor = FSlateColor(TintColor);
                        bModified = true;
                    }
                }

                if (bModified)
                {
                    if (UImage* ImageWidget = Cast<UImage>(Widget))
                    {
                        ImageWidget->SetBrush(*SlateBrush);
                    }
                    return true;
                }

                ErrorMessage = TEXT("SlateBrush JSON did not modify any fields");
                return false;
            }

            ErrorMessage = TEXT("Invalid SlateBrush JSON format - expected object with ResourceObject, DrawAs, Tiling, and/or TintColor");
            return false;
        }
        else if (StructProperty->Struct->GetName().Contains(TEXT("ButtonStyle")))
        {
            if (JsonValue->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject> StyleObj = JsonValue->AsObject();
                void* StylePtr = StructProperty->ContainerPtrToValuePtr<void>(Widget);
                if (!StylePtr)
                {
                    ErrorMessage = TEXT("Unable to access ButtonStyle instance");
                    return false;
                }

                bool bModified = false;

                // Helper lambda to set TintColor on a button state (Normal/Hovered/Pressed)
                auto TrySetStateTintColor = [&](const TCHAR* StateName) -> bool
                {
                    if (!StyleObj->HasField(StateName))
                    {
                        return false;
                    }
                    
                    const TSharedPtr<FJsonObject> StateObj = StyleObj->GetObjectField(StateName);
                    FLinearColor TintColor;
                    
                    // Use FJsonValueHelper for robust color parsing (arrays, objects, hex, named colors)
                    if (!FJsonValueHelper::TryGetLinearColorField(StateObj, TEXT("TintColor"), TintColor))
                    {
                        return false;
                    }
                    
                    if (FProperty* StateProp = StructProperty->Struct->FindPropertyByName(StateName))
                    {
                        if (FStructProperty* StateStructProp = CastField<FStructProperty>(StateProp))
                        {
                            void* StatePtr = StateStructProp->ContainerPtrToValuePtr<void>(StylePtr);
                            if (FProperty* TintProp = StateStructProp->Struct->FindPropertyByName(TEXT("TintColor")))
                            {
                                TintProp->SetValue_InContainer(StatePtr, &TintColor);
                                return true;
                            }
                        }
                    }
                    return false;
                };

                bModified |= TrySetStateTintColor(TEXT("Normal"));
                bModified |= TrySetStateTintColor(TEXT("Hovered"));
                bModified |= TrySetStateTintColor(TEXT("Pressed"));

                if (bModified)
                {
                    return true;
                }

                ErrorMessage = TEXT("ButtonStyle JSON did not modify any fields");
                return false;
            }

            ErrorMessage = TEXT("ButtonStyle requires object with Normal/Hovered/Pressed states containing TintColor arrays");
            return false;
        }
    }

    ErrorMessage = FString::Printf(TEXT("Unsupported complex property type: %s"), *Property->GetClass()->GetName());
    return false;
}

static void AddEnumConstraints(FProperty* Property, TSharedPtr<FJsonObject>& Constraints)
{
    if (!Property)
    {
        return;
    }

    if (!Constraints.IsValid())
    {
        Constraints = MakeShareable(new FJsonObject);
    }

    if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
    {
        if (UEnum* Enum = ByteProperty->Enum)
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
            {
                const FString Name = Enum->GetNameStringByIndex(Index);
                if (!Name.EndsWith(TEXT("_MAX")))
                {
                    Values.Add(MakeShared<FJsonValueString>(Name));
                }
            }
            Constraints->SetArrayField(TEXT("enum_values"), Values);
        }
    }
    else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        if (UEnum* Enum = EnumProperty->GetEnum())
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
            {
                const FString Name = Enum->GetNameStringByIndex(Index);
                if (!Name.EndsWith(TEXT("_MAX")))
                {
                    Values.Add(MakeShared<FJsonValueString>(Name));
                }
            }
            Constraints->SetArrayField(TEXT("enum_values"), Values);
        }
    }
}

static void AddNumericConstraints(FProperty* Property, TSharedPtr<FJsonObject>& Constraints)
{
    if (!Property)
    {
        return;
    }

    if (!Constraints.IsValid())
    {
        Constraints = MakeShareable(new FJsonObject);
    }

    auto TryNumberMeta = [&](const TCHAR* Key, const TCHAR* OutKey)
    {
        if (Property->HasMetaData(Key))
        {
            const FString MetaValue = Property->GetMetaData(Key);
            const double NumericValue = FCString::Atod(*MetaValue);
            Constraints->SetNumberField(OutKey, NumericValue);
        }
    };

    TryNumberMeta(TEXT("ClampMin"), TEXT("min"));
    TryNumberMeta(TEXT("ClampMax"), TEXT("max"));
    TryNumberMeta(TEXT("UIMin"), TEXT("uiMin"));
    TryNumberMeta(TEXT("UIMax"), TEXT("uiMax"));
}

static void AppendCollectionLength(FProperty* Property, void* ContainerPtr, TSharedPtr<FJsonObject>& Constraints)
{
    if (!Property || !ContainerPtr)
    {
        return;
    }

    if (!Constraints.IsValid())
    {
        Constraints = MakeShareable(new FJsonObject);
    }

    if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        void* ArrayAddr = ArrayProperty->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptArrayHelper Helper(ArrayProperty, ArrayAddr);
        Constraints->SetNumberField(TEXT("length"), Helper.Num());
    }
    else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
    {
        void* SetAddr = SetProperty->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptSetHelper Helper(SetProperty, SetAddr);
        Constraints->SetNumberField(TEXT("length"), Helper.Num());
    }
    else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
    {
        void* MapAddr = MapProperty->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptMapHelper Helper(MapProperty, MapAddr);
        Constraints->SetNumberField(TEXT("length"), Helper.Num());
    }
}

static void AppendSchemaHints(FProperty* Property, TSharedPtr<FJsonObject>& Schema)
{
    if (!Property)
    {
        return;
    }

    if (!Schema.IsValid())
    {
        Schema = MakeShareable(new FJsonObject);
    }

    if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        TSharedPtr<FJsonObject> StructInfo = MakeShareable(new FJsonObject);
        StructInfo->SetStringField(TEXT("name"), StructProperty->Struct->GetName());
        Schema->SetObjectField(TEXT("struct"), StructInfo);
    }
    else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        TSharedPtr<FJsonObject> ArrayInfo = MakeShareable(new FJsonObject);
        ArrayInfo->SetStringField(TEXT("element_type"), ArrayProperty->Inner->GetClass()->GetName());
        Schema->SetObjectField(TEXT("array"), ArrayInfo);
    }
    else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
    {
        TSharedPtr<FJsonObject> SetInfo = MakeShareable(new FJsonObject);
        SetInfo->SetStringField(TEXT("element_type"), SetProperty->ElementProp->GetClass()->GetName());
        Schema->SetObjectField(TEXT("set"), SetInfo);
    }
    else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
    {
        TSharedPtr<FJsonObject> MapInfo = MakeShareable(new FJsonObject);
        MapInfo->SetStringField(TEXT("key_type"), MapProperty->KeyProp->GetClass()->GetName());
        MapInfo->SetStringField(TEXT("value_type"), MapProperty->ValueProp->GetClass()->GetName());
        Schema->SetObjectField(TEXT("map"), MapInfo);
    }
}
} // namespace

FWidgetPropertyService::FWidgetPropertyService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<FString> FWidgetPropertyService::GetWidgetProperty(UWidget* Widget, const FString& PropertyPath)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(PropertyPath, TEXT("PropertyPath"));
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Property '%s' not found on widget"), *PropertyPath)
        );
    }

    FString Value = PropertyValueToString(Property, Container);
    return TResult<FString>::Success(Value);
}

TResult<void> FWidgetPropertyService::SetWidgetProperty(UWidget* Widget, const FString& PropertyPath, const FString& Value)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    ValidationResult = ValidateNotEmpty(PropertyPath, TEXT("PropertyPath"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Property '%s' not found on widget"), *PropertyPath)
        );
    }

    if (!SetPropertyValueFromString(Property, Container, Value))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
            FString::Printf(TEXT("Failed to set property '%s' to value '%s'"), *PropertyPath, *Value)
        );
    }

    Widget->Modify();
    return TResult<void>::Success();
}

TResult<FWidgetPropertySetResult> FWidgetPropertyService::SetWidgetProperty(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FWidgetPropertySetRequest& Request)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertySetResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertySetResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(Request.PropertyPath, TEXT("PropertyPath"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertySetResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<FWidgetPropertySetResult>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* FoundWidget = nullptr;
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

    if (!FoundWidget)
    {
        return TResult<FWidgetPropertySetResult>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprint->GetName())
        );
    }

    FWidgetPropertySetResult ResultPayload;
    ResultPayload.PropertyPath = Request.PropertyPath;

    FProperty* Property = FoundWidget->GetClass()->FindPropertyByName(*Request.PropertyPath);
    void* ContainerPtrForSet = static_cast<void*>(FoundWidget);
    bool bUsedResolver = false;
    FResolvedTarget ResolvedTarget;

    if (!Property)
    {
        bool bSlotRoot = false;
        TArray<FPathSegment> Segments;
        if (!ParsePropertyPath(Request.PropertyPath, bSlotRoot, Segments))
        {
            // Provide helpful error message for common mistakes
            if (Request.PropertyPath.Equals(TEXT("Slot"), ESearchCase::IgnoreCase))
            {
                return TResult<FWidgetPropertySetResult>::Error(
                    VibeUE::ErrorCodes::PARAM_INVALID,
                    TEXT("Cannot access 'Slot' directly. Use 'Slot.PropertyName' format (e.g., 'Slot.Padding', 'Slot.HorizontalAlignment', 'Slot.ChildOrder').")
                );
            }
            return TResult<FWidgetPropertySetResult>::Error(
                VibeUE::ErrorCodes::PARAM_INVALID,
                TEXT("Invalid property_path")
            );
        }

        FString ResolveError;
        if (!ResolvePath(FoundWidget, Segments, bSlotRoot, ResolvedTarget, ResolveError))
        {
            return TResult<FWidgetPropertySetResult>::Error(
                VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
                ResolveError
            );
        }

        bUsedResolver = true;

        if (ResolvedTarget.bIsSyntheticChildOrder)
        {
            int32 DesiredIndex = INDEX_NONE;
            if (Request.Value.HasString())
            {
                DesiredIndex = FCString::Atoi(*Request.Value.StringValue);
            }
            else if (Request.Value.HasJson() && Request.Value.JsonValue->Type == EJson::Number)
            {
                DesiredIndex = static_cast<int32>(Request.Value.JsonValue->AsNumber());
            }
            else
            {
                return TResult<FWidgetPropertySetResult>::Error(
                    VibeUE::ErrorCodes::PARAM_TYPE_MISMATCH,
                    TEXT("ChildOrder requires integer value")
                );
            }

            UPanelSlot* Slot = Cast<UPanelSlot>(FoundWidget->Slot);
            if (!Slot || !Slot->Parent)
            {
                return TResult<FWidgetPropertySetResult>::Error(
                    VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
                    TEXT("Widget has no parent panel for ChildOrder")
                );
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
                if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
                {
                    TArray<IAssetEditorInstance*> AssetEditors = AssetSubsystem->FindEditorsForAsset(WidgetBlueprint);
                    for (IAssetEditorInstance* EditorInstance : AssetEditors)
                    {
                        if (FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(EditorInstance))
                        {
                            WidgetEditor->RefreshEditors();
                        }
                    }
                }
            }

            WidgetBlueprint->MarkPackageDirty();

            ResultPayload.AppliedValue.SetString(FString::FromInt(DesiredIndex));
            ResultPayload.bChildOrderUpdated = true;
            ResultPayload.ChildOrderValue = DesiredIndex;
            ResultPayload.Note = TEXT("ChildOrder updated");
            return TResult<FWidgetPropertySetResult>::Success(ResultPayload);
        }

        // Handle canvas slot virtual properties (alignment, anchors, position, size, etc.)
        if (ResolvedTarget.bIsCanvasSlotVirtualProperty)
        {
            UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(FoundWidget->Slot);
            if (!CanvasSlot)
            {
                return TResult<FWidgetPropertySetResult>::Error(
                    VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
                    TEXT("Widget is not in a CanvasPanel - cannot set canvas slot properties")
                );
            }

            const FString& PropName = ResolvedTarget.CanvasSlotVirtualPropertyName;
            
            // Parse the value based on property type
            if (PropName.Equals(TEXT("alignment"), ESearchCase::IgnoreCase))
            {
                // Alignment expects [x, y] array where 0.5, 0.5 = center
                if (Request.Value.HasJson() && Request.Value.JsonValue->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& Arr = Request.Value.JsonValue->AsArray();
                    if (Arr.Num() >= 2)
                    {
                        FVector2D Alignment(Arr[0]->AsNumber(), Arr[1]->AsNumber());
                        CanvasSlot->SetAlignment(Alignment);
                        ResultPayload.AppliedValue.SetString(FString::Printf(TEXT("[%f, %f]"), Alignment.X, Alignment.Y));
                    }
                }
                else if (Request.Value.HasString())
                {
                    // Support string shortcuts like "center"
                    FString AlignStr = Request.Value.StringValue.ToLower();
                    FVector2D Alignment(0.0, 0.0);
                    if (AlignStr.Contains(TEXT("center")))
                    {
                        Alignment = FVector2D(0.5, 0.5);
                    }
                    else if (AlignStr.Contains(TEXT("right")))
                    {
                        Alignment.X = 1.0;
                    }
                    else if (AlignStr.Contains(TEXT("bottom")))
                    {
                        Alignment.Y = 1.0;
                    }
                    CanvasSlot->SetAlignment(Alignment);
                    ResultPayload.AppliedValue.SetString(FString::Printf(TEXT("[%f, %f]"), Alignment.X, Alignment.Y));
                }
            }
            else if (PropName.Equals(TEXT("anchors"), ESearchCase::IgnoreCase))
            {
                // Anchors expects {min_x, min_y, max_x, max_y} or string like "fill", "center"
                FAnchors Anchors;
                if (Request.Value.HasJson() && Request.Value.JsonValue->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject>& Obj = Request.Value.JsonValue->AsObject();
                    Anchors.Minimum.X = Obj->GetNumberField(TEXT("min_x"));
                    Anchors.Minimum.Y = Obj->GetNumberField(TEXT("min_y"));
                    Anchors.Maximum.X = Obj->GetNumberField(TEXT("max_x"));
                    Anchors.Maximum.Y = Obj->GetNumberField(TEXT("max_y"));
                }
                else if (Request.Value.HasString())
                {
                    FString AnchorStr = Request.Value.StringValue.ToLower();
                    if (AnchorStr.Contains(TEXT("fill")) || AnchorStr.Contains(TEXT("stretch")))
                    {
                        Anchors.Minimum = FVector2D(0, 0);
                        Anchors.Maximum = FVector2D(1, 1);
                    }
                    else if (AnchorStr.Contains(TEXT("center")))
                    {
                        Anchors.Minimum = FVector2D(0.5, 0.5);
                        Anchors.Maximum = FVector2D(0.5, 0.5);
                    }
                    else
                    {
                        // Default to top-left
                        Anchors.Minimum = FVector2D(0, 0);
                        Anchors.Maximum = FVector2D(0, 0);
                    }
                }
                CanvasSlot->SetAnchors(Anchors);
                ResultPayload.AppliedValue.SetString(FString::Printf(TEXT("{min:[%f,%f], max:[%f,%f]}"), 
                    Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y));
            }
            else if (PropName.Equals(TEXT("position"), ESearchCase::IgnoreCase))
            {
                // Position expects [x, y] array
                if (Request.Value.HasJson() && Request.Value.JsonValue->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& Arr = Request.Value.JsonValue->AsArray();
                    if (Arr.Num() >= 2)
                    {
                        FVector2D Position(Arr[0]->AsNumber(), Arr[1]->AsNumber());
                        CanvasSlot->SetPosition(Position);
                        ResultPayload.AppliedValue.SetString(FString::Printf(TEXT("[%f, %f]"), Position.X, Position.Y));
                    }
                }
            }
            else if (PropName.Equals(TEXT("size"), ESearchCase::IgnoreCase))
            {
                // Size expects [width, height] array
                if (Request.Value.HasJson() && Request.Value.JsonValue->Type == EJson::Array)
                {
                    const TArray<TSharedPtr<FJsonValue>>& Arr = Request.Value.JsonValue->AsArray();
                    if (Arr.Num() >= 2)
                    {
                        FVector2D Size(Arr[0]->AsNumber(), Arr[1]->AsNumber());
                        CanvasSlot->SetSize(Size);
                        ResultPayload.AppliedValue.SetString(FString::Printf(TEXT("[%f, %f]"), Size.X, Size.Y));
                    }
                }
            }
            else if (PropName.Equals(TEXT("auto_size"), ESearchCase::IgnoreCase) || PropName.Equals(TEXT("autosize"), ESearchCase::IgnoreCase))
            {
                bool bAutoSize = false;
                if (Request.Value.HasJson())
                {
                    bAutoSize = Request.Value.JsonValue->AsBool();
                }
                else if (Request.Value.HasString())
                {
                    bAutoSize = Request.Value.StringValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
                }
                CanvasSlot->SetAutoSize(bAutoSize);
                ResultPayload.AppliedValue.SetString(bAutoSize ? TEXT("true") : TEXT("false"));
            }
            else if (PropName.Equals(TEXT("z_order"), ESearchCase::IgnoreCase) || PropName.Equals(TEXT("zorder"), ESearchCase::IgnoreCase))
            {
                int32 ZOrder = 0;
                if (Request.Value.HasJson() && Request.Value.JsonValue->Type == EJson::Number)
                {
                    ZOrder = static_cast<int32>(Request.Value.JsonValue->AsNumber());
                }
                else if (Request.Value.HasString())
                {
                    ZOrder = FCString::Atoi(*Request.Value.StringValue);
                }
                CanvasSlot->SetZOrder(ZOrder);
                ResultPayload.AppliedValue.SetString(FString::FromInt(ZOrder));
            }

            FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
            WidgetBlueprint->MarkPackageDirty();

            ResultPayload.Note = FString::Printf(TEXT("Canvas slot property '%s' set successfully"), *PropName);
            return TResult<FWidgetPropertySetResult>::Success(ResultPayload);
        }

        if (!ResolvedTarget.Property)
        {
            return TResult<FWidgetPropertySetResult>::Error(
                VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
                FString::Printf(TEXT("Property '%s' not found on target"), *Request.PropertyPath)
            );
        }

        Property = ResolvedTarget.Property;
        ContainerPtrForSet = ResolvedTarget.ContainerPtr ? ResolvedTarget.ContainerPtr : static_cast<void*>(FoundWidget);
    }

    if (Request.CollectionOperation.IsSet())
    {
        const FString Operation = Request.CollectionOperation->Operation.ToLower();
        FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
        if (!ArrayProperty)
        {
            return TResult<FWidgetPropertySetResult>::Error(
                VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
                TEXT("collection_op currently supports TArray only")
            );
        }

        void* ArrayAddr = ArrayProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
        FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);

        auto ConvertAndAssignElement = [&](int32 DestIndex, const TSharedPtr<FJsonValue>& JsonElem, FString& OutError) -> bool
        {
            ArrayHelper.ExpandForIndex(DestIndex);
            void* ElemPtr = ArrayHelper.GetRawPtr(DestIndex);
            FProperty* ElemProp = ArrayProperty->Inner;

            if (FStrProperty* PropStr = CastField<FStrProperty>(ElemProp))
            {
                FString Value;
                if (JsonElem->Type == EJson::String)
                {
                    Value = JsonElem->AsString();
                }
                else if (JsonElem->Type == EJson::Number)
                {
                    Value = FString::SanitizeFloat(JsonElem->AsNumber());
                }
                else if (JsonElem->Type == EJson::Boolean)
                {
                    Value = JsonElem->AsBool() ? TEXT("true") : TEXT("false");
                }
                else
                {
                    Value = JsonElem->AsString();
                }
                PropStr->SetPropertyValue(ElemPtr, Value);
                return true;
            }

            if (FTextProperty* PropText = CastField<FTextProperty>(ElemProp))
            {
                FText Value = FText::FromString(JsonElem->AsString());
                PropText->SetPropertyValue(ElemPtr, Value);
                return true;
            }

            if (FBoolProperty* PropBool = CastField<FBoolProperty>(ElemProp))
            {
                bool Value = (JsonElem->Type == EJson::Boolean) ? JsonElem->AsBool() : JsonElem->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase);
                PropBool->SetPropertyValue(ElemPtr, Value);
                return true;
            }

            if (FFloatProperty* PropFloat = CastField<FFloatProperty>(ElemProp))
            {
                float Value = (JsonElem->Type == EJson::Number) ? static_cast<float>(JsonElem->AsNumber()) : FCString::Atof(*JsonElem->AsString());
                PropFloat->SetPropertyValue(ElemPtr, Value);
                return true;
            }

            if (FIntProperty* PropInt = CastField<FIntProperty>(ElemProp))
            {
                int32 Value = (JsonElem->Type == EJson::Number) ? static_cast<int32>(JsonElem->AsNumber()) : FCString::Atoi(*JsonElem->AsString());
                PropInt->SetPropertyValue(ElemPtr, Value);
                return true;
            }

            if (FByteProperty* PropByte = CastField<FByteProperty>(ElemProp))
            {
                if (PropByte->Enum)
                {
                    const FString NameStr = JsonElem->AsString();
                    const int64 EnumValue = PropByte->Enum->GetValueByNameString(NameStr);
                    if (EnumValue == INDEX_NONE)
                    {
                        OutError = FString::Printf(TEXT("Invalid enum value '%s'"), *NameStr);
                        return false;
                    }
                    PropByte->SetPropertyValue(ElemPtr, static_cast<uint8>(EnumValue));
                    return true;
                }

                const uint8 Value = (JsonElem->Type == EJson::Number) ? static_cast<uint8>(JsonElem->AsNumber()) : static_cast<uint8>(FCString::Atoi(*JsonElem->AsString()));
                PropByte->SetPropertyValue(ElemPtr, Value);
                return true;
            }

            if (FStructProperty* PropStruct = CastField<FStructProperty>(ElemProp))
            {
                if (JsonElem->Type != EJson::Object)
                {
                    OutError = TEXT("Struct array element requires JSON object");
                    return false;
                }

                TSharedPtr<FJsonObject> Obj = JsonElem->AsObject();
                return FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), PropStruct->Struct, ElemPtr, 0, 0);
            }

            OutError = TEXT("Unsupported array element type");
            return false;
        };

        if (Operation == TEXT("clear"))
        {
            ArrayHelper.Resize(0);
        }
        else if (Operation == TEXT("set") || Operation == TEXT("append"))
        {
            if (!Request.Value.HasJson() || Request.Value.JsonValue->Type != EJson::Array)
            {
                return TResult<FWidgetPropertySetResult>::Error(
                    VibeUE::ErrorCodes::PARAM_TYPE_MISMATCH,
                    TEXT("collection_op requires property_value to be an array")
                );
            }

            const TArray<TSharedPtr<FJsonValue>>& JsonArr = Request.Value.JsonValue->AsArray();
            int32 StartIndex = (Operation == TEXT("set")) ? 0 : ArrayHelper.Num();
            if (Operation == TEXT("set"))
            {
                ArrayHelper.Resize(0);
            }

            for (int32 Index = 0; Index < JsonArr.Num(); ++Index)
            {
                FString ConversionError;
                if (!ConvertAndAssignElement(StartIndex + Index, JsonArr[Index], ConversionError))
                {
                    return TResult<FWidgetPropertySetResult>::Error(
                        VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
                        ConversionError
                    );
                }
            }
        }
        else if (Operation == TEXT("insert") || Operation == TEXT("updateat") || Operation == TEXT("removeat"))
        {
            if (!Request.CollectionOperation->Index.IsSet())
            {
                return TResult<FWidgetPropertySetResult>::Error(
                    VibeUE::ErrorCodes::PARAM_FIELD_REQUIRED,
                    TEXT("collection_op requires 'index' parameter")
                );
            }

            int32 IndexValue = Request.CollectionOperation->Index.GetValue();

            if (Operation == TEXT("removeat"))
            {
                if (IndexValue < 0 || IndexValue >= ArrayHelper.Num())
                {
                    return TResult<FWidgetPropertySetResult>::Error(
                        VibeUE::ErrorCodes::PARAM_OUT_OF_RANGE,
                        TEXT("removeAt index out of range")
                    );
                }
                ArrayHelper.RemoveValues(IndexValue, 1);
            }
            else
            {
                if (!Request.Value.HasJson())
                {
                    return TResult<FWidgetPropertySetResult>::Error(
                        VibeUE::ErrorCodes::PARAM_TYPE_MISMATCH,
                        TEXT("insert/updateAt requires JSON property_value for element")
                    );
                }

                if (Operation == TEXT("insert"))
                {
                    IndexValue = FMath::Clamp(IndexValue, 0, ArrayHelper.Num());
                    ArrayHelper.InsertValues(IndexValue, 1);
                }
                else
                {
                    if (IndexValue < 0 || IndexValue >= ArrayHelper.Num())
                    {
                        return TResult<FWidgetPropertySetResult>::Error(
                            VibeUE::ErrorCodes::PARAM_OUT_OF_RANGE,
                            TEXT("updateAt index out of range")
                        );
                    }
                }

                FString ConversionError;
                if (!ConvertAndAssignElement(IndexValue, Request.Value.JsonValue, ConversionError))
                {
                    return TResult<FWidgetPropertySetResult>::Error(
                        VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
                        ConversionError
                    );
                }
            }
        }
        else
        {
            return TResult<FWidgetPropertySetResult>::Error(
                VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
                TEXT("Unsupported collection_op for arrays")
            );
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
        if (GEditor)
        {
            GEditor->NoteSelectionChange();
            if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
            {
                TArray<IAssetEditorInstance*> AssetEditors = AssetSubsystem->FindEditorsForAsset(WidgetBlueprint);
                for (IAssetEditorInstance* EditorInstance : AssetEditors)
                {
                    if (FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(EditorInstance))
                    {
                        WidgetEditor->RefreshEditors();
                    }
                }
            }
        }

        WidgetBlueprint->MarkPackageDirty();

        ResultPayload.CollectionOperation = Request.CollectionOperation->Operation;
        ResultPayload.Note = TEXT("Array collection operation applied");
        return TResult<FWidgetPropertySetResult>::Success(ResultPayload);
    }

    bool bPropertySet = false;
    FString ErrorMessage;

    if (Request.Value.HasJson())
    {
        if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            const FString StructName = StructProperty->Struct->GetName();
            
            // Special handling for color types - support array format [R, G, B, A]
            if (StructName.Contains(TEXT("LinearColor")) || StructName.Contains(TEXT("SlateColor")))
            {
                FLinearColor ParsedColor;
                if (FJsonValueHelper::TryGetLinearColor(Request.Value.JsonValue, ParsedColor))
                {
                    void* ValuePtr = bUsedResolver ? ContainerPtrForSet : StructProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
                    if (StructName.Contains(TEXT("SlateColor")))
                    {
                        FSlateColor* SlateColorPtr = static_cast<FSlateColor*>(ValuePtr);
                        *SlateColorPtr = FSlateColor(ParsedColor);
                    }
                    else
                    {
                        FLinearColor* ColorPtr = static_cast<FLinearColor*>(ValuePtr);
                        *ColorPtr = ParsedColor;
                    }
                    bPropertySet = true;
                }
                else
                {
                    ErrorMessage = FString::Printf(TEXT("Invalid color format for '%s'. Use array [R,G,B,A], hex \"#RRGGBB\", or object {\"R\":...,\"G\":...,\"B\":...,\"A\":...}"), *Request.PropertyPath);
                }
            }
            else if (Request.Value.JsonValue->Type != EJson::Object)
            {
                ErrorMessage = FString::Printf(TEXT("Struct property '%s' requires JSON object"), *Request.PropertyPath);
            }
            else
            {
                // Try custom ParseComplexPropertyValue first for special struct types like FSlateBrush, FButtonStyle
                // These have custom JSON parsing that UE's FJsonObjectConverter doesn't handle well
                if (StructName.Contains(TEXT("SlateBrush")) || StructName.Contains(TEXT("ButtonStyle")))
                {
                    bPropertySet = ParseComplexPropertyValue(Request.Value.JsonValue, Property, FoundWidget, ErrorMessage);
                }
                
                // Fall back to standard JSON conversion if custom parsing didn't work
                if (!bPropertySet)
                {
                    TSharedPtr<FJsonObject> JsonObj = Request.Value.JsonValue->AsObject();
                    void* ValuePtr = bUsedResolver ? ContainerPtrForSet : StructProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
                    bPropertySet = FJsonObjectConverter::JsonObjectToUStruct(JsonObj.ToSharedRef(), StructProperty->Struct, ValuePtr, 0, 0);
                    if (!bPropertySet)
                    {
                        ErrorMessage = FString::Printf(TEXT("Failed to convert JSON to struct for property '%s'. For FSlateBrush use: {\"ResourceObject\": \"/path/to/texture\", \"DrawAs\": \"Image\", \"TintColor\": [R,G,B,A]}"), *Request.PropertyPath);
                    }
                }
            }
        }
    }

    if (!bPropertySet && Request.Value.HasJson())
    {
        if (!CastField<FStructProperty>(Property))
        {
            bPropertySet = ParseComplexPropertyValue(Request.Value.JsonValue, Property, FoundWidget, ErrorMessage);
        }
    }

    if (!bPropertySet && Request.Value.HasString())
    {
        const FString& StringValue = Request.Value.StringValue;

        if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
        {
            StrProperty->SetPropertyValue_InContainer(ContainerPtrForSet, StringValue);
            bPropertySet = true;
        }
        else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
        {
            FText TextValue = FText::FromString(StringValue);
            TextProperty->SetPropertyValue_InContainer(ContainerPtrForSet, TextValue);
            bPropertySet = true;
        }
        else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
        {
            bool BoolValue = StringValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || StringValue.Equals(TEXT("1"));
            BoolProperty->SetPropertyValue_InContainer(ContainerPtrForSet, BoolValue);
            bPropertySet = true;
        }
        else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
        {
            float FloatValue = FCString::Atof(*StringValue);
            FloatProperty->SetPropertyValue_InContainer(ContainerPtrForSet, FloatValue);
            bPropertySet = true;
        }
        else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
        {
            int32 IntValue = FCString::Atoi(*StringValue);
            IntProperty->SetPropertyValue_InContainer(ContainerPtrForSet, IntValue);
            bPropertySet = true;
        }
        else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
        {
            if (ByteProperty->Enum)
            {
                const int64 EnumValue = ByteProperty->Enum->GetValueByNameString(StringValue);
                if (EnumValue != INDEX_NONE)
                {
                    ByteProperty->SetPropertyValue_InContainer(ContainerPtrForSet, static_cast<uint8>(EnumValue));
                    bPropertySet = true;
                }
                else
                {
                    ErrorMessage = FString::Printf(TEXT("Invalid enum value '%s' for property '%s'"), *StringValue, *Request.PropertyPath);
                }
            }
            else
            {
                const uint8 ByteValue = static_cast<uint8>(FCString::Atoi(*StringValue));
                ByteProperty->SetPropertyValue_InContainer(ContainerPtrForSet, ByteValue);
                bPropertySet = true;
            }
        }
        else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
        {
            if (EnumProperty->GetUnderlyingProperty() && EnumProperty->GetEnum())
            {
                const int64 EnumValue = EnumProperty->GetEnum()->GetValueByNameString(StringValue);
                if (EnumValue != INDEX_NONE)
                {
                    uint8* EnumValuePtr = static_cast<uint8*>(EnumProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet));
                    EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumValuePtr, EnumValue);
                    bPropertySet = true;
                }
                else
                {
                    ErrorMessage = FString::Printf(TEXT("Invalid enum value '%s' for property '%s'"), *StringValue, *Request.PropertyPath);
                }
            }
            else
            {
                ErrorMessage = FString::Printf(TEXT("Cannot set enum property '%s'"), *Request.PropertyPath);
            }
        }
        else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            TSharedPtr<FJsonObject> JsonObj;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StringValue);
            if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
            {
                void* ValuePtr = bUsedResolver ? ContainerPtrForSet : StructProperty->ContainerPtrToValuePtr<void>(ContainerPtrForSet);
                bPropertySet = FJsonObjectConverter::JsonObjectToUStruct(JsonObj.ToSharedRef(), StructProperty->Struct, ValuePtr, 0, 0);
                if (!bPropertySet)
                {
                    TSharedPtr<FJsonValue> JsonValue = MakeShareable(new FJsonValueObject(JsonObj));
                    bPropertySet = ParseComplexPropertyValue(JsonValue, Property, FoundWidget, ErrorMessage);
                }
            }
            else
            {
                ErrorMessage = FString::Printf(TEXT("Invalid JSON for struct property '%s'"), *Request.PropertyPath);
            }
        }
    }

    if (!bPropertySet)
    {
        if (ErrorMessage.IsEmpty())
        {
            ErrorMessage = FString::Printf(TEXT("Unsupported property type for '%s'"), *Request.PropertyPath);
        }

        return TResult<FWidgetPropertySetResult>::Error(
            VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
            ErrorMessage
        );
    }

    bool bStructuralChange = false;

    if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
        PropertyChangedEvent.MemberProperty = Property;
        FoundWidget->PostEditChangeProperty(PropertyChangedEvent);
    }

    if (Property->GetFName() == TEXT("bIsVariable") || Request.PropertyPath.Equals(TEXT("IsVariable"), ESearchCase::IgnoreCase))
    {
        bStructuralChange = true;
    }

    if (bStructuralChange)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    }
    else
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
    }

    if (GEditor)
    {
        GEditor->NoteSelectionChange();
        if (UAssetEditorSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            TArray<IAssetEditorInstance*> AssetEditors = AssetSubsystem->FindEditorsForAsset(WidgetBlueprint);
            for (IAssetEditorInstance* EditorInstance : AssetEditors)
            {
                if (FWidgetBlueprintEditor* WidgetEditor = static_cast<FWidgetBlueprintEditor*>(EditorInstance))
                {
                    WidgetEditor->RefreshEditors();
                }
            }
        }
    }

    WidgetBlueprint->MarkPackageDirty();

    if (Request.Value.HasJson())
    {
        ResultPayload.AppliedValue.SetJson(Request.Value.JsonValue);
    }
    if (Request.Value.HasString())
    {
        ResultPayload.AppliedValue.SetString(Request.Value.StringValue);
    }

    ResultPayload.bStructuralChange = bStructuralChange;
    ResultPayload.Note = TEXT("Property set successfully");
    return TResult<FWidgetPropertySetResult>::Success(ResultPayload);
}

TResult<FWidgetPropertyGetResult> FWidgetPropertyService::GetWidgetProperty(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, const FString& PropertyPath)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertyGetResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertyGetResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(PropertyPath, TEXT("PropertyPath"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertyGetResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<FWidgetPropertyGetResult>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* FoundWidget = nullptr;
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

    if (!FoundWidget)
    {
        return TResult<FWidgetPropertyGetResult>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprint->GetName())
        );
    }

    bool bSlotRoot = false;
    TArray<FPathSegment> Segments;
    if (!ParsePropertyPath(PropertyPath, bSlotRoot, Segments))
    {
        // Provide helpful error message for common mistakes
        if (PropertyPath.Equals(TEXT("Slot"), ESearchCase::IgnoreCase))
        {
            return TResult<FWidgetPropertyGetResult>::Error(
                VibeUE::ErrorCodes::PARAM_INVALID,
                TEXT("Cannot access 'Slot' directly. Use 'Slot.PropertyName' format (e.g., 'Slot.Padding', 'Slot.HorizontalAlignment', 'Slot.ChildOrder').")
            );
        }
        return TResult<FWidgetPropertyGetResult>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            TEXT("Invalid property_name path")
        );
    }

    FString ResolveError;
    FResolvedTarget Target;
    if (!ResolvePath(FoundWidget, Segments, bSlotRoot, Target, ResolveError))
    {
        return TResult<FWidgetPropertyGetResult>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            ResolveError
        );
    }

    FWidgetPropertyGetResult ResultPayload;
    ResultPayload.PropertyPath = PropertyPath;
    ResultPayload.SlotClass = FoundWidget->Slot ? FoundWidget->Slot->GetClass()->GetName() : TEXT("");

    if (Target.bIsSyntheticChildOrder)
    {
        UPanelSlot* Slot = Cast<UPanelSlot>(FoundWidget->Slot);
        UPanelWidget* Parent = Slot ? Slot->Parent : nullptr;
        const int32 Index = Parent ? Parent->GetChildIndex(FoundWidget) : 0;

        ResultPayload.Value.SetString(FString::FromInt(Index));
        ResultPayload.PropertyType = TEXT("int");
        ResultPayload.bIsEditable = true;
        ResultPayload.bIsChildOrder = true;
        ResultPayload.ChildOrderValue = Index;
        ResultPayload.ChildCount = Parent ? Parent->GetChildrenCount() : 0;

        ResultPayload.Constraints = MakeShareable(new FJsonObject);
        ResultPayload.Constraints->SetNumberField(TEXT("child_count"), ResultPayload.ChildCount);

        return TResult<FWidgetPropertyGetResult>::Success(ResultPayload);
    }

    // Handle canvas slot virtual properties (alignment, anchors, position, size, etc.)
    if (Target.bIsCanvasSlotVirtualProperty)
    {
        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(FoundWidget->Slot);
        if (!CanvasSlot)
        {
            return TResult<FWidgetPropertyGetResult>::Error(
                VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
                TEXT("Widget is not in a CanvasPanel - cannot get canvas slot properties")
            );
        }

        const FString& PropName = Target.CanvasSlotVirtualPropertyName;
        
        if (PropName.Equals(TEXT("alignment"), ESearchCase::IgnoreCase))
        {
            FVector2D Alignment = CanvasSlot->GetAlignment();
            ResultPayload.Value.SetString(FString::Printf(TEXT("[%f, %f]"), Alignment.X, Alignment.Y));
            ResultPayload.PropertyType = TEXT("FVector2D");
            ResultPayload.bIsEditable = true;
        }
        else if (PropName.Equals(TEXT("anchors"), ESearchCase::IgnoreCase))
        {
            FAnchors Anchors = CanvasSlot->GetAnchors();
            ResultPayload.Value.SetString(FString::Printf(TEXT("{\"min_x\":%f,\"min_y\":%f,\"max_x\":%f,\"max_y\":%f}"), 
                Anchors.Minimum.X, Anchors.Minimum.Y, Anchors.Maximum.X, Anchors.Maximum.Y));
            ResultPayload.PropertyType = TEXT("FAnchors");
            ResultPayload.bIsEditable = true;
        }
        else if (PropName.Equals(TEXT("position"), ESearchCase::IgnoreCase))
        {
            FVector2D Position = CanvasSlot->GetPosition();
            ResultPayload.Value.SetString(FString::Printf(TEXT("[%f, %f]"), Position.X, Position.Y));
            ResultPayload.PropertyType = TEXT("FVector2D");
            ResultPayload.bIsEditable = true;
        }
        else if (PropName.Equals(TEXT("size"), ESearchCase::IgnoreCase))
        {
            FVector2D Size = CanvasSlot->GetSize();
            ResultPayload.Value.SetString(FString::Printf(TEXT("[%f, %f]"), Size.X, Size.Y));
            ResultPayload.PropertyType = TEXT("FVector2D");
            ResultPayload.bIsEditable = true;
        }
        else if (PropName.Equals(TEXT("auto_size"), ESearchCase::IgnoreCase) || PropName.Equals(TEXT("autosize"), ESearchCase::IgnoreCase))
        {
            bool bAutoSize = CanvasSlot->GetAutoSize();
            ResultPayload.Value.SetString(bAutoSize ? TEXT("true") : TEXT("false"));
            ResultPayload.PropertyType = TEXT("bool");
            ResultPayload.bIsEditable = true;
        }
        else if (PropName.Equals(TEXT("z_order"), ESearchCase::IgnoreCase) || PropName.Equals(TEXT("zorder"), ESearchCase::IgnoreCase))
        {
            int32 ZOrder = CanvasSlot->GetZOrder();
            ResultPayload.Value.SetString(FString::FromInt(ZOrder));
            ResultPayload.PropertyType = TEXT("int32");
            ResultPayload.bIsEditable = true;
        }

        return TResult<FWidgetPropertyGetResult>::Success(ResultPayload);
    }

    FProperty* Property = Target.Property;
    if (!Property)
    {
        return TResult<FWidgetPropertyGetResult>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Property '%s' not found on target"), *PropertyPath)
        );
    }

    FString PropertyValueString;
    FString PropertyType;
    TSharedPtr<FJsonValue> PropertyJson;
    TSharedPtr<FJsonObject> Constraints;
    TSharedPtr<FJsonObject> Schema;

    void* ContainerPtr = Target.ContainerPtr ? Target.ContainerPtr : static_cast<void*>(FoundWidget);

    if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
    {
        PropertyValueString = StrProperty->GetPropertyValue_InContainer(ContainerPtr);
        PropertyType = TEXT("String");
    }
    else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
    {
        const FText TextValue = TextProperty->GetPropertyValue_InContainer(ContainerPtr);
        PropertyValueString = TextValue.ToString();
        PropertyType = TEXT("Text");
    }
    else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
    {
        const bool BoolValue = BoolProperty->GetPropertyValue_InContainer(ContainerPtr);
        PropertyValueString = BoolValue ? TEXT("true") : TEXT("false");
        PropertyType = TEXT("bool");
    }
    else if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
    {
        const float FloatValue = FloatProperty->GetPropertyValue_InContainer(ContainerPtr);
        PropertyValueString = FString::SanitizeFloat(FloatValue);
        PropertyType = TEXT("float");
    }
    else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
    {
        const int32 IntValue = IntProperty->GetPropertyValue_InContainer(ContainerPtr);
        PropertyValueString = FString::FromInt(IntValue);
        PropertyType = TEXT("int");
    }
    else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
    {
        const uint8 ByteValue = ByteProperty->GetPropertyValue_InContainer(ContainerPtr);
        if (ByteProperty->Enum)
        {
            PropertyValueString = ByteProperty->Enum->GetNameStringByValue(ByteValue);
            PropertyType = FString::Printf(TEXT("Enum<%s>"), *ByteProperty->Enum->GetName());
        }
        else
        {
            PropertyValueString = FString::FromInt(ByteValue);
            PropertyType = TEXT("byte");
        }
    }
    else if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        if (EnumProperty->GetUnderlyingProperty() && EnumProperty->GetEnum())
        {
            uint8* EnumValuePtr = static_cast<uint8*>(EnumProperty->ContainerPtrToValuePtr<void>(ContainerPtr));
            const int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumValuePtr);
            PropertyValueString = EnumProperty->GetEnum()->GetNameStringByValue(EnumValue);
            PropertyType = FString::Printf(TEXT("Enum<%s>"), *EnumProperty->GetEnum()->GetName());
        }
        else
        {
            PropertyValueString = TEXT("UnknownEnum");
            PropertyType = TEXT("EnumProperty");
        }
    }
    else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        void* ValuePtr = Target.ContainerPtr ? Target.ContainerPtr : StructProperty->ContainerPtrToValuePtr<void>(FoundWidget);
        TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
        if (FJsonObjectConverter::UStructToJsonObject(StructProperty->Struct, ValuePtr, JsonObject.ToSharedRef(), 0, 0))
        {
            PropertyJson = MakeShareable(new FJsonValueObject(JsonObject));
            PropertyType = FString::Printf(TEXT("Struct<%s>"), *StructProperty->Struct->GetName());
        }
        else
        {
            PropertyValueString = TEXT("StructSerializationFailed");
            PropertyType = FString::Printf(TEXT("Struct<%s>"), *StructProperty->Struct->GetName());
        }
    }
    else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        void* ArrayAddr = ArrayProperty->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayAddr);
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
        {
            void* ElementPtr = ArrayHelper.GetRawPtr(Index);
            FProperty* ElementProperty = ArrayProperty->Inner;
            if (FStrProperty* StrProp = CastField<FStrProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueString(StrProp->GetPropertyValue(ElementPtr))));
            }
            else if (FTextProperty* TextProp = CastField<FTextProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueString(TextProp->GetPropertyValue(ElementPtr).ToString())));
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueBoolean(BoolProp->GetPropertyValue(ElementPtr))));
            }
            else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueNumber(FloatProp->GetPropertyValue(ElementPtr))));
            }
            else if (FIntProperty* IntProp = CastField<FIntProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueNumber(IntProp->GetPropertyValue(ElementPtr))));
            }
            else if (FByteProperty* ByteProp = CastField<FByteProperty>(ElementProperty))
            {
                if (ByteProp->Enum)
                {
                    const uint8 EnumValue = ByteProp->GetPropertyValue(ElementPtr);
                    JsonArray.Add(MakeShareable(new FJsonValueString(ByteProp->Enum->GetNameStringByValue(EnumValue))));
                }
                else
                {
                    JsonArray.Add(MakeShareable(new FJsonValueNumber(ByteProp->GetPropertyValue(ElementPtr))));
                }
            }
            else if (FStructProperty* StructProp = CastField<FStructProperty>(ElementProperty))
            {
                TSharedPtr<FJsonObject> ElemObj = MakeShareable(new FJsonObject);
                FJsonObjectConverter::UStructToJsonObject(StructProp->Struct, ElementPtr, ElemObj.ToSharedRef(), 0, 0);
                JsonArray.Add(MakeShareable(new FJsonValueObject(ElemObj)));
            }
            else
            {
                JsonArray.Add(MakeShareable(new FJsonValueString(TEXT("UnsupportedArrayElemType"))));
            }
        }
        PropertyJson = MakeShareable(new FJsonValueArray(JsonArray));
        PropertyType = TEXT("Array");
        AppendCollectionLength(Property, ContainerPtr, Constraints);
    }
    else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
    {
        void* SetAddr = SetProperty->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptSetHelper SetHelper(SetProperty, SetAddr);
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        for (int32 Index = 0; Index < SetHelper.Num(); ++Index)
        {
            if (!SetHelper.IsValidIndex(Index))
            {
                continue;
            }

            void* ElementPtr = SetHelper.GetElementPtr(Index);
            FProperty* ElementProperty = SetProperty->ElementProp;

            if (FStrProperty* StrProp = CastField<FStrProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueString(StrProp->GetPropertyValue(ElementPtr))));
            }
            else if (FTextProperty* TextProp = CastField<FTextProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueString(TextProp->GetPropertyValue(ElementPtr).ToString())));
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueBoolean(BoolProp->GetPropertyValue(ElementPtr))));
            }
            else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueNumber(FloatProp->GetPropertyValue(ElementPtr))));
            }
            else if (FIntProperty* IntProp = CastField<FIntProperty>(ElementProperty))
            {
                JsonArray.Add(MakeShareable(new FJsonValueNumber(IntProp->GetPropertyValue(ElementPtr))));
            }
            else if (FByteProperty* ByteProp = CastField<FByteProperty>(ElementProperty))
            {
                if (ByteProp->Enum)
                {
                    const uint8 EnumValue = ByteProp->GetPropertyValue(ElementPtr);
                    JsonArray.Add(MakeShareable(new FJsonValueString(ByteProp->Enum->GetNameStringByValue(EnumValue))));
                }
                else
                {
                    JsonArray.Add(MakeShareable(new FJsonValueNumber(ByteProp->GetPropertyValue(ElementPtr))));
                }
            }
            else if (FStructProperty* StructProp = CastField<FStructProperty>(ElementProperty))
            {
                TSharedPtr<FJsonObject> ElemObj = MakeShareable(new FJsonObject);
                FJsonObjectConverter::UStructToJsonObject(StructProp->Struct, ElementPtr, ElemObj.ToSharedRef(), 0, 0);
                JsonArray.Add(MakeShareable(new FJsonValueObject(ElemObj)));
            }
        }

        PropertyJson = MakeShareable(new FJsonValueArray(JsonArray));
        PropertyType = TEXT("Set");
        AppendCollectionLength(Property, ContainerPtr, Constraints);
    }
    else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
    {
        void* MapAddr = MapProperty->ContainerPtrToValuePtr<void>(ContainerPtr);
        FScriptMapHelper MapHelper(MapProperty, MapAddr);
        TSharedPtr<FJsonObject> MapObject = MakeShareable(new FJsonObject);
        for (int32 Index = 0; Index < MapHelper.GetMaxIndex(); ++Index)
        {
            if (!MapHelper.IsValidIndex(Index))
            {
                continue;
            }

            uint8* PairPtr = static_cast<uint8*>(MapHelper.GetPairPtr(Index));
            void* KeyPtr = PairPtr;
            void* ValuePtr = PairPtr + MapProperty->MapLayout.ValueOffset;

            FString KeyString;
            if (FNameProperty* NameProp = CastField<FNameProperty>(MapProperty->KeyProp))
            {
                KeyString = NameProp->GetPropertyValue(KeyPtr).ToString();
            }
            else if (FStrProperty* StrProp = CastField<FStrProperty>(MapProperty->KeyProp))
            {
                KeyString = StrProp->GetPropertyValue(KeyPtr);
            }
            else if (FIntProperty* IntProp = CastField<FIntProperty>(MapProperty->KeyProp))
            {
                KeyString = FString::FromInt(IntProp->GetPropertyValue(KeyPtr));
            }
            else if (FByteProperty* ByteProp = CastField<FByteProperty>(MapProperty->KeyProp))
            {
                if (ByteProp->Enum)
                {
                    KeyString = ByteProp->Enum->GetNameStringByValue(ByteProp->GetPropertyValue(KeyPtr));
                }
                else
                {
                    KeyString = FString::FromInt(ByteProp->GetPropertyValue(KeyPtr));
                }
            }
            else
            {
                KeyString = TEXT("UnsupportedKey");
            }

            TSharedPtr<FJsonValue> ValueJson;
            FProperty* ValueProperty = MapProperty->ValueProp;
            if (FStrProperty* StrProp = CastField<FStrProperty>(ValueProperty))
            {
                ValueJson = MakeShareable(new FJsonValueString(StrProp->GetPropertyValue(ValuePtr)));
            }
            else if (FTextProperty* TextProp = CastField<FTextProperty>(ValueProperty))
            {
                ValueJson = MakeShareable(new FJsonValueString(TextProp->GetPropertyValue(ValuePtr).ToString()));
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(ValueProperty))
            {
                ValueJson = MakeShareable(new FJsonValueBoolean(BoolProp->GetPropertyValue(ValuePtr)));
            }
            else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(ValueProperty))
            {
                ValueJson = MakeShareable(new FJsonValueNumber(FloatProp->GetPropertyValue(ValuePtr)));
            }
            else if (FIntProperty* IntProp = CastField<FIntProperty>(ValueProperty))
            {
                ValueJson = MakeShareable(new FJsonValueNumber(IntProp->GetPropertyValue(ValuePtr)));
            }
            else if (FByteProperty* ByteProp = CastField<FByteProperty>(ValueProperty))
            {
                if (ByteProp->Enum)
                {
                    ValueJson = MakeShareable(new FJsonValueString(ByteProp->Enum->GetNameStringByValue(ByteProp->GetPropertyValue(ValuePtr))));
                }
                else
                {
                    ValueJson = MakeShareable(new FJsonValueNumber(ByteProp->GetPropertyValue(ValuePtr)));
                }
            }
            else if (FStructProperty* StructProp = CastField<FStructProperty>(ValueProperty))
            {
                TSharedPtr<FJsonObject> ValueObject = MakeShareable(new FJsonObject);
                FJsonObjectConverter::UStructToJsonObject(StructProp->Struct, ValuePtr, ValueObject.ToSharedRef(), 0, 0);
                ValueJson = MakeShareable(new FJsonValueObject(ValueObject));
            }
            else
            {
                ValueJson = MakeShareable(new FJsonValueString(TEXT("UnsupportedValueType")));
            }

            MapObject->SetField(KeyString, ValueJson);
        }

        PropertyJson = MakeShareable(new FJsonValueObject(MapObject));
        PropertyType = TEXT("Map");
        AppendCollectionLength(Property, ContainerPtr, Constraints);
    }
    else
    {
        PropertyValueString = TEXT("UnsupportedType");
        PropertyType = Property->GetClass()->GetName();
    }

    AddEnumConstraints(Property, Constraints);
    AddNumericConstraints(Property, Constraints);
    AppendSchemaHints(Property, Schema);

    ResultPayload.PropertyType = PropertyType;
    ResultPayload.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);

    if (PropertyJson.IsValid())
    {
        ResultPayload.Value.SetJson(PropertyJson);
    }
    else
    {
        ResultPayload.Value.SetString(PropertyValueString);
    }

    if (Constraints.IsValid())
    {
        ResultPayload.Constraints = Constraints;
    }

    if (Schema.IsValid())
    {
        ResultPayload.Schema = Schema;
    }

    return TResult<FWidgetPropertyGetResult>::Success(ResultPayload);
}

TResult<TArray<FWidgetPropertyInfo>> FWidgetPropertyService::ListWidgetProperties(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, bool bIncludeSlotProperties)
{
    auto ValidationResult = ValidateNotNull(WidgetBlueprint, TEXT("WidgetBlueprint"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    ValidationResult = ValidateNotEmpty(WidgetName, TEXT("WidgetName"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    if (!WidgetBlueprint->WidgetTree)
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            TEXT("Widget blueprint has no widget tree")
        );
    }

    UWidget* FoundWidget = nullptr;
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

    if (!FoundWidget)
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(
            VibeUE::ErrorCodes::WIDGET_NOT_FOUND,
            FString::Printf(TEXT("Widget component '%s' not found in blueprint '%s'"), *WidgetName, *WidgetBlueprint->GetName())
        );
    }

    return ListWidgetProperties(FoundWidget, bIncludeSlotProperties);
}

TResult<TArray<FWidgetPropertyInfo>> FWidgetPropertyService::ListWidgetProperties(UWidget* Widget, bool bIncludeSlotProperties)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FWidgetPropertyInfo> Properties;

    // Get widget properties
    for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
        {
            continue;
        }

        FWidgetPropertyInfo Info;
        Info.PropertyName = Property->GetName();
        Info.PropertyType = Property->GetCPPType();
        Info.Category = Property->GetMetaData(TEXT("Category"));
        Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
        Info.bIsBlueprintVisible = Property->HasAnyPropertyFlags(CPF_BlueprintVisible);

        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
        if (ValuePtr)
        {
            Info.CurrentValue = PropertyValueToString(Property, ValuePtr);
        }
        
        Properties.Add(Info);
    }

    // Add slot properties if requested
    if (bIncludeSlotProperties)
    {
        UPanelSlot* Slot = Widget->Slot;
        if (Slot)
        {
            for (TFieldIterator<FProperty> It(Slot->GetClass()); It; ++It)
            {
                FProperty* Property = *It;
                if (!Property || Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
                {
                    continue;
                }

                FWidgetPropertyInfo Info;
                Info.PropertyName = FString::Printf(TEXT("Slot.%s"), *Property->GetName());
                Info.PropertyType = Property->GetCPPType();
                Info.Category = TEXT("Slot");
                Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);

                void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Slot);
                if (ValuePtr)
                {
                    Info.CurrentValue = PropertyValueToString(Property, ValuePtr);
                }
                
                Properties.Add(Info);
            }
        }
    }

    return TResult<TArray<FWidgetPropertyInfo>>::Success(Properties);
}

TResult<FWidgetPropertyDescriptor> FWidgetPropertyService::GetPropertyDescriptor(UWidget* Widget, const FString& PropertyPath)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<FWidgetPropertyDescriptor>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<FWidgetPropertyDescriptor>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Property '%s' not found"), *PropertyPath)
        );
    }

    FWidgetPropertyDescriptor Descriptor;
    Descriptor.Info.PropertyName = PropertyPath;
    Descriptor.Info.PropertyType = Property->GetCPPType();
    Descriptor.Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
    Descriptor.Info.CurrentValue = PropertyValueToString(Property, Container);

    return TResult<FWidgetPropertyDescriptor>::Success(Descriptor);
}

TResult<bool> FWidgetPropertyService::ValidatePropertyValue(UWidget* Widget, const FString& PropertyPath, const FString& Value)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    FProperty* Property = nullptr;
    void* Container = nullptr;
    
    if (!FindPropertyByPath(Widget, PropertyPath, Property, Container))
    {
        return TResult<bool>::Success(false);
    }

    // Basic type validation - check if property accepts the type
    if (!Property)
    {
        return TResult<bool>::Success(false);
    }

    // For now, basic validation only
    // Full implementation would validate:
    // - Numeric ranges
    // - Enum values
    // - Object references
    // - String formats
    return TResult<bool>::Success(true);
}

TResult<TArray<FString>> FWidgetPropertyService::SetPropertiesBatch(UWidget* Widget, const TArray<FWidgetPropertyUpdate>& Updates)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FString> Errors;

    for (const FWidgetPropertyUpdate& Update : Updates)
    {
        TResult<void> Result = SetWidgetProperty(Widget, Update.PropertyPath, Update.NewValue);
        if (Result.IsError())
        {
            Errors.Add(FString::Printf(TEXT("Property '%s': %s"), *Update.PropertyPath, *Result.GetErrorMessage()));
        }
    }

    return TResult<TArray<FString>>::Success(Errors);
}

TResult<TArray<FWidgetPropertyInfo>> FWidgetPropertyService::GetSlotProperties(UWidget* Widget)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<FWidgetPropertyInfo> Properties;

    UPanelSlot* Slot = Widget->Slot;
    if (!Slot)
    {
        return TResult<TArray<FWidgetPropertyInfo>>::Success(Properties);
    }

    for (TFieldIterator<FProperty> It(Slot->GetClass()); It; ++It)
    {
        FProperty* Property = *It;
        if (!Property || Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
        {
            continue;
        }

        FWidgetPropertyInfo Info;
        Info.PropertyName = Property->GetName();
        Info.PropertyType = Property->GetCPPType();
        Info.Category = TEXT("Slot");
        Info.bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit);
        
        Properties.Add(Info);
    }

    return TResult<TArray<FWidgetPropertyInfo>>::Success(Properties);
}

TResult<void> FWidgetPropertyService::SetSlotProperty(UWidget* Widget, const FString& PropertyPath, const FString& Value)
{
    auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
    if (ValidationResult.IsError())
    {
        return ValidationResult;
    }

    UPanelSlot* Slot = Widget->Slot;
    if (!Slot)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            TEXT("Widget is not in a slot")
        );
    }

    FProperty* Property = FindFProperty<FProperty>(Slot->GetClass(), *PropertyPath);
    if (!Property)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
            FString::Printf(TEXT("Slot property '%s' not found"), *PropertyPath)
        );
    }

    void* PropertyValue = Property->ContainerPtrToValuePtr<void>(Slot);
    if (!SetPropertyValueFromString(Property, PropertyValue, Value))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
            FString::Printf(TEXT("Failed to set slot property '%s'"), *PropertyPath)
        );
    }

    Slot->Modify();
    return TResult<void>::Success();
}

bool FWidgetPropertyService::FindPropertyByPath(UObject* Object, const FString& PropertyPath, FProperty*& OutProperty, void*& OutContainer)
{
    if (!Object || PropertyPath.IsEmpty())
    {
        return false;
    }

    // Handle slot properties
    if (PropertyPath.StartsWith(TEXT("Slot.")))
    {
        UWidget* Widget = Cast<UWidget>(Object);
        if (!Widget || !Widget->Slot)
        {
            return false;
        }

        FString SlotPropertyName = PropertyPath.Mid(5); // Remove "Slot."
        OutProperty = FindFProperty<FProperty>(Widget->Slot->GetClass(), *SlotPropertyName);
        if (OutProperty)
        {
            OutContainer = Widget->Slot;
            return true;
        }
        return false;
    }

    // Simple property lookup
    OutProperty = FindFProperty<FProperty>(Object->GetClass(), *PropertyPath);
    if (OutProperty)
    {
        OutContainer = Object;
        return true;
    }

    return false;
}

FString FWidgetPropertyService::PropertyValueToString(FProperty* Property, const void* ValuePtr)
{
    if (!Property || !ValuePtr)
    {
        return TEXT("");
    }

    FString ValueString;
    Property->ExportTextItem_Direct(ValueString, ValuePtr, nullptr, nullptr, PPF_None);
    return ValueString;
}

bool FWidgetPropertyService::SetPropertyValueFromString(FProperty* Property, void* ValuePtr, const FString& ValueString)
{
    if (!Property || !ValuePtr)
    {
        return false;
    }

    const TCHAR* Result = Property->ImportText_Direct(*ValueString, ValuePtr, nullptr, PPF_None);
    return Result != nullptr;
}
