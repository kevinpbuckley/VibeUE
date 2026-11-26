// Copyright Buckley Builds LLC 2026 All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "PythonAPI/BlueprintTypeParser.h"
#include "EdGraphSchema_K2.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeMapReadbackTest, "VibeUE.BlueprintService.MapReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeMapReadbackTest::RunTest(const FString&)
{
	FEdGraphPinType Map;
	Map.ContainerType = EPinContainerType::Map;
	Map.PinCategory = UEdGraphSchema_K2::PC_Name;
	Map.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int64;
	TestEqual(TEXT("shows the key followed by the actual value"), FBlueprintTypeParser::GetFriendlyTypeName(Map), FString(TEXT("TMap<FName, int64>")));
	return true;
}
#endif
