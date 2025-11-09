#include "Misc/AutomationTest.h"
#include "Core/ServiceContext.h"
#include "Core/ServiceBase.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

// Test service class for testing service registration
class FTestService : public FServiceBase
{
public:
	explicit FTestService(TSharedPtr<FServiceContext> InContext)
		: FServiceBase(InContext)
		, CallCount(0)
	{
	}

	void IncrementCallCount()
	{
		CallCount++;
	}

	int32 GetCallCount() const
	{
		return CallCount;
	}

	virtual FString GetServiceName() const override
	{
		return TEXT("TestService");
	}

private:
	int32 CallCount;
};

// Runnable for thread safety tests
class FServiceContextTestRunnable : public FRunnable
{
public:
	FServiceContextTestRunnable(TSharedPtr<FServiceContext> InContext, int32 InIterations)
		: Context(InContext)
		, Iterations(InIterations)
		, bComplete(false)
	{
	}

	virtual uint32 Run() override
	{
		for (int32 i = 0; i < Iterations; ++i)
		{
			// Test concurrent config access
			FString Key = FString::Printf(TEXT("ThreadKey_%d"), i);
			FString Value = FString::Printf(TEXT("ThreadValue_%d"), i);
			Context->SetConfigValue(Key, Value);
			
			FString Retrieved = Context->GetConfigValue(Key, TEXT(""));
			check(!Retrieved.IsEmpty());
			
			// Test concurrent logging
			Context->LogInfo(TEXT("Test message"), TEXT("ThreadTest"));
		}
		
		bComplete = true;
		return 0;
	}

