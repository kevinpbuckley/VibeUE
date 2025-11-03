// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/UMG/WidgetStyleService.h"
#include "Core/Result.h"
#include "Core/ErrorCodes.h"
#include "Core/ServiceContext.h"
#include "Misc/AutomationTest.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

// ============================================================================
// Constructor Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_Constructor_ValidContext_Success,
	"VibeUE.Services.WidgetStyleService.Constructor.ValidContext",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_Constructor_ValidContext_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act
	FWidgetStyleService Service(Context);

	// Assert - should not crash
	TestTrue(TEXT("Service created successfully"), true);

	return true;
}

// ============================================================================
// GetAvailableStyleSets Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_GetAvailableStyleSets_ReturnsStyleSets,
	"VibeUE.Services.WidgetStyleService.StyleSets.GetAvailable",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_GetAvailableStyleSets_ReturnsStyleSets::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);

	// Act
	TResult<TArray<FString>> Result = Service.GetAvailableStyleSets();

	// Assert
	TestTrue(TEXT("Should return success"), Result.IsSuccess());
	TestTrue(TEXT("Should have at least one style set"), Result.GetValue().Num() > 0);
	TestTrue(TEXT("Should contain 'Modern' style"), Result.GetValue().Contains(TEXT("Modern")));
	TestTrue(TEXT("Should contain 'Minimal' style"), Result.GetValue().Contains(TEXT("Minimal")));
	TestTrue(TEXT("Should contain 'Dark' style"), Result.GetValue().Contains(TEXT("Dark")));
	TestTrue(TEXT("Should contain 'Vibrant' style"), Result.GetValue().Contains(TEXT("Vibrant")));

	return true;
}

// ============================================================================
// GetStyleSet Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_GetStyleSet_ValidName_Success,
	"VibeUE.Services.WidgetStyleService.StyleSets.GetValid",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_GetStyleSet_ValidName_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);

	// Act
	TResult<FWidgetStyle> Result = Service.GetStyleSet(TEXT("Modern"));

	// Assert
	TestTrue(TEXT("Should return success"), Result.IsSuccess());
	TestTrue(TEXT("Primary color should be set"), !Result.GetValue().PrimaryColor.Equals(FLinearColor::Black, 0.01f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_GetStyleSet_InvalidName_Error,
	"VibeUE.Services.WidgetStyleService.StyleSets.GetInvalid",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_GetStyleSet_InvalidName_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);

	// Act
	TResult<FWidgetStyle> Result = Service.GetStyleSet(TEXT("NonExistentStyle"));

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_GetStyleSet_EmptyName_Error,
	"VibeUE.Services.WidgetStyleService.StyleSets.GetEmpty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_GetStyleSet_EmptyName_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);

	// Act
	TResult<FWidgetStyle> Result = Service.GetStyleSet(TEXT(""));

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_EMPTY"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_EMPTY));

	return true;
}

// ============================================================================
// SetColor Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_SetColor_NullWidget_Error,
	"VibeUE.Services.WidgetStyleService.SetColor.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_SetColor_NullWidget_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);

	// Act
	TResult<void> Result = Service.SetColor(nullptr, TEXT("Component"), FLinearColor::Red);

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_SetColor_EmptyComponentName_Error,
	"VibeUE.Services.WidgetStyleService.SetColor.EmptyComponent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_SetColor_EmptyComponentName_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);
	
	// Create a minimal widget blueprint for testing
	UWidgetBlueprint* TestWidget = NewObject<UWidgetBlueprint>();

	// Act
	TResult<void> Result = Service.SetColor(TestWidget, TEXT(""), FLinearColor::Red);

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_EMPTY"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_EMPTY));

	return true;
}

// ============================================================================
// SetFontSize Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_SetFontSize_InvalidSize_Error,
	"VibeUE.Services.WidgetStyleService.SetFontSize.InvalidSize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_SetFontSize_InvalidSize_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);
	UWidgetBlueprint* TestWidget = NewObject<UWidgetBlueprint>();

	// Act
	TResult<void> Result = Service.SetFontSize(TestWidget, TEXT("Text"), -1);

	// Assert
	TestTrue(TEXT("Should return error for negative size"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_OUT_OF_RANGE"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_OUT_OF_RANGE));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_SetFontSize_SizeTooBig_Error,
	"VibeUE.Services.WidgetStyleService.SetFontSize.SizeTooBig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_SetFontSize_SizeTooBig_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);
	UWidgetBlueprint* TestWidget = NewObject<UWidgetBlueprint>();

	// Act
	TResult<void> Result = Service.SetFontSize(TestWidget, TEXT("Text"), 2000);

	// Assert
	TestTrue(TEXT("Should return error for size too big"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_OUT_OF_RANGE"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_OUT_OF_RANGE));

	return true;
}

// ============================================================================
// SetPadding Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_SetPadding_NullWidget_Error,
	"VibeUE.Services.WidgetStyleService.SetPadding.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_SetPadding_NullWidget_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);

	// Act
	TResult<void> Result = Service.SetPadding(nullptr, TEXT("Component"), FMargin(10.0f));

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));

	return true;
}

// ============================================================================
// SetAlignment Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_SetAlignment_NullWidget_Error,
	"VibeUE.Services.WidgetStyleService.SetAlignment.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_SetAlignment_NullWidget_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);

	// Act
	TResult<void> Result = Service.SetAlignment(nullptr, TEXT("Component"), HAlign_Center, VAlign_Center);

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));

	return true;
}

// ============================================================================
// ApplyStyle Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_ApplyStyle_NullWidget_Error,
	"VibeUE.Services.WidgetStyleService.ApplyStyle.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_ApplyStyle_NullWidget_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);
	FWidgetStyle Style;

	// Act
	TResult<void> Result = Service.ApplyStyle(nullptr, TEXT("Component"), Style);

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_ApplyStyle_EmptyComponentName_Error,
	"VibeUE.Services.WidgetStyleService.ApplyStyle.EmptyComponent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_ApplyStyle_EmptyComponentName_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);
	UWidgetBlueprint* TestWidget = NewObject<UWidgetBlueprint>();
	FWidgetStyle Style;

	// Act
	TResult<void> Result = Service.ApplyStyle(TestWidget, TEXT(""), Style);

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_EMPTY"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_EMPTY));

	return true;
}

// ============================================================================
// ApplyStyleSet Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetStyleService_ApplyStyleSet_InvalidStyleSet_Error,
	"VibeUE.Services.WidgetStyleService.ApplyStyleSet.InvalidStyleSet",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetStyleService_ApplyStyleSet_InvalidStyleSet_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetStyleService Service(Context);
	UWidgetBlueprint* TestWidget = NewObject<UWidgetBlueprint>();

	// Act
	TResult<void> Result = Service.ApplyStyleSet(TestWidget, TEXT("Component"), TEXT("InvalidStyle"));

	// Assert
	TestTrue(TEXT("Should return error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));

	return true;
}
