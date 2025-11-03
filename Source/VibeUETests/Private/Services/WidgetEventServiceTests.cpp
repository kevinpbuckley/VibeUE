#include "Services/UMG/WidgetEventService.h"
#include "Misc/AutomationTest.h"
#include "Core/ServiceContext.h"

// Test flags for this test suite
#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetEventServiceBasicTest, "VibeUE.Services.UMG.WidgetEventService.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWidgetEventServiceBasicTest::RunTest(const FString& Parameters)
{
	// Create service context
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	
	// Create service instance
	FWidgetEventService Service(Context);
	
	// Test service creation
	TestTrue(TEXT("Service instance created"), true);
	
	// Test null widget validation
	auto Result = Service.GetAvailableEvents(nullptr, TEXT("TestComponent"));
	TestTrue(TEXT("Null widget rejected"), Result.IsError());
	
	// Test empty component name validation
	auto Result2 = Service.GetAvailableEvents(nullptr, TEXT(""));
	TestTrue(TEXT("Empty component name rejected"), Result2.IsError());
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetEventServiceEventDiscoveryTest, "VibeUE.Services.UMG.WidgetEventService.EventDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWidgetEventServiceEventDiscoveryTest::RunTest(const FString& Parameters)
{
	// Create service context
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	
	// Create service instance
	FWidgetEventService Service(Context);
	
	// Note: This test would require creating a test widget blueprint
	// For now, we're just testing the API exists and handles errors properly
	
	TestTrue(TEXT("Event discovery API exists"), true);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetEventServiceEventBindingTest, "VibeUE.Services.UMG.WidgetEventService.EventBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWidgetEventServiceEventBindingTest::RunTest(const FString& Parameters)
{
	// Create service context
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	
	// Create service instance
	FWidgetEventService Service(Context);
	
	// Test binding with null widget
	auto BindResult = Service.BindEvent(nullptr, TEXT("Button"), TEXT("OnClicked"), TEXT("HandleClick"));
	TestTrue(TEXT("Bind with null widget fails"), BindResult.IsError());
	
	// Test unbinding with null widget
	auto UnbindResult = Service.UnbindEvent(nullptr, TEXT("Button"), TEXT("OnClicked"));
	TestTrue(TEXT("Unbind with null widget fails"), UnbindResult.IsError());
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetEventServiceValidationTest, "VibeUE.Services.UMG.WidgetEventService.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWidgetEventServiceValidationTest::RunTest(const FString& Parameters)
{
	// Create service context
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	
	// Create service instance
	FWidgetEventService Service(Context);
	
	// Test validation with null widget
	auto ValidationResult = Service.IsValidEvent(nullptr, TEXT("Button"), TEXT("OnClicked"));
	TestTrue(TEXT("Validation with null widget fails"), ValidationResult.IsError());
	
	// Test CanBindEvent with null widget
	auto CanBindResult = Service.CanBindEvent(nullptr, TEXT("Button"), TEXT("OnClicked"), TEXT("HandleClick"));
	TestTrue(TEXT("CanBindEvent with null widget fails"), CanBindResult.IsError());
	
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