	bool IsComplete() const { return bComplete; }

private:
	TSharedPtr<FServiceContext> Context;
	int32 Iterations;
	bool bComplete;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextConstructorTest,
	"VibeUE.Core.ServiceContext.Constructor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextConstructorTest::RunTest(const FString& Parameters)
{
	// Arrange & Act
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Assert
	TestTrue(TEXT("ServiceContext should be created"), Context.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextLoggingTest,
	"VibeUE.Core.ServiceContext.Logging",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextLoggingTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act & Assert - Should not crash
	Context->LogInfo(TEXT("Test info message"), TEXT("TestService"));
	Context->LogWarning(TEXT("Test warning message"), TEXT("TestService"));
	Context->LogError(TEXT("Test error message"), TEXT("TestService"));

	TestTrue(TEXT("Logging should not crash"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextGetWorldTest,
	"VibeUE.Core.ServiceContext.GetWorld",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextGetWorldTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act
	UWorld* World = Context->GetWorld();

	// Assert - May be null in test environment, but should not crash
	TestTrue(TEXT("GetWorld should not crash"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextGetEditorEngineTest,
	"VibeUE.Core.ServiceContext.GetEditorEngine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextGetEditorEngineTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act
	UEditorEngine* EditorEngine = Context->GetEditorEngine();

	// Assert - May be null in test environment, but should not crash
	TestTrue(TEXT("GetEditorEngine should not crash"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextGetAssetRegistryTest,
	"VibeUE.Core.ServiceContext.GetAssetRegistry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextGetAssetRegistryTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act
	IAssetRegistry* AssetRegistry = Context->GetAssetRegistry();

	// Assert - Should not crash and should return valid pointer
	TestNotNull(TEXT("GetAssetRegistry should return valid pointer"), AssetRegistry);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextGetAssetRegistryCachingTest,
	"VibeUE.Core.ServiceContext.GetAssetRegistry.Caching",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextGetAssetRegistryCachingTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act - Get AssetRegistry twice
	IAssetRegistry* First = Context->GetAssetRegistry();
	IAssetRegistry* Second = Context->GetAssetRegistry();

	// Assert - Should return the same cached instance
	TestEqual(TEXT("GetAssetRegistry should return cached instance"), First, Second);
	TestNotNull(TEXT("Cached instance should be valid"), First);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextRegisterServiceTest,
	"VibeUE.Core.ServiceContext.RegisterService",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextRegisterServiceTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	TSharedPtr<FTestService> Service = MakeShared<FTestService>(Context);

	// Act
	Context->RegisterService(TEXT("TestService"), Service);

	// Assert
	TSharedPtr<FServiceBase> RetrievedService = Context->GetService(TEXT("TestService"));
	TestTrue(TEXT("Retrieved service should be valid"), RetrievedService.IsValid());
	TestEqual(TEXT("Retrieved service should be the same instance"), 
		RetrievedService.Get(), 
		StaticCastSharedPtr<FServiceBase>(Service).Get());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextGetServiceNotFoundTest,
	"VibeUE.Core.ServiceContext.GetService.NotFound",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextGetServiceNotFoundTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();

	// Act
	TSharedPtr<FServiceBase> Service = Context->GetService(TEXT("NonExistentService"));

	// Assert
	TestFalse(TEXT("Non-existent service should return nullptr"), Service.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextRegisterServiceUpdateTest,
	"VibeUE.Core.ServiceContext.RegisterService.Update",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextRegisterServiceUpdateTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	TSharedPtr<FTestService> Service1 = MakeShared<FTestService>(Context);
	TSharedPtr<FTestService> Service2 = MakeShared<FTestService>(Context);

	// Act - Register service, then update it with a different instance
	Context->RegisterService(TEXT("TestService"), Service1);
	Context->RegisterService(TEXT("TestService"), Service2);
	TSharedPtr<FServiceBase> RetrievedService = Context->GetService(TEXT("TestService"));

	// Assert - Should get the updated (second) service
	TestTrue(TEXT("Retrieved service should be valid"), RetrievedService.IsValid());
	TestEqual(TEXT("Retrieved service should be the updated instance"), 
		RetrievedService.Get(), 
		StaticCastSharedPtr<FServiceBase>(Service2).Get());
	TestNotEqual(TEXT("Retrieved service should not be the first instance"), 
		RetrievedService.Get(), 
		StaticCastSharedPtr<FServiceBase>(Service1).Get());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextConfigValueTest,
	"VibeUE.Core.ServiceContext.ConfigValue",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextConfigValueTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FString Key = TEXT("TestKey");
	FString Value = TEXT("TestValue");

	// Act
	Context->SetConfigValue(Key, Value);
	FString RetrievedValue = Context->GetConfigValue(Key);

	// Assert
	TestEqual(TEXT("Retrieved value should match set value"), RetrievedValue, Value);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextConfigValueDefaultTest,
	"VibeUE.Core.ServiceContext.ConfigValue.Default",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextConfigValueDefaultTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FString DefaultValue = TEXT("DefaultValue");

	// Act
	FString RetrievedValue = Context->GetConfigValue(TEXT("NonExistentKey"), DefaultValue);

	// Assert
	TestEqual(TEXT("Non-existent key should return default value"), RetrievedValue, DefaultValue);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextConfigValueUpdateTest,
	"VibeUE.Core.ServiceContext.ConfigValue.Update",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextConfigValueUpdateTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	FString Key = TEXT("TestKey");
	FString Value1 = TEXT("FirstValue");
	FString Value2 = TEXT("SecondValue");

	// Act
	Context->SetConfigValue(Key, Value1);
	Context->SetConfigValue(Key, Value2);
	FString RetrievedValue = Context->GetConfigValue(Key);

	// Assert
	TestEqual(TEXT("Updated value should be retrieved"), RetrievedValue, Value2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextThreadSafetyTest,
	"VibeUE.Core.ServiceContext.ThreadSafety",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextThreadSafetyTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	const int32 NumThreads = 4;
	const int32 IterationsPerThread = 100;
	
	TArray<TUniquePtr<FServiceContextTestRunnable>> Runnables;
	TArray<FRunnableThread*> Threads;

	// Act - Create and start multiple threads
	for (int32 i = 0; i < NumThreads; ++i)
	{
		TUniquePtr<FServiceContextTestRunnable> Runnable = MakeUnique<FServiceContextTestRunnable>(
			Context, 
			IterationsPerThread
		);
		
		FRunnableThread* Thread = FRunnableThread::Create(
			Runnable.Get(),
			*FString::Printf(TEXT("ServiceContextTest_%d"), i)
		);
		
		Runnables.Add(MoveTemp(Runnable));
		Threads.Add(Thread);
	}

	// Wait for all threads to complete
	bool bAllComplete = false;
	int32 WaitCount = 0;
	const int32 MaxWaitIterations = 100;
	
	while (!bAllComplete && WaitCount < MaxWaitIterations)
	{
		FPlatformProcess::Sleep(0.01f);
		bAllComplete = true;
		
		for (const TUniquePtr<FServiceContextTestRunnable>& Runnable : Runnables)
		{
			if (!Runnable->IsComplete())
			{
				bAllComplete = false;
				break;
			}
		}
		
		WaitCount++;
	}

	// Cleanup threads - wait for completion instead of forcing kill
	for (FRunnableThread* Thread : Threads)
	{
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
		}
	}

	// Assert
	TestTrue(TEXT("All threads should complete successfully"), bAllComplete);
	
	// Verify some config values were set
	int32 FoundValues = 0;
	for (int32 i = 0; i < IterationsPerThread; ++i)
	{
		FString Key = FString::Printf(TEXT("ThreadKey_%d"), i);
		FString Value = Context->GetConfigValue(Key, TEXT(""));
		if (!Value.IsEmpty())
		{
			FoundValues++;
		}
	}
	
	TestTrue(TEXT("Config values should be set by threads"), FoundValues > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceContextMultipleServicesTest,
	"VibeUE.Core.ServiceContext.MultipleServices",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceContextMultipleServicesTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	TSharedPtr<FTestService> Service1 = MakeShared<FTestService>(Context);
	TSharedPtr<FTestService> Service2 = MakeShared<FTestService>(Context);
	TSharedPtr<FTestService> Service3 = MakeShared<FTestService>(Context);

	// Act
	Context->RegisterService(TEXT("Service1"), Service1);
	Context->RegisterService(TEXT("Service2"), Service2);
	Context->RegisterService(TEXT("Service3"), Service3);

	// Assert
	TSharedPtr<FServiceBase> Retrieved1 = Context->GetService(TEXT("Service1"));
	TSharedPtr<FServiceBase> Retrieved2 = Context->GetService(TEXT("Service2"));
	TSharedPtr<FServiceBase> Retrieved3 = Context->GetService(TEXT("Service3"));

	TestTrue(TEXT("Service1 should be retrieved"), Retrieved1.IsValid());
	TestTrue(TEXT("Service2 should be retrieved"), Retrieved2.IsValid());
	TestTrue(TEXT("Service3 should be retrieved"), Retrieved3.IsValid());
	
	TestNotEqual(TEXT("Services should be different instances"), 
		Retrieved1.Get(), Retrieved2.Get());
	TestNotEqual(TEXT("Services should be different instances"), 
		Retrieved2.Get(), Retrieved3.Get());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBaseLoggingTest,
	"VibeUE.Core.ServiceBase.Logging",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBaseLoggingTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	TSharedPtr<FTestService> Service = MakeShared<FTestService>(Context);

	// Act & Assert - Should not crash
	Service->LogInfo(TEXT("Test info"));
	Service->LogWarning(TEXT("Test warning"));
	Service->LogError(TEXT("Test error"));

	TestTrue(TEXT("Service logging should not crash"), true);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FServiceBaseLifecycleTest,
	"VibeUE.Core.ServiceBase.Lifecycle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FServiceBaseLifecycleTest::RunTest(const FString& Parameters)
{
	// Arrange
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	TSharedPtr<FTestService> Service = MakeShared<FTestService>(Context);

	// Act & Assert - Should not crash
	Service->Initialize();
	Service->Shutdown();

	TestTrue(TEXT("Service lifecycle methods should not crash"), true);

	return true;
}
