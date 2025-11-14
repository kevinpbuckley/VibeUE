// Copyright Kevin Buckley 2025 All Rights Reserved.

/**
 * @file WidgetBlueprintInfoService.cpp
 * @brief Implementation for FWidgetBlueprintInfoService
 */

#include "Services/UMG/WidgetBlueprintInfoService.h"

#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_Event.h"
#include "K2Node_InputAction.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"

FWidgetBlueprintInfoService::FWidgetBlueprintInfoService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

TResult<FWidgetBlueprintInfo> FWidgetBlueprintInfoService::GetWidgetBlueprintInfo(UWidgetBlueprint* WidgetBlueprint)
{
    if (!WidgetBlueprint)
    {
        return TResult<FWidgetBlueprintInfo>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("WidgetBlueprint is null"));
    }

    FWidgetBlueprintInfo Info;
    Info.Name = WidgetBlueprint->GetName();
    Info.Path = WidgetBlueprint->GetPathName();
    Info.PackagePath = WidgetBlueprint->GetPackage() ? WidgetBlueprint->GetPackage()->GetPathName() : FString();
    Info.ParentClass = WidgetBlueprint->ParentClass ? WidgetBlueprint->ParentClass->GetName() : TEXT("UserWidget");

    if (WidgetBlueprint->WidgetTree && WidgetBlueprint->WidgetTree->RootWidget)
    {
        Info.RootWidget = WidgetBlueprint->WidgetTree->RootWidget->GetName();

        TArray<UWidget*> AllWidgets;
        WidgetBlueprint->WidgetTree->GetAllWidgets(AllWidgets);
        Info.WidgetCount = AllWidgets.Num();

        for (UWidget* Widget : AllWidgets)
        {
            if (!Widget) continue;
            FWidgetInfo W;
            W.Name = Widget->GetName();
            W.Type = Widget->GetClass() ? Widget->GetClass()->GetName() : FString();
            W.bIsVariable = Widget->bIsVariable;
            if (UPanelWidget* Parent = Cast<UPanelWidget>(Widget->GetParent()))
            {
                W.ParentName = Parent->GetName();
            }
            if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
            {
                int32 Count = Panel->GetChildrenCount();
                for (int32 i = 0; i < Count; ++i)
                {
                    if (UWidget* Child = Panel->GetChildAt(i))
                    {
                        W.Children.Add(Child->GetName());
                    }
                }
            }
            Info.Components.Add(MoveTemp(W));
        }
    }
    else
    {
        Info.WidgetCount = 0;
    }

    // Variables
    if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass))
    {
        for (TFieldIterator<FProperty> It(BlueprintClass); It; ++It)
        {
            FProperty* Prop = *It;
            if (Prop && Prop->HasAllPropertyFlags(CPF_BlueprintVisible))
            {
                Info.VariableNames.Add(Prop->GetName());
            }
        }
    }

    // Events
    if (WidgetBlueprint->UbergraphPages.Num() > 0)
    {
        for (UEdGraph* Graph : WidgetBlueprint->UbergraphPages)
        {
            if (!Graph) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
                {
                    Info.EventNames.Add(EventNode->EventReference.GetMemberName().ToString());
                }
                else if (UK2Node_InputAction* InputNode = Cast<UK2Node_InputAction>(Node))
                {
                    Info.EventNames.Add(InputNode->InputActionName.ToString());
                }
            }
        }
    }

    // Animations
    for (UWidgetAnimation* Animation : WidgetBlueprint->Animations)
    {
        if (Animation)
        {
            Info.AnimationNames.Add(Animation->GetName());
        }
    }

    return TResult<FWidgetBlueprintInfo>::Success(Info);
}
