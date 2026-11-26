// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UWidgetService.h"
#include "PythonAPI/WidgetColorParsing.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintEditorSettings.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ExpandableArea.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Templates/UnrealTemplate.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"
#include "WidgetBlueprintExtension.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

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

static bool RunWidgetLayoutRoundTripCase(FAutomationTestBase& Test)
{
	const FString Leaf = TEXT("WBP_LayoutRoundTrip_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString PackageName = TEXT("/Game/__VibeUETest/") + Leaf;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Test.TestNotNull(TEXT("created a package for the transient test Widget Blueprint"), Package))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		Package->SetDirtyFlag(false);
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		Test.TestFalse(TEXT("scratch package is clean"), Package->IsDirty());
		Test.TestFalse(TEXT("scratch asset was never saved to Content"), IFileManager::Get().FileExists(*Filename));
	};

	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		UUserWidget::StaticClass(), Package, FName(*Leaf), BPTYPE_Normal,
		UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	if (!Test.TestNotNull(TEXT("CreateBlueprint returned a Widget Blueprint"), WidgetBP))
	{
		return false;
	}
	FAssetRegistryModule::AssetCreated(WidgetBP);
	ON_SCOPE_EXIT
	{
		FAssetRegistryModule::AssetDeleted(WidgetBP);
		WidgetBP->ClearFlags(RF_Standalone | RF_Public);
		WidgetBP->MarkAsGarbage();
	};
	const FString Path = PackageName;

	// --- Validation rejects bad layouts without writing anything --------------------------------
	{
		WidgetBP->Status = BS_Dirty;
		Package->SetDirtyFlag(false);
		const FString Self = FString::Printf(TEXT("{\"root\":{\"type\":\"%s\",\"name\":\"Root\"}}"), *Leaf);
		const auto Circular = UWidgetService::BuildLayout(Path, Self, TEXT("replace"), true);
		Test.TestTrue(TEXT("self-reference refused without compiling the target"), !Circular.bSuccess &&
			FString::Join(Circular.Errors, TEXT(" ")).Contains(TEXT("circular reference")) && WidgetBP->Status == BS_Dirty && !Package->IsDirty());
	}
	{
		const FString Bad = TEXT(R"JSON({"root": {"type": "CanvasPanel", "name": "Root", "children": [
			{"type": "TextBlock", "name": "Dup", "children": [{"type": "TextBlock", "name": "Inner"}]},
			{"type": "NoSuchWidgetType", "name": "Dup"},
			{"type": "Button", "name": "TwoKids", "children": [{"type": "Spacer", "name": "A"}, {"type": "Spacer", "name": "B"}]},
			{"type": "TextBlock", "name": "BadProp", "properties": {"NotAProperty": "1"}}]}})JSON");
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Bad, TEXT("append"), false);
		Test.TestFalse(TEXT("invalid layout fails"), R.bSuccess);
		Test.TestEqual(TEXT("invalid layout creates nothing"), R.WidgetsCreated, 0);
		// child under non-panel, unknown type, duplicate name, two children under a Button, unknown property
		Test.TestTrue(TEXT("every problem is reported in one pass"), R.Errors.Num() >= 5);
		Test.TestTrue(TEXT("tree is still empty"), UWidgetService::GetRootWidget(Path).IsEmpty());
	}

	// --- Dry run validates and counts, writes nothing ---------------------------------------------
	{
		FString Nested = TEXT(R"({"type":"Spacer","name":"Leaf"})");
		for (int32 Depth = 2; Depth <= 80; ++Depth)
		{
			Nested = FString::Printf(TEXT(R"({"type":"VerticalBox","name":"Level%d","children":[%s]})"), Depth, *Nested);
			if (Depth == 64 || Depth == 65 || Depth == 80)
			{
				const auto Check = UWidgetService::BuildLayout(Path, TEXT("{\"root\":") + Nested + TEXT("}"), TEXT("append"), Depth == 64);
				Test.TestEqual(TEXT("64 input widget levels are allowed, 65 are refused"), Check.bSuccess, Depth == 64);
				if (Depth > 64)
				{
					Test.TestTrue(TEXT("depth limit explained"), FString::Join(Check.Errors, TEXT(" ")).Contains(TEXT("64")));
					Test.TestEqual(TEXT("over-depth request wrote nothing"), Check.WidgetsCreated, 0);
				}
			}
		}
	}
	const FString Layout = TEXT(R"JSON({"root": {"type": "CanvasPanel", "name": "Root", "is_variable": false, "children": [
		{"type": "VerticalBox", "name": "Menu", "slot": {"Position X": 100, "Position Y": 200}, "children": [
			{"type": "Button", "name": "PlayButton", "slot": {"Padding": 8}, "children": [
				{"type": "TextBlock", "name": "PlayLabel", "properties": {"Text": "Play", "Font.Size": 28}}]},
			{"type": "TextBlock", "name": "Footer", "properties": {"Text": "v1.0"}}]}]}})JSON");
	{
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Layout, TEXT("append"), true);
		Test.TestTrue(TEXT("dry run succeeds"), R.bSuccess);
		Test.TestTrue(TEXT("dry run is flagged"), R.bDryRun);
		Test.TestEqual(TEXT("dry run counts widgets"), R.WidgetsCreated, 5);
		Test.TestTrue(TEXT("dry run writes nothing"), UWidgetService::GetRootWidget(Path).IsEmpty());
	}

	// --- Real build -----------------------------------------------------------------------------
	{
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Layout, TEXT("append"), false);
		for (const FString& E : R.Errors)
		{
			Test.AddError(FString::Printf(TEXT("build_layout: %s"), *E));
		}
		Test.TestTrue(TEXT("build succeeds"), R.bSuccess);
		Test.TestEqual(TEXT("build creates every widget"), R.WidgetsCreated, 5);
		Test.TestEqual(TEXT("no property failed"), R.PropertiesFailed, 0);
		Test.TestEqual(TEXT("root is the CanvasPanel"), UWidgetService::GetRootWidget(Path), FString(TEXT("Root")));
		Test.TestTrue(TEXT("nested widget exists"), UWidgetService::WidgetExists(Path, TEXT("PlayLabel")));
		Test.TestEqual(TEXT("font size applied"), FCString::Atof(*UWidgetService::GetProperty(Path, TEXT("PlayLabel"), TEXT("Font.Size"))), 28.0f, 1e-3f);
		Test.TestEqual(TEXT("canvas slot position applied"), FCString::Atof(*UWidgetService::GetProperty(Path, TEXT("Menu"), TEXT("Position X"))), 100.0f, 1e-3f);
	}
	{
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Layout, TEXT("append"), false);
		Test.TestFalse(TEXT("append onto an existing root is refused"), R.bSuccess);
	}
	{
		const FString Append = TEXT(R"JSON({"parent": "Menu", "children": [{"type": "TextBlock", "name": "Extra"}]})JSON");
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Append, TEXT("append"), false);
		Test.TestTrue(TEXT("append under a panel succeeds"), R.bSuccess);
		Test.TestTrue(TEXT("appended widget exists"), UWidgetService::WidgetExists(Path, TEXT("Extra")));
	}

	// --- set_property_verified ------------------------------------------------------------------
	{
		const FWidgetPropertyWriteResult Good = UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Font.Size"), TEXT("14"));
		Test.TestTrue(TEXT("verified write succeeds"), Good.bSuccess);
		Test.TestTrue(TEXT("verified write reports a change"), Good.bChanged);
		Test.TestEqual(TEXT("verified write reads back"), FCString::Atof(*Good.NewValue), 14.0f, 1e-3f);

		const FWidgetPropertyWriteResult Same = UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Font.Size"), TEXT("14"));
		Test.TestTrue(TEXT("re-writing the same value is a success"), Same.bSuccess);
		Test.TestFalse(TEXT("re-writing the same value is not a change"), Same.bChanged);

		// A struct string with no matching member names is the silent no-op set_property can't see.
		Test.TestTrue(TEXT("padding set"), UWidgetService::SetPropertyVerified(Path, TEXT("PlayButton"), TEXT("Slot.Padding"), TEXT("(Left=4,Top=4,Right=4,Bottom=4)")).bSuccess);
		const FWidgetPropertyWriteResult NoOp = UWidgetService::SetPropertyVerified(Path, TEXT("PlayButton"), TEXT("Slot.Padding"), TEXT("(Lft=9)"));
		Test.TestFalse(TEXT("no-op struct write is caught"), NoOp.bSuccess);
		Test.TestFalse(TEXT("no-op struct write explains itself"), NoOp.Message.IsEmpty());

		const auto DefaultTypo = UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Slot.Padding"), TEXT("(Lft=9)"));
		Test.TestFalse(TEXT("unknown member on a default-valued slot fails"), DefaultTypo.bSuccess);
		Test.TestFalse(TEXT("unknown member leaves the default untouched"), DefaultTypo.bChanged);
		Test.TestTrue(TEXT("reports unknown and valid members"), DefaultTypo.Message.Contains(TEXT("Lft")) && DefaultTypo.Message.Contains(TEXT("Left")));
		const auto Mixed = UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Slot.Padding"), TEXT("(Left=8,Botom=9,Rght=2)"));
		Test.TestFalse(TEXT("mixed valid and unknown members fail before importing Left"), Mixed.bSuccess || Mixed.bChanged);
		Test.TestTrue(TEXT("both unknown names are listed"), Mixed.Message.Contains(TEXT("Botom")) && Mixed.Message.Contains(TEXT("Rght")));
		Test.TestTrue(TEXT("member matching is case insensitive"),
			UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Slot.Padding"), TEXT("(lEfT=2)")).bSuccess);

		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
		const auto Partial = UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("Slot.Padding"), TEXT("(Left=17"));
		Test.TestFalse(TEXT("unterminated tuple fails"), Partial.bSuccess);
		Test.TestTrue(TEXT("failed import reports the partial write"), Partial.bChanged && Partial.NewValue != Partial.PreviousValue);
		Test.TestEqual(TEXT("failure returns actual slot contents"), Partial.NewValue, UWidgetService::GetProperty(Path, TEXT("Footer"), TEXT("Slot.Padding")));
		Test.TestTrue(TEXT("partial write marked the Blueprint modified"), WidgetBP->Status == BS_Dirty);

		Test.TestFalse(TEXT("unknown property fails"), UWidgetService::SetPropertyVerified(Path, TEXT("Footer"), TEXT("NotAProperty"), TEXT("1")).bSuccess);
	}

	// --- set_color --------------------------------------------------------------------------------
	{
		const auto BadNested = UWidgetService::SetPropertyVerified(Path, TEXT("PlayLabel"), TEXT("ColorAndOpacity"),
			TEXT("(SpecifiedColor=(R=0.5,Gren=1),ColorUseRule=UseColor_Specified)"));
		Test.TestFalse(TEXT("unknown nested member rejects the whole colour"), BadNested.bChanged || BadNested.bSuccess);
		Test.TestTrue(TEXT("a SlateColor rule by itself is a valid member assignment"),
			UWidgetService::SetPropertyVerified(Path, TEXT("PlayLabel"), TEXT("ColorAndOpacity"), TEXT("(ColorUseRule=UseColor_Foreground)")).bSuccess);
		const FWidgetPropertyWriteResult R = UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("ColorAndOpacity"), TEXT("#FF0000"));
		Test.TestTrue(TEXT("set_color on FSlateColor succeeds"), R.bSuccess);
		Test.TestTrue(TEXT("set_color stores a specified colour"), R.NewValue.Contains(TEXT("SpecifiedColor")));

		Test.TestTrue(TEXT("set_color on FLinearColor succeeds"),
			UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("ShadowColorAndOpacity"), TEXT("black")).bSuccess);
		Test.TestFalse(TEXT("set_color on a non-colour property fails"),
			UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("Font.Size"), TEXT("red")).bSuccess);
		Test.TestFalse(TEXT("set_color with an unparseable colour fails"),
			UWidgetService::SetColor(Path, TEXT("PlayLabel"), TEXT("ColorAndOpacity"), TEXT("not-a-colour")).bSuccess);
	}

	// --- export_layout -> build_layout(replace) round trip ---------------------------------------
	{
		const FString Exported = UWidgetService::ExportLayout(Path, false);
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Exported);
		if (!Test.TestTrue(TEXT("export_layout returns JSON"), FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid()))
		{
			return false;
		}
		Test.TestTrue(TEXT("export_layout reports success"), Parsed->GetBoolField(TEXT("success")));
		Test.TestEqual(TEXT("exported root name"), Parsed->GetObjectField(TEXT("root"))->GetStringField(TEXT("name")), FString(TEXT("Root")));

		const FGuid PlayGuid = WidgetBP->WidgetVariableNameToGuidMap.FindRef(TEXT("PlayButton"));
		const FWidgetBuildLayoutResult R = UWidgetService::BuildLayout(Path, Exported, TEXT("replace"), false);
		for (const FString& E : R.Errors)
		{
			Test.AddError(FString::Printf(TEXT("rebuild: %s"), *E));
		}
		Test.TestTrue(TEXT("rebuild from export succeeds"), R.bSuccess);
		Test.TestEqual(TEXT("rebuild removes the old tree"), R.WidgetsRemoved, 6);
		Test.TestEqual(TEXT("rebuild recreates every widget"), R.WidgetsCreated, 6);
		Test.TestTrue(TEXT("reused name and class keep the variable GUID"), PlayGuid.IsValid() && PlayGuid == WidgetBP->WidgetVariableNameToGuidMap.FindRef(TEXT("PlayButton")));

		Test.TestEqual(TEXT("export -> rebuild -> export is stable"), UWidgetService::ExportLayout(Path, false), Exported);
	}

	const FString AddArea = TEXT(R"json({"parent":"Menu","children":[{"type":"ExpandableArea","name":"Area","properties":{"AreaPadding":"(Lft=9)"}}]})json");
	const auto WithTypo = UWidgetService::BuildLayout(Path, AddArea, TEXT("append"), false);
	Test.TestEqual(TEXT("misspelt layout assignment is counted as failed"), WithTypo.PropertiesFailed, 1);
	Test.TestEqual(TEXT("misspelt layout assignment is never counted as set"), WithTypo.PropertiesSet, 0);
	UExpandableArea* Area = WidgetBP->WidgetTree->FindWidget<UExpandableArea>(TEXT("Area"));
	if (!Test.TestNotNull(TEXT("created expandable area"), Area)) { return false; }
	UTextBlock* Header = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HiddenHeader"));
	if (!Test.TestNotNull(TEXT("created named-slot content"), Header)) { return false; }
	WidgetBP->OnVariableAdded(Header->GetFName());
	Area->SetContentForSlot(TEXT("Header"), Header);

	const FString ExportedArea = UWidgetService::ExportLayout(Path, false);
	Test.TestFalse(TEXT("panel export omits the ExpandableArea header widget"), ExportedArea.Contains(TEXT("HiddenHeader")));
	const auto Hidden = UWidgetService::BuildLayout(Path, ExportedArea, TEXT("replace"), false);
	if (!Test.TestFalse(TEXT("named-slot loss requires explicit opt-in"), Hidden.bSuccess)) { return false; }
	Test.TestTrue(TEXT("refusal names the content"), FString::Join(Hidden.Errors, TEXT(" ")).Contains(TEXT("HiddenHeader")));
	Test.TestTrue(TEXT("named slot still owns its content"), Area->GetContentForSlot(TEXT("Header")) == Header);
	WidgetBP->Bindings.AddDefaulted_GetRef().ObjectName = TEXT("HiddenHeader");
	const auto BoundHeader = UWidgetService::BuildLayout(Path, ExportedArea, TEXT("replace"), false, true);
	if (!Test.TestFalse(TEXT("named-slot opt-in still protects bound content"), BoundHeader.bSuccess)) { return false; }
	Test.TestTrue(TEXT("retains the bound named-slot widget"), Area->GetContentForSlot(TEXT("Header")) == Header);
	WidgetBP->Bindings.Pop();
	{
		UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(WidgetBP);
		Animation->AnimationBindings.AddDefaulted_GetRef().WidgetName = Header->GetFName();
		WidgetBP->Animations.Add(Animation);
		const auto Animated = UWidgetService::BuildLayout(Path, ExportedArea, TEXT("replace"), true, true);
		Test.TestTrue(TEXT("animation binding protects its target"), !Animated.bSuccess && FString::Join(Animated.Errors, TEXT(" ")).Contains(TEXT("HiddenHeader")));
		WidgetBP->Animations.Pop();
	}
	{
		auto* Extension = UWidgetBlueprintExtension::RequestExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBP);
		Extension->CreateBlueprintViewInstance();
		Extension->GetBlueprintView()->AddDefaultBinding().SourcePath.SetWidgetName(Header->GetFName());
		const auto ViewBound = UWidgetService::BuildLayout(Path, ExportedArea, TEXT("replace"), true, true);
		Test.TestTrue(TEXT("whole-widget MVVM source protects its target"), !ViewBound.bSuccess && FString::Join(ViewBound.Errors, TEXT(" ")).Contains(TEXT("HiddenHeader")));
		WidgetBP->RemoveExtension(Extension);
	}
	const auto DropHidden = UWidgetService::BuildLayout(Path, ExportedArea, TEXT("replace"), false, true);
	Test.TestTrue(TEXT("explicit opt-in drops unbound named-slot content"), DropHidden.bSuccess && !WidgetBP->WidgetVariableNameToGuidMap.Contains(TEXT("HiddenHeader")));

	FObjectProperty* ButtonVariable = FindFProperty<FObjectProperty>(WidgetBP->SkeletonGeneratedClass, TEXT("PlayButton"));
	if (!Test.TestNotNull(TEXT("PlayButton is a graph variable"), ButtonVariable)) { return false; }
	FKismetEditorUtilities::CreateNewBoundEventForClass(UButton::StaticClass(), TEXT("OnClicked"), WidgetBP, ButtonVariable, false);
	const auto* Event = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, TEXT("OnClicked"), TEXT("PlayButton"));
	if (!Test.TestTrue(TEXT("created the OnClicked event node"), Event != nullptr)) { return false; }
	const auto* OriginalRoot = WidgetBP->WidgetTree->RootWidget.Get();
	const auto* OriginalButton = WidgetBP->WidgetTree->FindWidget(TEXT("PlayButton"));
	const FGuid OriginalGuid = WidgetBP->WidgetVariableNameToGuidMap.FindRef(TEXT("PlayButton"));
	const FString OriginalLayout = UWidgetService::ExportLayout(Path, false);
	FDelegateEditorBinding& FooterBinding = WidgetBP->Bindings.AddDefaulted_GetRef();
	FooterBinding.ObjectName = TEXT("Footer");
	FooterBinding.PropertyName = TEXT("Text");
	Package->SetDirtyFlag(false);
	// First keep the bound Footer: only the graph can block the dropped button. Then retype
	// the button and drop Footer, which should identify both independent reasons for refusal.
	const FString KeepFooter = TEXT(R"({"root":{"type":"TextBlock","name":"Footer"}})");
	for (const FString& Candidate : {KeepFooter,
		FString(TEXT(R"({"root":{"type":"VerticalBox","name":"PlayButton"}})"))})
	{
		for (bool bPreview : {true, false})
		{
			const auto Refused = UWidgetService::BuildLayout(Path, Candidate, TEXT("replace"), bPreview, true);
			const FString Errors = FString::Join(Refused.Errors, TEXT(" "));
			Test.TestFalse(TEXT("dropping or retyping bound widgets is refused"), Refused.bSuccess);
			Test.TestTrue(TEXT("graph error identifies the bound button"), Errors.Contains(TEXT("PlayButton")));
			Test.TestEqual(TEXT("binding error identifies Footer only if dropped"), Errors.Contains(TEXT("Footer")), Candidate != KeepFooter);
			Test.TestEqual(TEXT("refusal leaves widget data unchanged"), UWidgetService::ExportLayout(Path, false), OriginalLayout);
			Test.TestTrue(TEXT("original templates and GUID survive"), WidgetBP->WidgetTree->RootWidget.Get() == OriginalRoot &&
				WidgetBP->WidgetTree->FindWidget(TEXT("PlayButton")) == OriginalButton && WidgetBP->WidgetVariableNameToGuidMap.FindRef(TEXT("PlayButton")) == OriginalGuid);
			Test.TestTrue(TEXT("graph node and binding survive"), Event && FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, TEXT("OnClicked"), TEXT("PlayButton")) == Event &&
				WidgetBP->Bindings.Num() == 1 && WidgetBP->Bindings[0].ObjectName == TEXT("Footer"));
			Test.TestFalse(TEXT("refusal leaves package clean"), Package->IsDirty());
		}
	}
	WidgetBP->Bindings.Reset();
	FWidgetBlueprintEditorUtils::SetDesiredFocus(WidgetBP, TEXT("Footer"));
	Test.TestTrue(TEXT("desired focus starts on the widget to drop"), WidgetBP->GeneratedClass->GetDefaultObject<UUserWidget>()->GetDesiredFocusWidgetName() == FName(TEXT("Footer")));
	const FGuid OldExtraGuid = WidgetBP->WidgetVariableNameToGuidMap.FindRef(TEXT("Extra"));
	const auto Moved = UWidgetService::BuildLayout(Path,
		TEXT(R"({"root":{"type":"Button","name":"PlayButton","children":[{"type":"Spacer","name":"Extra"}]}})"), TEXT("replace"), false);
	Test.TestTrue(TEXT("can drop a parent of a retained graph-bound child"), Moved.bSuccess);
	Test.TestTrue(TEXT("retained child keeps its GUID and event"), OriginalGuid == WidgetBP->WidgetVariableNameToGuidMap.FindRef(TEXT("PlayButton")) &&
		FKismetEditorUtilities::FindBoundEventForComponent(WidgetBP, TEXT("OnClicked"), TEXT("PlayButton")) != nullptr);
	Test.TestFalse(TEXT("dropped widget lost its GUID"), WidgetBP->WidgetVariableNameToGuidMap.Contains(TEXT("Footer")));
	const FGuid NewExtraGuid = WidgetBP->WidgetVariableNameToGuidMap.FindRef(TEXT("Extra"));
	Test.TestTrue(TEXT("free retyped widget received a fresh GUID"), OldExtraGuid.IsValid() && NewExtraGuid.IsValid() && OldExtraGuid != NewExtraGuid);
	Test.TestTrue(TEXT("dropped desired focus was cleared"), WidgetBP->GeneratedClass->GetDefaultObject<UUserWidget>()->GetDesiredFocusWidgetName().IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeWidgetLayoutRoundTripTest,
	"VibeUE.Widget.LayoutRoundTrip", kWidgetTestFlags)
bool FVibeWidgetLayoutRoundTripTest::RunTest(const FString&)
{
	TGuardValue<TEnumAsByte<ESaveOnCompile>> SaveSetting(GetMutableDefault<UBlueprintEditorSettings>()->SaveOnCompile, SoC_Never);
	AddExpectedMessage(TEXT("Unknown member 'Lft'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 4, false);
	AddExpectedMessage(TEXT("Unknown member 'Botom'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2, false);
	AddExpectedMessage(TEXT("Unknown member 'Gren'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2, false);
	AddExpectedMessage(TEXT("Failed to parse value '(Left=17'"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2, false);
	AddExpectedMessage(TEXT("Property 'NotAProperty' not found"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2, false);
	// Deliberately repeat in one editor session with the first fixture still in the undo buffer.
	return RunWidgetLayoutRoundTripCase(*this) && RunWidgetLayoutRoundTripCase(*this);
}

#endif // WITH_AUTOMATION_TESTS
