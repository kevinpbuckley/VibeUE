// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/UMG/WidgetLifecycleService.h"
#include "Core/Result.h"
#include "Core/ErrorCodes.h"
#include "Core/ServiceContext.h"
#include "Misc/AutomationTest.h"
#include "WidgetBlueprint.h"

// Test: Constructor with valid context
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_Constructor_ValidContext_Success,
	"VibeUE.Services.WidgetLifecycleService.Constructor.ValidContext",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_Constructor_ValidContext_Success::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TestTrue(TEXT("Service should be created successfully"), true);
	return true;
}

// Test: IsWidgetValid with null widget returns false
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_IsWidgetValid_NullWidget_ReturnsFalse,
	"VibeUE.Services.WidgetLifecycleService.IsWidgetValid.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_IsWidgetValid_NullWidget_ReturnsFalse::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<bool> Result = Service.IsWidgetValid(nullptr);
	
	TestTrue(TEXT("Result should be success"), Result.IsSuccess());
	TestFalse(TEXT("Null widget should be invalid"), Result.GetValue());
	return true;
}

// Test: ValidateWidget with null widget returns error
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_ValidateWidget_NullWidget_ReturnsError,
	"VibeUE.Services.WidgetLifecycleService.ValidateWidget.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_ValidateWidget_NullWidget_ReturnsError::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<TArray<FString>> Result = Service.ValidateWidget(nullptr);
	
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), 
		Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));
	return true;
}

// Test: ValidateHierarchy with null widget returns error
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_ValidateHierarchy_NullWidget_ReturnsError,
	"VibeUE.Services.WidgetLifecycleService.ValidateHierarchy.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_ValidateHierarchy_NullWidget_ReturnsError::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<void> Result = Service.ValidateHierarchy(nullptr);
	
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), 
		Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));
	return true;
}

// Test: GetWidgetInfo with null widget returns error
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_GetWidgetInfo_NullWidget_ReturnsError,
	"VibeUE.Services.WidgetLifecycleService.GetWidgetInfo.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_GetWidgetInfo_NullWidget_ReturnsError::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<FWidgetInfo> Result = Service.GetWidgetInfo(nullptr);
	
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), 
		Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));
	return true;
}

// Test: GetWidgetCategories with null widget returns error
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_GetWidgetCategories_NullWidget_ReturnsError,
	"VibeUE.Services.WidgetLifecycleService.GetWidgetCategories.NullWidget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_GetWidgetCategories_NullWidget_ReturnsError::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<TArray<FString>> Result = Service.GetWidgetCategories(nullptr);
	
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), 
		Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));
	return true;
}

// Test: OpenWidgetInEditor returns not supported error
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_OpenWidgetInEditor_ReturnsNotSupported,
	"VibeUE.Services.WidgetLifecycleService.OpenWidgetInEditor.NotSupported",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_OpenWidgetInEditor_ReturnsNotSupported::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<void> Result = Service.OpenWidgetInEditor(TEXT("TestWidget"));
	
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be OPERATION_NOT_SUPPORTED"), 
		Result.GetErrorCode(), FString(VibeUE::ErrorCodes::OPERATION_NOT_SUPPORTED));
	return true;
}

// Test: IsWidgetOpen returns false
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_IsWidgetOpen_ReturnsFalse,
	"VibeUE.Services.WidgetLifecycleService.IsWidgetOpen.ReturnsFalse",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_IsWidgetOpen_ReturnsFalse::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<bool> Result = Service.IsWidgetOpen(TEXT("TestWidget"));
	
	TestTrue(TEXT("Result should be success"), Result.IsSuccess());
	TestFalse(TEXT("Widget should not be open"), Result.GetValue());
	return true;
}

// Test: CloseWidget returns not supported error
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWidgetLifecycleService_CloseWidget_ReturnsNotSupported,
	"VibeUE.Services.WidgetLifecycleService.CloseWidget.NotSupported",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FWidgetLifecycleService_CloseWidget_ReturnsNotSupported::RunTest(const FString& Parameters)
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FWidgetLifecycleService Service(Context);
	
	TResult<void> Result = Service.CloseWidget(TEXT("TestWidget"));
	
	TestTrue(TEXT("Result should be error"), Result.IsError());
	TestEqual(TEXT("Error code should be OPERATION_NOT_SUPPORTED"), 
		Result.GetErrorCode(), FString(VibeUE::ErrorCodes::OPERATION_NOT_SUPPORTED));
	return true;
}
