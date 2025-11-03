// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/UMG/WidgetReflectionService.h"
#include "Core/Result.h"
#include "Core/ErrorCodes.h"
#include "Core/ServiceContext.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceInitializationTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.Initialization", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceInitializationTest::RunTest(const FString& Parameters)
{
    // Create service context
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    
    // Create service
    FWidgetReflectionService Service(Context);
    
    TestTrue(TEXT("Service should be created successfully"), true);
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceGetAvailableWidgetTypesTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.GetAvailableWidgetTypes", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceGetAvailableWidgetTypesTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test getting all widget types
    auto Result = Service.GetAvailableWidgetTypes();
    TestTrue(TEXT("GetAvailableWidgetTypes should succeed"), Result.IsSuccess());
    TestTrue(TEXT("Should return widget types"), Result.GetValue().Num() > 0);
    TestTrue(TEXT("Should contain Button"), Result.GetValue().Contains(TEXT("Button")));
    TestTrue(TEXT("Should contain TextBlock"), Result.GetValue().Contains(TEXT("TextBlock")));
    TestTrue(TEXT("Should contain CanvasPanel"), Result.GetValue().Contains(TEXT("CanvasPanel")));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceGetWidgetCategoriesTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.GetWidgetCategories", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceGetWidgetCategoriesTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    auto Result = Service.GetWidgetCategories();
    TestTrue(TEXT("GetWidgetCategories should succeed"), Result.IsSuccess());
    TestTrue(TEXT("Should return categories"), Result.GetValue().Num() > 0);
    TestTrue(TEXT("Should contain Panel category"), Result.GetValue().Contains(TEXT("Panel")));
    TestTrue(TEXT("Should contain Common category"), Result.GetValue().Contains(TEXT("Common")));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceGetPanelWidgetsTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.GetPanelWidgets", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceGetPanelWidgetsTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    auto Result = Service.GetPanelWidgets();
    TestTrue(TEXT("GetPanelWidgets should succeed"), Result.IsSuccess());
    TestTrue(TEXT("Should return panel widgets"), Result.GetValue().Num() > 0);
    TestTrue(TEXT("Should contain CanvasPanel"), Result.GetValue().Contains(TEXT("CanvasPanel")));
    TestTrue(TEXT("Should contain VerticalBox"), Result.GetValue().Contains(TEXT("VerticalBox")));
    TestTrue(TEXT("Should contain HorizontalBox"), Result.GetValue().Contains(TEXT("HorizontalBox")));
    TestFalse(TEXT("Should not contain Button (not a panel)"), Result.GetValue().Contains(TEXT("Button")));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceGetCommonWidgetsTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.GetCommonWidgets", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceGetCommonWidgetsTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    auto Result = Service.GetCommonWidgets();
    TestTrue(TEXT("GetCommonWidgets should succeed"), Result.IsSuccess());
    TestTrue(TEXT("Should return common widgets"), Result.GetValue().Num() > 0);
    TestTrue(TEXT("Should contain Button"), Result.GetValue().Contains(TEXT("Button")));
    TestTrue(TEXT("Should contain TextBlock"), Result.GetValue().Contains(TEXT("TextBlock")));
    TestTrue(TEXT("Should contain Image"), Result.GetValue().Contains(TEXT("Image")));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceIsValidWidgetTypeTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.IsValidWidgetType", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceIsValidWidgetTypeTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test valid widget types
    auto ButtonResult = Service.IsValidWidgetType(TEXT("Button"));
    TestTrue(TEXT("Button should be valid"), ButtonResult.IsSuccess());
    TestTrue(TEXT("Button should return true"), ButtonResult.GetValue());
    
    auto TextBlockResult = Service.IsValidWidgetType(TEXT("TextBlock"));
    TestTrue(TEXT("TextBlock should be valid"), TextBlockResult.IsSuccess());
    TestTrue(TEXT("TextBlock should return true"), TextBlockResult.GetValue());
    
    // Test invalid widget type
    auto InvalidResult = Service.IsValidWidgetType(TEXT("InvalidWidgetType"));
    TestTrue(TEXT("InvalidWidgetType check should succeed"), InvalidResult.IsSuccess());
    TestFalse(TEXT("InvalidWidgetType should return false"), InvalidResult.GetValue());
    
    // Test empty widget type
    auto EmptyResult = Service.IsValidWidgetType(TEXT(""));
    TestTrue(TEXT("Empty widget type should return error"), EmptyResult.IsError());
    TestEqual(TEXT("Should return PARAM_EMPTY error"), EmptyResult.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_EMPTY));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceIsPanelWidgetTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.IsPanelWidget", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceIsPanelWidgetTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test panel widgets
    auto CanvasResult = Service.IsPanelWidget(TEXT("CanvasPanel"));
    TestTrue(TEXT("CanvasPanel check should succeed"), CanvasResult.IsSuccess());
    TestTrue(TEXT("CanvasPanel should be a panel widget"), CanvasResult.GetValue());
    
    auto VerticalBoxResult = Service.IsPanelWidget(TEXT("VerticalBox"));
    TestTrue(TEXT("VerticalBox check should succeed"), VerticalBoxResult.IsSuccess());
    TestTrue(TEXT("VerticalBox should be a panel widget"), VerticalBoxResult.GetValue());
    
    // Test non-panel widgets
    auto ButtonResult = Service.IsPanelWidget(TEXT("Button"));
    TestTrue(TEXT("Button check should succeed"), ButtonResult.IsSuccess());
    TestFalse(TEXT("Button should not be a panel widget"), ButtonResult.GetValue());
    
    auto TextBlockResult = Service.IsPanelWidget(TEXT("TextBlock"));
    TestTrue(TEXT("TextBlock check should succeed"), TextBlockResult.IsSuccess());
    TestFalse(TEXT("TextBlock should not be a panel widget"), TextBlockResult.GetValue());
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceCanContainChildrenTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.CanContainChildren", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceCanContainChildrenTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test widgets that can contain children
    auto CanvasResult = Service.CanContainChildren(TEXT("CanvasPanel"));
    TestTrue(TEXT("CanvasPanel check should succeed"), CanvasResult.IsSuccess());
    TestTrue(TEXT("CanvasPanel can contain children"), CanvasResult.GetValue());
    
    // Test widgets that cannot contain children
    auto ButtonResult = Service.CanContainChildren(TEXT("Button"));
    TestTrue(TEXT("Button check should succeed"), ButtonResult.IsSuccess());
    TestFalse(TEXT("Button cannot contain children"), ButtonResult.GetValue());
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceGetWidgetTypeInfoTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.GetWidgetTypeInfo", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceGetWidgetTypeInfoTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test getting info for Button
    auto ButtonResult = Service.GetWidgetTypeInfo(TEXT("Button"));
    TestTrue(TEXT("GetWidgetTypeInfo for Button should succeed"), ButtonResult.IsSuccess());
    
    FWidgetTypeInfo ButtonInfo = ButtonResult.GetValue();
    TestEqual(TEXT("Button type name should be correct"), ButtonInfo.TypeName, FString(TEXT("Button")));
    TestFalse(TEXT("Button should not be a panel widget"), ButtonInfo.bIsPanelWidget);
    TestTrue(TEXT("Button should be a common widget"), ButtonInfo.bIsCommonWidget);
    TestFalse(TEXT("Button class path should not be empty"), ButtonInfo.ClassPath.IsEmpty());
    
    // Test getting info for CanvasPanel
    auto CanvasResult = Service.GetWidgetTypeInfo(TEXT("CanvasPanel"));
    TestTrue(TEXT("GetWidgetTypeInfo for CanvasPanel should succeed"), CanvasResult.IsSuccess());
    
    FWidgetTypeInfo CanvasInfo = CanvasResult.GetValue();
    TestEqual(TEXT("CanvasPanel type name should be correct"), CanvasInfo.TypeName, FString(TEXT("CanvasPanel")));
    TestTrue(TEXT("CanvasPanel should be a panel widget"), CanvasInfo.bIsPanelWidget);
    TestEqual(TEXT("CanvasPanel category should be Panel"), CanvasInfo.Category, FString(TEXT("Panel")));
    
    // Test invalid widget type
    auto InvalidResult = Service.GetWidgetTypeInfo(TEXT("InvalidWidget"));
    TestTrue(TEXT("GetWidgetTypeInfo for invalid widget should fail"), InvalidResult.IsError());
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceGetWidgetTypePathTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.GetWidgetTypePath", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceGetWidgetTypePathTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test getting path for Button
    auto ButtonResult = Service.GetWidgetTypePath(TEXT("Button"));
    TestTrue(TEXT("GetWidgetTypePath for Button should succeed"), ButtonResult.IsSuccess());
    TestEqual(TEXT("Button path should be correct"), ButtonResult.GetValue(), FString(TEXT("/Script/UMG.Button")));
    
    // Test getting path for TextBlock
    auto TextBlockResult = Service.GetWidgetTypePath(TEXT("TextBlock"));
    TestTrue(TEXT("GetWidgetTypePath for TextBlock should succeed"), TextBlockResult.IsSuccess());
    TestEqual(TEXT("TextBlock path should be correct"), TextBlockResult.GetValue(), FString(TEXT("/Script/UMG.TextBlock")));
    
    // Test invalid widget type
    auto InvalidResult = Service.GetWidgetTypePath(TEXT("InvalidWidget"));
    TestTrue(TEXT("GetWidgetTypePath for invalid widget should fail"), InvalidResult.IsError());
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceGetWidgetTypeEventsTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.GetWidgetTypeEvents", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceGetWidgetTypeEventsTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test Button events
    auto ButtonResult = Service.GetWidgetTypeEvents(TEXT("Button"));
    TestTrue(TEXT("GetWidgetTypeEvents for Button should succeed"), ButtonResult.IsSuccess());
    TestTrue(TEXT("Button should have OnClicked event"), ButtonResult.GetValue().Contains(TEXT("OnClicked")));
    TestTrue(TEXT("Button should have OnPressed event"), ButtonResult.GetValue().Contains(TEXT("OnPressed")));
    TestTrue(TEXT("Button should have OnReleased event"), ButtonResult.GetValue().Contains(TEXT("OnReleased")));
    
    // Test Slider events
    auto SliderResult = Service.GetWidgetTypeEvents(TEXT("Slider"));
    TestTrue(TEXT("GetWidgetTypeEvents for Slider should succeed"), SliderResult.IsSuccess());
    TestTrue(TEXT("Slider should have OnValueChanged event"), SliderResult.GetValue().Contains(TEXT("OnValueChanged")));
    
    // All widgets should have OnVisibilityChanged
    auto TextBlockResult = Service.GetWidgetTypeEvents(TEXT("TextBlock"));
    TestTrue(TEXT("GetWidgetTypeEvents for TextBlock should succeed"), TextBlockResult.IsSuccess());
    TestTrue(TEXT("TextBlock should have OnVisibilityChanged event"), TextBlockResult.GetValue().Contains(TEXT("OnVisibilityChanged")));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetReflectionServiceFilterByCategoryTest, 
    "VibeUE.Services.UMG.WidgetReflectionService.FilterByCategory", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetReflectionServiceFilterByCategoryTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FWidgetReflectionService Service(Context);
    
    // Test Panel category
    auto PanelResult = Service.GetAvailableWidgetTypes(TEXT("Panel"));
    TestTrue(TEXT("GetAvailableWidgetTypes with Panel category should succeed"), PanelResult.IsSuccess());
    TestTrue(TEXT("Panel category should contain CanvasPanel"), PanelResult.GetValue().Contains(TEXT("CanvasPanel")));
    TestFalse(TEXT("Panel category should not contain Button"), PanelResult.GetValue().Contains(TEXT("Button")));
    
    // Test Common category
    auto CommonResult = Service.GetAvailableWidgetTypes(TEXT("Common"));
    TestTrue(TEXT("GetAvailableWidgetTypes with Common category should succeed"), CommonResult.IsSuccess());
    TestTrue(TEXT("Common category should contain Button"), CommonResult.GetValue().Contains(TEXT("Button")));
    TestTrue(TEXT("Common category should contain TextBlock"), CommonResult.GetValue().Contains(TEXT("TextBlock")));
    
    // Test invalid category
    auto InvalidResult = Service.GetAvailableWidgetTypes(TEXT("InvalidCategory"));
    TestTrue(TEXT("GetAvailableWidgetTypes with invalid category should fail"), InvalidResult.IsError());
    TestEqual(TEXT("Should return PARAM_INVALID error"), InvalidResult.GetErrorCode(), FString(VibeUE::ErrorCodes::PARAM_INVALID));
    
    return true;
}
