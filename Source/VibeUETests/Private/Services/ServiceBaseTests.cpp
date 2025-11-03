// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Core/ErrorCodes.h"
#include "Core/ServiceContext.h"
#include "Misc/AutomationTest.h"

/**
 * @brief Test service class for testing ServiceBase functionality
 */
class FTestService : public FServiceBase
{
public:
	explicit FTestService(TSharedPtr<FServiceContext> InContext)
		: FServiceBase(InContext)
		, bInitialized(false)
		, bShutdown(false)
	{
	}

	virtual void Initialize() override
	{
		bInitialized = true;
	}

	virtual void Shutdown() override
	{
		bShutdown = true;
	}

	// Expose protected methods for testing
	TResult<void> TestValidateNotEmpty(const FString& Value, const FString& ParamName) const
	{
		return ValidateNotEmpty(Value, ParamName);
	}

	TResult<void> TestValidateNotNull(const void* Value, const FString& ParamName) const
	{
		return ValidateNotNull(Value, ParamName);
	}

	TResult<void> TestValidateRange(int32 Value, int32 Min, int32 Max, const FString& ParamName) const
	{
		return ValidateRange(Value, Min, Max, ParamName);
	}

	TResult<void> TestValidateArray(const TArray<FString>& Value, const FString& ParamName) const
	{
		return ValidateArray(Value, ParamName);
	}

	void TestLogInfo(const FString& Message) const
	{
		LogInfo(Message);
	}

	void TestLogWarning(const FString& Message) const
	{
		LogWarning(Message);
	}

	void TestLogError(const FString& Message) const
	{
		LogError(Message);
	}

	TSharedPtr<FServiceContext> TestGetContext() const
	{
		return GetContext();
	}

	bool WasInitialized() const { return bInitialized; }
	bool WasShutdown() const { return bShutdown; }

private:
	bool bInitialized;
	bool bShutdown;
};

// Constructor Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_Constructor_ValidContext_Success,
	"VibeUE.Services.ServiceBase.Constructor.ValidContext",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_Constructor_ValidContext_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act
	FTestService Service(Context);

	// Assert
	TestNotNull(TEXT("Service context should not be null"), Service.TestGetContext().Get());

	return true;
}

// Lifecycle Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_Initialize_Called_Success,
	"VibeUE.Services.ServiceBase.Lifecycle.Initialize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_Initialize_Called_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	Service.Initialize();

	// Assert
	TestTrue(TEXT("Service should be initialized"), Service.WasInitialized());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_Shutdown_Called_Success,
	"VibeUE.Services.ServiceBase.Lifecycle.Shutdown",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_Shutdown_Called_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	Service.Shutdown();

	// Assert
	TestTrue(TEXT("Service should be shutdown"), Service.WasShutdown());

	return true;
}

// ValidateNotEmpty Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateNotEmpty_ValidString_Success,
	"VibeUE.Services.ServiceBase.Validation.NotEmpty.ValidString",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateNotEmpty_ValidString_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateNotEmpty(TEXT("ValidValue"), TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should succeed for non-empty string"), Result.IsSuccess());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateNotEmpty_EmptyString_Error,
	"VibeUE.Services.ServiceBase.Validation.NotEmpty.EmptyString",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateNotEmpty_EmptyString_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateNotEmpty(TEXT(""), TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should fail for empty string"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_EMPTY"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_EMPTY));
	TestTrue(TEXT("Error message should contain parameter name"), Result.GetErrorMessage().Contains(TEXT("TestParam")));

	return true;
}

// ValidateNotNull Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateNotNull_ValidPointer_Success,
	"VibeUE.Services.ServiceBase.Validation.NotNull.ValidPointer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateNotNull_ValidPointer_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);
	int32 ValidValue = 42;

	// Act
	TResult<void> Result = Service.TestValidateNotNull(&ValidValue, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should succeed for non-null pointer"), Result.IsSuccess());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateNotNull_NullPointer_Error,
	"VibeUE.Services.ServiceBase.Validation.NotNull.NullPointer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateNotNull_NullPointer_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateNotNull(nullptr, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should fail for null pointer"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_INVALID"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));
	TestTrue(TEXT("Error message should contain parameter name"), Result.GetErrorMessage().Contains(TEXT("TestParam")));

	return true;
}

