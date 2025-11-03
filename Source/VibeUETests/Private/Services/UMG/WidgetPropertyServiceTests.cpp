// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/UMG/WidgetPropertyService.h"
#include "Core/ServiceContext.h"
#include "Misc/AutomationTest.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"

/**
 * Test fixture for WidgetPropertyService tests
 */
class FWidgetPropertyServiceTestFixture
{
public:
	TSharedPtr<FServiceContext> Context;
	TSharedPtr<FWidgetPropertyService> Service;

	void Setup()
	{
		Context = MakeShared<FServiceContext>();
		Service = MakeShared<FWidgetPropertyService>(Context);
	}

	void Teardown()
	{
		Service.Reset();
		Context.Reset();
	}
};

// Constructor Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_Constructor_ValidContext_Success,
	"VibeUE.Services.UMG.WidgetPropertyService.Constructor.ValidContext",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_Constructor_ValidContext_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act
	FWidgetPropertyService Service(Context);

	// Assert
	TestTrue(TEXT("Service should be constructed successfully"), true);

	return true;
}

// ValidatePropertyValue Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_ValidatePropertyValue_BoolType_ValidValue_ReturnsTrue,
	"VibeUE.Services.UMG.WidgetPropertyService.ValidatePropertyValue.BoolType.ValidValue",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_ValidatePropertyValue_BoolType_ValidValue_ReturnsTrue::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<bool> Result1 = Fixture.Service->ValidatePropertyValue(TEXT("bool"), TEXT("true"));
	TResult<bool> Result2 = Fixture.Service->ValidatePropertyValue(TEXT("bool"), TEXT("false"));
	TResult<bool> Result3 = Fixture.Service->ValidatePropertyValue(TEXT("bool"), TEXT("1"));
	TResult<bool> Result4 = Fixture.Service->ValidatePropertyValue(TEXT("bool"), TEXT("0"));

	// Assert
	TestTrue(TEXT("Result1 should be success"), Result1.IsSuccess());
	TestTrue(TEXT("Result1 value should be true"), Result1.GetValue());
	TestTrue(TEXT("Result2 should be success"), Result2.IsSuccess());
	TestTrue(TEXT("Result2 value should be true"), Result2.GetValue());
	TestTrue(TEXT("Result3 should be success"), Result3.IsSuccess());
	TestTrue(TEXT("Result3 value should be true"), Result3.GetValue());
	TestTrue(TEXT("Result4 should be success"), Result4.IsSuccess());
	TestTrue(TEXT("Result4 value should be true"), Result4.GetValue());

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_ValidatePropertyValue_BoolType_InvalidValue_ReturnsFalse,
	"VibeUE.Services.UMG.WidgetPropertyService.ValidatePropertyValue.BoolType.InvalidValue",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_ValidatePropertyValue_BoolType_InvalidValue_ReturnsFalse::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<bool> Result = Fixture.Service->ValidatePropertyValue(TEXT("bool"), TEXT("invalid"));

	// Assert
	TestTrue(TEXT("Result should be success"), Result.IsSuccess());
	TestFalse(TEXT("Result value should be false for invalid bool"), Result.GetValue());

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_ValidatePropertyValue_IntType_ValidValue_ReturnsTrue,
	"VibeUE.Services.UMG.WidgetPropertyService.ValidatePropertyValue.IntType.ValidValue",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_ValidatePropertyValue_IntType_ValidValue_ReturnsTrue::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<bool> Result1 = Fixture.Service->ValidatePropertyValue(TEXT("int"), TEXT("123"));
	TResult<bool> Result2 = Fixture.Service->ValidatePropertyValue(TEXT("int32"), TEXT("456"));

	// Assert
	TestTrue(TEXT("Result1 should be success"), Result1.IsSuccess());
	TestTrue(TEXT("Result1 value should be true"), Result1.GetValue());
	TestTrue(TEXT("Result2 should be success"), Result2.IsSuccess());
	TestTrue(TEXT("Result2 value should be true"), Result2.GetValue());

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_ValidatePropertyValue_FloatType_ValidValue_ReturnsTrue,
	"VibeUE.Services.UMG.WidgetPropertyService.ValidatePropertyValue.FloatType.ValidValue",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_ValidatePropertyValue_FloatType_ValidValue_ReturnsTrue::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<bool> Result1 = Fixture.Service->ValidatePropertyValue(TEXT("float"), TEXT("123.45"));
	TResult<bool> Result2 = Fixture.Service->ValidatePropertyValue(TEXT("float"), TEXT("456"));

	// Assert
	TestTrue(TEXT("Result1 should be success"), Result1.IsSuccess());
	TestTrue(TEXT("Result1 value should be true"), Result1.GetValue());
	TestTrue(TEXT("Result2 should be success"), Result2.IsSuccess());
	TestTrue(TEXT("Result2 value should be true"), Result2.GetValue());

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_ValidatePropertyValue_StringType_AlwaysValid,
	"VibeUE.Services.UMG.WidgetPropertyService.ValidatePropertyValue.StringType.AlwaysValid",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_ValidatePropertyValue_StringType_AlwaysValid::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<bool> Result1 = Fixture.Service->ValidatePropertyValue(TEXT("String"), TEXT("any value"));
	TResult<bool> Result2 = Fixture.Service->ValidatePropertyValue(TEXT("Text"), TEXT("any value"));

	// Assert
	TestTrue(TEXT("Result1 should be success"), Result1.IsSuccess());
	TestTrue(TEXT("String type should always be valid"), Result1.GetValue());
	TestTrue(TEXT("Result2 should be success"), Result2.IsSuccess());
	TestTrue(TEXT("Text type should always be valid"), Result2.GetValue());

	Fixture.Teardown();
	return true;
}

