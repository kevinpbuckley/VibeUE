// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWidgetService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintEditorSettings.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
	struct FWidgetServiceFixture
	{
		TGuardValue<TEnumAsByte<ESaveOnCompile>> NoSave{
			GetMutableDefault<UBlueprintEditorSettings>()->SaveOnCompile, SoC_Never};
		TStrongObjectPtr<UWidgetBlueprint> Blueprint;

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Editor exists"), GEditor)
				|| !Test.TestFalse(TEXT("Fixture compilation has no outer transaction"), GEditor->IsTransactionActive())) { return false; }
			const FString Name = TEXT("WBP_VibeDelegate_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			UPackage* Package = CreatePackage(*(TEXT("/Temp/") + Name));
			// LoadAsset deliberately rejects RF_Transient packages. This remains an
			// unsaved /Temp fixture, with SaveOnCompile disabled for its whole lifetime.
			Blueprint.Reset(Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
				UUserWidget::StaticClass(), Package, FName(*Name), BPTYPE_Normal,
				UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(), NAME_None)));
			if (!Test.TestNotNull(TEXT("Transient widget Blueprint created"), Blueprint.Get())) { return false; }
			FAssetRegistryModule::AssetCreated(Blueprint.Get());
			return Test.TestNotNull(TEXT("Widget tree created"), Blueprint->WidgetTree.Get());
		}

		~FWidgetServiceFixture()
		{
			if (Blueprint.IsValid())
			{
				FAssetRegistryModule::AssetDeleted(Blueprint.Get());
				Blueprint->GetOutermost()->SetDirtyFlag(false);
				Blueprint->ClearFlags(RF_Standalone);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetServiceEmptyDelegateTest,
	"VibeUE.Widget.EmptyDelegates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWidgetServiceEmptyDelegateTest::RunTest(const FString&)
{
	FWidgetServiceFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	UWidgetTree* Tree = Fixture.Blueprint->WidgetTree;
	UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border"));
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button"));
	UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image"));
	Tree->RootWidget = Border;
	Border->SetContent(Button);
	Button->SetContent(Image);
	for (UWidget* Widget : {static_cast<UWidget*>(Border), static_cast<UWidget*>(Button), static_cast<UWidget*>(Image)})
	{
		Fixture.Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
	}
	const FString Path = Fixture.Blueprint->GetPathName();
	for (UWidget* Widget : {static_cast<UWidget*>(Border), static_cast<UWidget*>(Image)})
	{
		FDelegateProperty* Property = FindFProperty<FDelegateProperty>(Widget->GetClass(), TEXT("OnMouseButtonDownEvent"));
		if (!TestNotNull(TEXT("FOnPointerEvent property exists"), Property)) { return false; }
		FScriptDelegate* Value = Property->ContainerPtrToValuePtr<FScriptDelegate>(Widget);
		const FString EmptyReadback = UWidgetService::GetProperty(Path, Widget->GetName(), Property->GetName());
		for (const FString& Empty : {FString(), FString(TEXT("(null).None"))})
		{
			Value->BindUFunction(GetMutableDefault<UUserWidget>(), TEXT("OnMouseButtonDown"));
			TestTrue(TEXT("Pointer delegate seeded bound"), Value->IsBound());
			const FString BoundReadback = UWidgetService::GetProperty(Path, Widget->GetName(), Property->GetName());
			TestTrue(TEXT("Empty pointer delegate assignment succeeds"), UWidgetService::SetProperty(Path, Widget->GetName(), Property->GetName(), Empty));
			TestFalse(TEXT("Pointer delegate is unbound after assignment"), Value->IsBound());
			TestEqual(TEXT("Service reads the unbound delegate back"), UWidgetService::GetProperty(Path, Widget->GetName(), Property->GetName()), EmptyReadback);
			TestTrue(TEXT("Bound delegate still uses the normal import path"), UWidgetService::SetProperty(Path, Widget->GetName(), Property->GetName(), BoundReadback));
			TestTrue(TEXT("Bound delegate roundtrip preserves binding"), Value->IsBound());
		}
	}
	FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(Button->GetClass(), TEXT("OnClicked"));
	if (!TestNotNull(TEXT("Multicast property exists"), Property)) { return false; }
	void* Value = Property->ContainerPtrToValuePtr<void>(Button);
	const FString EmptyReadback = UWidgetService::GetProperty(Path, Button->GetName(), Property->GetName());
	for (const FString& Empty : {FString(), FString(TEXT("(null).None"))})
	{
		FScriptDelegate Bound;
		Bound.BindUFunction(Button, TEXT("InvalidateLayoutAndVolatility"));
		Property->AddDelegate(Bound, Button, Value);
		TestTrue(TEXT("Multicast delegate seeded bound"), Property->GetMulticastDelegate(Value)->IsBound());
		TestTrue(TEXT("Empty multicast assignment succeeds"), UWidgetService::SetProperty(Path, Button->GetName(), Property->GetName(), Empty));
		const FMulticastScriptDelegate* Cleared = Property->GetMulticastDelegate(Value);
		TestTrue(TEXT("Multicast delegate is unbound"), !Cleared || !Cleared->IsBound());
		TestEqual(TEXT("Service reads the unbound multicast back"), UWidgetService::GetProperty(Path, Button->GetName(), Property->GetName()), EmptyReadback);
	}
	return true;
}

#endif