// ValidateRange Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateRange_ValueInRange_Success,
	"VibeUE.Services.ServiceBase.Validation.Range.ValueInRange",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateRange_ValueInRange_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateRange(5, 0, 10, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should succeed for value in range"), Result.IsSuccess());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateRange_ValueAtMin_Success,
	"VibeUE.Services.ServiceBase.Validation.Range.ValueAtMin",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateRange_ValueAtMin_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateRange(0, 0, 10, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should succeed for value at minimum"), Result.IsSuccess());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateRange_ValueAtMax_Success,
	"VibeUE.Services.ServiceBase.Validation.Range.ValueAtMax",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateRange_ValueAtMax_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateRange(10, 0, 10, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should succeed for value at maximum"), Result.IsSuccess());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateRange_ValueBelowMin_Error,
	"VibeUE.Services.ServiceBase.Validation.Range.ValueBelowMin",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateRange_ValueBelowMin_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateRange(-1, 0, 10, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should fail for value below minimum"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_OUT_OF_RANGE"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_OUT_OF_RANGE));
	TestTrue(TEXT("Error message should contain parameter name"), Result.GetErrorMessage().Contains(TEXT("TestParam")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateRange_ValueAboveMax_Error,
	"VibeUE.Services.ServiceBase.Validation.Range.ValueAboveMax",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateRange_ValueAboveMax_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act
	TResult<void> Result = Service.TestValidateRange(11, 0, 10, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should fail for value above maximum"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_OUT_OF_RANGE"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_OUT_OF_RANGE));
	TestTrue(TEXT("Error message should contain parameter name"), Result.GetErrorMessage().Contains(TEXT("TestParam")));

	return true;
}

// ValidateArray Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateArray_NonEmptyArray_Success,
	"VibeUE.Services.ServiceBase.Validation.Array.NonEmpty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateArray_NonEmptyArray_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);
	TArray<FString> ValidArray = { TEXT("Item1"), TEXT("Item2") };

	// Act
	TResult<void> Result = Service.TestValidateArray(ValidArray, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should succeed for non-empty array"), Result.IsSuccess());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_ValidateArray_EmptyArray_Error,
	"VibeUE.Services.ServiceBase.Validation.Array.Empty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_ValidateArray_EmptyArray_Error::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);
	TArray<FString> EmptyArray;

	// Act
	TResult<void> Result = Service.TestValidateArray(EmptyArray, TEXT("TestParam"));

	// Assert
	TestTrue(TEXT("Validation should fail for empty array"), Result.IsError());
	TestEqual(TEXT("Error code should be PARAM_EMPTY"), Result.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_EMPTY));
	TestTrue(TEXT("Error message should contain parameter name"), Result.GetErrorMessage().Contains(TEXT("TestParam")));

	return true;
}

// Logging Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_LogInfo_Message_Success,
	"VibeUE.Services.ServiceBase.Logging.Info",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_LogInfo_Message_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act & Assert (should not crash)
	Service.TestLogInfo(TEXT("Test info message"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_LogWarning_Message_Success,
	"VibeUE.Services.ServiceBase.Logging.Warning",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_LogWarning_Message_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act & Assert (should not crash)
	Service.TestLogWarning(TEXT("Test warning message"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_LogError_Message_Success,
	"VibeUE.Services.ServiceBase.Logging.Error",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_LogError_Message_Success::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FTestService Service(Context);

	// Act & Assert (should not crash)
	Service.TestLogError(TEXT("Test error message"));

	return true;
}

// Context Access Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBase_GetContext_ReturnsValidContext,
	"VibeUE.Services.ServiceBase.Context.Get",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBase_GetContext_ReturnsValidContext::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	Context->SetLogCategoryName(TEXT("TestCategory"));
	FTestService Service(Context);

	// Act
	TSharedPtr<FServiceContext> RetrievedContext = Service.TestGetContext();

	// Assert
	TestNotNull(TEXT("Retrieved context should not be null"), RetrievedContext.Get());
	TestEqual(TEXT("Context log category should match"), RetrievedContext->GetLogCategoryName(), TEXT("TestCategory"));

	return true;
}
