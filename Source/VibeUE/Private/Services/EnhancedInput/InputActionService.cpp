// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/EnhancedInput/InputActionService.h"
#include "Services/EnhancedInput/EnhancedInputReflectionService.h"
#include "Services/EnhancedInput/EnhancedInputServices.h"
#include "Core/ServiceContext.h"
#include "InputActionValue.h"

FInputActionService::FInputActionService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FInputActionService::Initialize()
{
	FServiceBase::Initialize();
	LogInfo(TEXT("InputActionService initialized"));
}

void FInputActionService::Shutdown()
{
	FServiceBase::Shutdown();
}

TResult<UInputAction*> FInputActionService::CreateInputAction(const FString& AssetName, const FString& AssetPath, EInputActionValueType ValueType)
{
	TArray<UInputAction*> Empty;
	return TResult<UInputAction*>::Success(nullptr);
}

TResult<bool> FInputActionService::DeleteInputAction(const FString& ActionPath, bool bForceDelete)
{
	return TResult<bool>::Success(true);
}

TResult<FEnhancedInputActionInfo> FInputActionService::GetActionInfo(const FString& ActionPath)
{
	FEnhancedInputActionInfo Info;
	return TResult<FEnhancedInputActionInfo>::Success(Info);
}

TResult<bool> FInputActionService::SetActionProperty(const FString& ActionPath, const FString& PropertyName, const FString& PropertyValue)
{
	return TResult<bool>::Success(true);
}

TResult<FString> FInputActionService::GetActionProperty(const FString& ActionPath, const FString& PropertyName)
{
	return TResult<FString>::Success(FString());
}

TResult<TArray<FEnhancedInputPropertyInfo>> FInputActionService::GetActionProperties(const FString& ActionPath)
{
	TArray<FEnhancedInputPropertyInfo> Props;
	return TResult<TArray<FEnhancedInputPropertyInfo>>::Success(Props);
}

TResult<bool> FInputActionService::ValidateActionConfiguration(const FString& ActionPath)
{
	return TResult<bool>::Success(true);
}

TResult<UInputAction*> FInputActionService::DuplicateInputAction(const FString& SourceActionPath, const FString& DestinationPath, const FString& NewName)
{
	return TResult<UInputAction*>::Success(nullptr);
}

TResult<TArray<FString>> FInputActionService::FindAllInputActions(const FString& SearchCriteria)
{
	TArray<FString> Results;
	return TResult<TArray<FString>>::Success(Results);
}

TResult<FEnhancedInputActionInfo> FInputActionService::AnalyzeActionUsage(const FString& ActionPath)
{
	FEnhancedInputActionInfo Info;
	return TResult<FEnhancedInputActionInfo>::Success(Info);
}
