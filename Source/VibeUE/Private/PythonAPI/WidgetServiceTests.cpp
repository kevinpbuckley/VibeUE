// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UWidgetService.h"
#include "PythonAPI/WidgetColorParsing.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Dom/JsonObject.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WidgetBlueprint.h"

static const EAutomationTestFlags kWidgetTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// ---------------------------------------------------------------------------
// Colour parsing (headless — the same ParseColor SetColor uses)
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeWidgetColorParsingTest,
	"VibeUE.Widget.ColorParsing", kWidgetTestFlags)
bool FVibeWidgetColorParsingTest::RunTest(const FString&)
{
	using namespace WidgetColorParsing;

	FLinearColor C;
	FString Error;

	// Hex is sRGB and must be converted to linear: #8B0000 is ~0.258 linear red, not 0.545.
	TestTrue(TEXT("hex parses"), ParseColor(TEXT("#8B0000"), C, Error));
	TestEqual(TEXT("hex -> linear"), C, FLinearColor::FromSRGBColor(FColor(0x8B, 0, 0, 255)));
	TestTrue(TEXT("hex really is linearised"), C.R < 0.3f);

	TestTrue(TEXT("hex without # parses"), ParseColor(TEXT("8B0000"), C, Error));
	TestTrue(TEXT("hex with alpha parses"), ParseColor(TEXT("#FF000080"), C, Error));
	TestEqual(TEXT("hex alpha"), C.A, 128.0f / 255.0f, 1e-4f);

	TestTrue(TEXT("sRGB() parses"), ParseColor(TEXT("sRGB(229, 57, 53)"), C, Error));
	TestEqual(TEXT("sRGB() -> linear"), C, FLinearColor::FromSRGBColor(FColor(229, 57, 53, 255)));

	TestTrue(TEXT("rgba() fractional alpha parses"), ParseColor(TEXT("rgba(255,255,255,0.5)"), C, Error));
	TestEqual(TEXT("rgba() fractional alpha"), C.A, 128.0f / 255.0f, 1e-2f);

	TestTrue(TEXT("linear() parses"), ParseColor(TEXT("linear(0.25,0.5,0.75)"), C, Error));
	TestEqual(TEXT("linear() passes through"), C, FLinearColor(0.25f, 0.5f, 0.75f, 1.0f));

	TestTrue(TEXT("tuple parses"), ParseColor(TEXT("(R=0.1,G=0.2,B=0.3,A=0.4)"), C, Error));
	TestEqual(TEXT("tuple passes through"), C, FLinearColor(0.1f, 0.2f, 0.3f, 0.4f));

	TestTrue(TEXT("named colour parses"), ParseColor(TEXT("Orange"), C, Error));
	TestEqual(TEXT("named colour is sRGB"), C, FLinearColor::FromSRGBColor(FColor(255, 165, 0, 255)));

	TestFalse(TEXT("empty is rejected"), ParseColor(TEXT(""), C, Error));
	TestFalse(TEXT("bad hex is rejected"), ParseColor(TEXT("#12345G"), C, Error));
	TestFalse(TEXT("unknown name is rejected"), ParseColor(TEXT("blurple"), C, Error));
	TestFalse(TEXT("out-of-range sRGB is rejected"), ParseColor(TEXT("rgb(300,0,0)"), C, Error));
	TestFalse(TEXT("wrong channel count is rejected"), ParseColor(TEXT("rgb(1,2)"), C, Error));
	TestFalse(TEXT("unknown function is rejected"), ParseColor(TEXT("hsl(10,20,30)"), C, Error));
	TestFalse(TEXT("rejection explains itself"), Error.IsEmpty());
	return true;
}

// ---------------------------------------------------------------------------
// build_layout / set_property_verified / set_color / export_layout on a transient WBP
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeWidgetLayoutRoundTripTest,
	"VibeUE.Widget.LayoutRoundTrip", kWidgetTestFlags)