// Validation Tests for Invalid Parameters
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_GetProperty_NullWidget_ReturnsError,
	"VibeUE.Services.UMG.WidgetPropertyService.GetProperty.NullWidget.ReturnsError",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_GetProperty_NullWidget_ReturnsError::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<FString> Result = Fixture.Service->GetProperty(nullptr, TEXT("ComponentName"), TEXT("PropertyName"));

	// Assert
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), VibeUE::ErrorCodes::PARAM_INVALID);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_GetProperty_EmptyComponentName_ReturnsError,
	"VibeUE.Services.UMG.WidgetPropertyService.GetProperty.EmptyComponentName.ReturnsError",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_GetProperty_EmptyComponentName_ReturnsError::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();
	
	// Create a dummy widget blueprint (this won't be null but won't have components)
	UWidgetBlueprint* DummyWidget = NewObject<UWidgetBlueprint>();

	// Act
	TResult<FString> Result = Fixture.Service->GetProperty(DummyWidget, TEXT(""), TEXT("PropertyName"));

	// Assert
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_EMPTY"), Result.GetErrorCode(), VibeUE::ErrorCodes::PARAM_EMPTY);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_SetProperty_NullWidget_ReturnsError,
	"VibeUE.Services.UMG.WidgetPropertyService.SetProperty.NullWidget.ReturnsError",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_SetProperty_NullWidget_ReturnsError::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<void> Result = Fixture.Service->SetProperty(nullptr, TEXT("ComponentName"), TEXT("PropertyName"), TEXT("Value"));

	// Assert
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), VibeUE::ErrorCodes::PARAM_INVALID);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_IsValidProperty_NullWidget_ReturnsError,
	"VibeUE.Services.UMG.WidgetPropertyService.IsValidProperty.NullWidget.ReturnsError",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_IsValidProperty_NullWidget_ReturnsError::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();

	// Act
	TResult<bool> Result = Fixture.Service->IsValidProperty(nullptr, TEXT("ComponentName"), TEXT("PropertyName"));

	// Assert
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), VibeUE::ErrorCodes::PARAM_INVALID);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetPropertyService_GetPropertyMetadata_EmptyPropertyName_ReturnsError,
	"VibeUE.Services.UMG.WidgetPropertyService.GetPropertyMetadata.EmptyPropertyName.ReturnsError",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetPropertyService_GetPropertyMetadata_EmptyPropertyName_ReturnsError::RunTest(const FString& Parameters)
{
	// Arrange
	FWidgetPropertyServiceTestFixture Fixture;
	Fixture.Setup();
	
	UWidgetBlueprint* DummyWidget = NewObject<UWidgetBlueprint>();

	// Act
	TResult<FPropertyInfo> Result = Fixture.Service->GetPropertyMetadata(DummyWidget, TEXT("ComponentName"), TEXT(""));

	// Assert
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_EMPTY"), Result.GetErrorCode(), VibeUE::ErrorCodes::PARAM_EMPTY);

	Fixture.Teardown();
	return true;
}
