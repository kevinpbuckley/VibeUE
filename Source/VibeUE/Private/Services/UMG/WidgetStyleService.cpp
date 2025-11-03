#include "Services/UMG/WidgetStyleService.h"
#include "Core/ErrorCodes.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/PanelSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"

FWidgetStyleService::FWidgetStyleService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
	InitializeStyleSets();
}

void FWidgetStyleService::InitializeStyleSets()
{
	auto AddStyle = [this](const FString& Name, const FLinearColor& Primary, const FLinearColor& Secondary, 
	                       const FMargin& Pad, EHorizontalAlignment HAlign, EVerticalAlignment VAlign)
	{
		FWidgetStyle Style;
		Style.PrimaryColor = Primary; Style.SecondaryColor = Secondary; Style.Padding = Pad;
		Style.HorizontalAlignment = HAlign; Style.VerticalAlignment = VAlign;
		StyleSets.Add(Name, Style);
	};
	AddStyle(TEXT("Modern"), FLinearColor(0.129f, 0.588f, 0.953f), FLinearColor(0.961f, 0.961f, 0.961f),
	         FMargin(16.0f, 12.0f), HAlign_Fill, VAlign_Center);
	AddStyle(TEXT("Minimal"), FLinearColor(0.2f, 0.2f, 0.2f), FLinearColor(0.95f, 0.95f, 0.95f),
	         FMargin(12.0f, 8.0f), HAlign_Fill, VAlign_Center);
	AddStyle(TEXT("Dark"), FLinearColor(0.0f, 0.8f, 1.0f), FLinearColor(0.15f, 0.15f, 0.15f),
	         FMargin(16.0f, 12.0f), HAlign_Fill, VAlign_Center);
	AddStyle(TEXT("Vibrant"), FLinearColor(1.0f, 0.341f, 0.133f), FLinearColor(1.0f, 0.922f, 0.231f),
	         FMargin(20.0f, 16.0f), HAlign_Center, VAlign_Center);
}

UWidget* FWidgetStyleService::FindWidgetComponent(UWidgetBlueprint* Widget, const FString& ComponentName)
{
	return (Widget && Widget->WidgetTree) ? Widget->WidgetTree->FindWidget(FName(*ComponentName)) : nullptr;
}

TResult<void> FWidgetStyleService::ApplyStyle(UWidgetBlueprint* Widget, const FString& ComponentName, const FWidgetStyle& Style)
{
	auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError()) return ValidationResult;

	UWidget* WidgetComponent = FindWidgetComponent(Widget, ComponentName);
	if (!WidgetComponent)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));

	if (UTextBlock* TextBlock = Cast<UTextBlock>(WidgetComponent))
		TextBlock->SetColorAndOpacity(FSlateColor(Style.PrimaryColor));
	else if (UImage* Image = Cast<UImage>(WidgetComponent))
		Image->SetColorAndOpacity(Style.PrimaryColor);
	else if (UBorder* Border = Cast<UBorder>(WidgetComponent))
		Border->SetContentColorAndOpacity(Style.PrimaryColor);

	if (UPanelSlot* Slot = WidgetComponent->Slot)
		if (FProperty* PaddingProp = Slot->GetClass()->FindPropertyByName(TEXT("Padding")))
			if (FStructProperty* StructProp = CastField<FStructProperty>(PaddingProp))
				if (StructProp->Struct->GetName() == TEXT("Margin"))
					StructProp->CopyCompleteValue(StructProp->ContainerPtrToValuePtr<void>(Slot), &Style.Padding);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	return TResult<void>::Success();
}

TResult<void> FWidgetStyleService::ApplyStyleSet(UWidgetBlueprint* Widget, const FString& ComponentName, const FString& StyleSetName)
{
	auto StyleResult = GetStyleSet(StyleSetName);
	return StyleResult.IsError() ? TResult<void>::Error(StyleResult.GetErrorCode(), StyleResult.GetErrorMessage()) 
	                              : ApplyStyle(Widget, ComponentName, StyleResult.GetValue());
}

TResult<void> FWidgetStyleService::SetColor(UWidgetBlueprint* Widget, const FString& ComponentName, const FLinearColor& Color)
{
	auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError()) return ValidationResult;

	UWidget* WidgetComponent = FindWidgetComponent(Widget, ComponentName);
	if (!WidgetComponent)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));

	bool bApplied = false;
	if (UTextBlock* TextBlock = Cast<UTextBlock>(WidgetComponent))
		TextBlock->SetColorAndOpacity(FSlateColor(Color)), bApplied = true;
	else if (UBorder* Border = Cast<UBorder>(WidgetComponent))
		Border->SetBrushColor(Color), bApplied = true;

	if (!bApplied)
		return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
			FString::Printf(TEXT("Widget component '%s' does not support color property"), *ComponentName));

	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	return TResult<void>::Success();
}

TResult<void> FWidgetStyleService::SetColorAndOpacity(UWidgetBlueprint* Widget, const FString& ComponentName, const FLinearColor& Color)
{
	auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError()) return ValidationResult;

	UWidget* WidgetComponent = FindWidgetComponent(Widget, ComponentName);
	if (!WidgetComponent)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));

	bool bApplied = false;
	if (UImage* Image = Cast<UImage>(WidgetComponent))
		Image->SetColorAndOpacity(Color), bApplied = true;
	else if (UTextBlock* TextBlock = Cast<UTextBlock>(WidgetComponent))
		TextBlock->SetColorAndOpacity(FSlateColor(Color)), bApplied = true;
	else if (UBorder* Border = Cast<UBorder>(WidgetComponent))
		Border->SetContentColorAndOpacity(Color), bApplied = true;

	if (!bApplied)
		return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_SET_FAILED,
			FString::Printf(TEXT("Widget component '%s' does not support color and opacity property"), *ComponentName));

	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	return TResult<void>::Success();
}