bool FVibeWidgetLayoutRoundTripTest::RunTest(const FString&)
{
	const FString PackageName = TEXT("/Game/__VibeUETest/WBP_LayoutRoundTrip");
	UPackage* Package = CreatePackage(*PackageName);
	if (!TestNotNull(TEXT("created a package for the transient test Widget Blueprint"), Package))
	{
		return false;
	}

	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		UUserWidget::StaticClass(), Package, FName(TEXT("WBP_LayoutRoundTrip")), BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	if (!TestNotNull(TEXT("CreateBlueprint returned a Widget Blueprint"), WidgetBP))
	{
		return false;
	}
	FAssetRegistryModule::AssetCreated(WidgetBP);
	ON_SCOPE_EXIT
	{
		if (WidgetBP)
		{
			FAssetRegistryModule::AssetDeleted(WidgetBP);
			WidgetBP->ClearFlags(RF_Standalone | RF_Public);
		}
	};
	const FString Path = PackageName;

	// --- Validation rejects bad layouts without writing anything --------------------------------
	{
		const FString Bad = TEXT(R"JSON({"root": {"type": "CanvasPanel", "name": "Root", "children": [
			{"type": "TextBlock", "name": "Dup", "children": [{"type": "TextBlock", "name": "Inner"}]},
			{"type": "NoSuchWidgetType", "name": "Dup"},
			{"type": "Button", "name": "TwoKids", "children": [{"type": "Spacer", "name": "A"}, {"type": "Spacer", "name": "B"}]},
			{"type": "TextBlock", "name": "BadProp", "properties": {"NotAProperty": "1"}}]}})JSON");
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Bad, TEXT("append"), false);
		TestFalse(TEXT("invalid layout fails"), R.bSuccess);
		TestEqual(TEXT("invalid layout creates nothing"), R.WidgetsCreated, 0);
		// child under non-panel, unknown type, duplicate name, two children under a Button, unknown property
		TestTrue(TEXT("every problem is reported in one pass"), R.Errors.Num() >= 5);
		TestTrue(TEXT("tree is still empty"), UWidgetService::GetRootWidget(Path).IsEmpty());
	}

	// --- Dry run validates and counts, writes nothing ---------------------------------------------
	const FString Layout = TEXT(R"JSON({"root": {"type": "CanvasPanel", "name": "Root", "is_variable": false, "children": [
		{"type": "VerticalBox", "name": "Menu", "slot": {"Position X": 100, "Position Y": 200}, "children": [
			{"type": "Button", "name": "PlayButton", "slot": {"Padding": 8}, "children": [
				{"type": "TextBlock", "name": "PlayLabel", "properties": {"Text": "Play", "Font.Size": 28}}]},
			{"type": "TextBlock", "name": "Footer", "properties": {"Text": "v1.0"}}]}]}})JSON");
	{
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Layout, TEXT("append"), true);
		TestTrue(TEXT("dry run succeeds"), R.bSuccess);
		TestTrue(TEXT("dry run is flagged"), R.bDryRun);
		TestEqual(TEXT("dry run counts widgets"), R.WidgetsCreated, 5);
		TestTrue(TEXT("dry run writes nothing"), UWidgetService::GetRootWidget(Path).IsEmpty());
	}

	// --- Real build -----------------------------------------------------------------------------
	{
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Layout, TEXT("append"), false);
		for (const FString& E : R.Errors)
		{
			AddError(FString::Printf(TEXT("build_layout: %s"), *E));
		}
		TestTrue(TEXT("build succeeds"), R.bSuccess);
		TestEqual(TEXT("build creates every widget"), R.WidgetsCreated, 5);
		TestEqual(TEXT("no property failed"), R.PropertiesFailed, 0);
		TestEqual(TEXT("root is the CanvasPanel"), UWidgetService::GetRootWidget(Path), FString(TEXT("Root")));
		TestTrue(TEXT("nested widget exists"), UWidgetService::WidgetExists(Path, TEXT("PlayLabel")));
		TestEqual(TEXT("font size applied"), FCString::Atof(*UWidgetService::GetProperty(Path, TEXT("PlayLabel"), TEXT("Font.Size"))), 28.0f, 1e-3f);
		TestEqual(TEXT("canvas slot position applied"), FCString::Atof(*UWidgetService::GetProperty(Path, TEXT("Menu"), TEXT("Position X"))), 100.0f, 1e-3f);
	}
	{
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Layout, TEXT("append"), false);
		TestFalse(TEXT("append onto an existing root is refused"), R.bSuccess);
	}
	{
		const FString Append = TEXT(R"JSON({"parent": "Menu", "children": [{"type": "TextBlock", "name": "Extra"}]})JSON");
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Append, TEXT("append"), false);
		TestTrue(TEXT("append under a panel succeeds"), R.bSuccess);
		TestTrue(TEXT("appended widget exists"), UWidgetService::WidgetExists(Path, TEXT("Extra")));
	}

	// --- set_property_verified ------------------------------------------------------------------
	{
		const FWidgetPropertyWriteResult Good = UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Font.Size"), TEXT("14"));
		TestTrue(TEXT("verified write succeeds"), Good.bSuccess);
		TestTrue(TEXT("verified write reports a change"), Good.bChanged);
		TestEqual(TEXT("verified write reads back"), FCString::Atof(*Good.NewValue), 14.0f, 1e-3f);

		const FWidgetPropertyWriteResult Same = UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Font.Size"), TEXT("14"));
		TestTrue(TEXT("re-writing the same value is a success"), Same.bSuccess);
		TestFalse(TEXT("re-writing the same value is not a change"), Same.bChanged);

		// A struct string with no matching member names is the silent no-op set_property can't see.
		TestTrue(TEXT("padding set"), UWidgetService::SetPropertyVerified(Path, TEXT("PlayButton"), TEXT("Slot.Padding"), TEXT("(Left=4,Top=4,Right=4,Bottom=4)")).bSuccess);
		const FWidgetPropertyWriteResult NoOp = UWidgetService::SetPropertyVerified(Path, TEXT("PlayButton"), TEXT("Slot.Padding"), TEXT("(Lft=9)"));
		TestFalse(TEXT("no-op struct write is caught"), NoOp.bSuccess);
		TestFalse(TEXT("no-op struct write explains itself"), NoOp.Message.IsEmpty());

		TestFalse(TEXT("unknown property fails"), UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("NotAProperty"), TEXT("1")).bSuccess);
	}

	// --- set_color --------------------------------------------------------------------------------
	{
		const FWidgetPropertyWriteResult R = UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("ColorAndOpacity"), TEXT("#FF0000"));
		TestTrue(TEXT("set_color on FSlateColor succeeds"), R.bSuccess);
		TestTrue(TEXT("set_color stores a specified colour"), R.NewValue.Contains(TEXT("SpecifiedColor")));

		TestTrue(TEXT("set_color on FLinearColor succeeds"),
			UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("ShadowColorAndOpacity"), TEXT("black")).bSuccess);
		TestFalse(TEXT("set_color on a non-colour property fails"),
			UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("Font.Size"), TEXT("red")).bSuccess);
		TestFalse(TEXT("set_color with an unparseable colour fails"),
			UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("ColorAndOpacity"), TEXT("not-a-colour")).bSuccess);
	}

	// --- export_layout -> build_layout(replace) round trip ---------------------------------------
	{
		const FString Exported = UWidgetService::ExportLayout(Path, false);
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Exported);
		if (!TestTrue(TEXT("export_layout returns JSON"), FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid()))
		{
			return false;
		}
		TestTrue(TEXT("export_layout reports success"), Parsed->GetBoolField(TEXT("success")));
		TestEqual(TEXT("exported root name"), Parsed->GetObjectField(TEXT("root"))->GetStringField(TEXT("name")), FString(TEXT("Root")));

		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Exported, TEXT("replace"), false);
		for (const FString& E : R.Errors)
		{
			AddError(FString::Printf(TEXT("rebuild: %s"), *E));
		}
		TestTrue(TEXT("rebuild from export succeeds"), R.bSuccess);
		TestEqual(TEXT("rebuild removes the old tree"), R.WidgetsRemoved, 6);
		TestEqual(TEXT("rebuild recreates every widget"), R.WidgetsCreated, 6);

		TestEqual(TEXT("export -> rebuild -> export is stable"), UWidgetService::ExportLayout(Path, false), Exported);
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