TResult<void> FWidgetStyleService::SetFont(UWidgetBlueprint* Widget, const FString& ComponentName, const FSlateFontInfo& Font)
{
	auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError()) return ValidationResult;

	UWidget* WidgetComponent = FindWidgetComponent(Widget, ComponentName);
	if (!WidgetComponent)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));

	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetComponent);
	if (!TextBlock)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
			FString::Printf(TEXT("Widget component '%s' is not a text widget"), *ComponentName));

	TextBlock->SetFont(Font);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	return TResult<void>::Success();
}

TResult<void> FWidgetStyleService::SetFontSize(UWidgetBlueprint* Widget, const FString& ComponentName, int32 Size)
{
	auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateRange(Size, 1, 1000, TEXT("Size"));
	if (ValidationResult.IsError()) return ValidationResult;

	UWidget* WidgetComponent = FindWidgetComponent(Widget, ComponentName);
	if (!WidgetComponent)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
			FString::Printf(TEXT("Widget component '%s' not found"), *ComponentName));

	UTextBlock* TextBlock = Cast<UTextBlock>(WidgetComponent);
	if (!TextBlock)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_TYPE_INVALID,
			FString::Printf(TEXT("Widget component '%s' is not a text widget"), *ComponentName));

	FSlateFontInfo FontInfo = TextBlock->GetFont();
	FontInfo.Size = Size;
	TextBlock->SetFont(FontInfo);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	return TResult<void>::Success();
}

TResult<void> FWidgetStyleService::SetPadding(UWidgetBlueprint* Widget, const FString& ComponentName, const FMargin& Padding)
{
	auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError()) return ValidationResult;

	UWidget* WidgetComponent = FindWidgetComponent(Widget, ComponentName);
	if (!WidgetComponent || !WidgetComponent->Slot)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
			FString::Printf(TEXT("Widget component '%s' not found or not in slot"), *ComponentName));

	FProperty* PaddingProp = WidgetComponent->Slot->GetClass()->FindPropertyByName(TEXT("Padding"));
	if (!PaddingProp)
		return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
			FString::Printf(TEXT("Slot for widget '%s' does not support padding"), *ComponentName));

	FStructProperty* StructProp = CastField<FStructProperty>(PaddingProp);
	if (!StructProp || StructProp->Struct->GetName() != TEXT("Margin"))
		return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_TYPE_MISMATCH, TEXT("Padding property is not of type FMargin"));

	StructProp->CopyCompleteValue(StructProp->ContainerPtrToValuePtr<void>(WidgetComponent->Slot), &Padding);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	return TResult<void>::Success();
}

TResult<void> FWidgetStyleService::SetAlignment(UWidgetBlueprint* Widget, const FString& ComponentName,
                                                EHorizontalAlignment HAlign, EVerticalAlignment VAlign)
{
	auto ValidationResult = ValidateNotNull(Widget, TEXT("Widget"));
	if (ValidationResult.IsError()) return ValidationResult;
	ValidationResult = ValidateNotEmpty(ComponentName, TEXT("ComponentName"));
	if (ValidationResult.IsError()) return ValidationResult;

	UWidget* WidgetComponent = FindWidgetComponent(Widget, ComponentName);
	if (!WidgetComponent || !WidgetComponent->Slot)
		return TResult<void>::Error(VibeUE::ErrorCodes::WIDGET_COMPONENT_NOT_FOUND,
			FString::Printf(TEXT("Widget component '%s' not found or not in slot"), *ComponentName));

	bool bSet = false;
	if (FProperty* HAlignProp = WidgetComponent->Slot->GetClass()->FindPropertyByName(TEXT("HorizontalAlignment")))
		if (CastField<FEnumProperty>(HAlignProp) || CastField<FByteProperty>(HAlignProp))
			*HAlignProp->ContainerPtrToValuePtr<uint8>(WidgetComponent->Slot) = static_cast<uint8>(HAlign), bSet = true;
	if (FProperty* VAlignProp = WidgetComponent->Slot->GetClass()->FindPropertyByName(TEXT("VerticalAlignment")))
		if (CastField<FEnumProperty>(VAlignProp) || CastField<FByteProperty>(VAlignProp))
			*VAlignProp->ContainerPtrToValuePtr<uint8>(WidgetComponent->Slot) = static_cast<uint8>(VAlign), bSet = true;

	if (!bSet)
		return TResult<void>::Error(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND,
			FString::Printf(TEXT("Slot for widget '%s' does not support alignment"), *ComponentName));

	FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	return TResult<void>::Success();
}

TResult<TArray<FString>> FWidgetStyleService::GetAvailableStyleSets()
{
	TArray<FString> StyleSetNames;
	StyleSets.GenerateKeyArray(StyleSetNames);
	return TResult<TArray<FString>>::Success(StyleSetNames);
}

TResult<FWidgetStyle> FWidgetStyleService::GetStyleSet(const FString& StyleSetName)
{
	auto ValidationResult = ValidateNotEmpty(StyleSetName, TEXT("StyleSetName"));
	if (ValidationResult.IsError())
		return TResult<FWidgetStyle>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());

	const FWidgetStyle* Style = StyleSets.Find(StyleSetName);
	return Style ? TResult<FWidgetStyle>::Success(*Style) 
	             : TResult<FWidgetStyle>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
	                 FString::Printf(TEXT("Style set '%s' not found"), *StyleSetName));
}

