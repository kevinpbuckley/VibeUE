// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Core/ServiceContext.h"
#include "Editor.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeUE, Log, All);

FServiceContext::FServiceContext()
	: CachedAssetRegistry(nullptr)
{
}

FServiceContext::~FServiceContext()
{
	FScopeLock ScopeLock(&Lock);
	Services.Empty();
	ConfigValues.Empty();
	CachedAssetRegistry = nullptr;
}

void FServiceContext::LogInfo(const FString& Message, const FString& ServiceName) const
{
	UE_LOG(LogVibeUE, Log, TEXT("[%s] %s"), *ServiceName, *Message);
}

void FServiceContext::LogWarning(const FString& Message, const FString& ServiceName) const
{
	UE_LOG(LogVibeUE, Warning, TEXT("[%s] %s"), *ServiceName, *Message);
}

void FServiceContext::LogError(const FString& Message, const FString& ServiceName) const
{
	UE_LOG(LogVibeUE, Error, TEXT("[%s] %s"), *ServiceName, *Message);
}

UWorld* FServiceContext::GetWorld() const
{
	// Try to get the editor world if available
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	
	// Fallback to game world context
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() != nullptr)
			{
				return Context.World();
			}
		}
	}
	
	return nullptr;
}

UEditorEngine* FServiceContext::GetEditorEngine() const
{
	return GEditor;
}

IAssetRegistry* FServiceContext::GetAssetRegistry() const
{
	FScopeLock ScopeLock(&Lock);
	
	if (!CachedAssetRegistry)
	{
		FAssetRegistryModule& AssetRegistryModule = 
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		CachedAssetRegistry = &AssetRegistryModule.Get();
	}
	
	return CachedAssetRegistry;
}

void FServiceContext::RegisterService(const FString& ServiceName, TSharedPtr<FServiceBase> Service)
{
	FScopeLock ScopeLock(&Lock);
	Services.Emplace(ServiceName, Service);
}

TSharedPtr<FServiceBase> FServiceContext::GetService(const FString& ServiceName) const
{
	FScopeLock ScopeLock(&Lock);
	const TSharedPtr<FServiceBase>* FoundService = Services.Find(ServiceName);
	return FoundService ? *FoundService : nullptr;
}

FString FServiceContext::GetConfigValue(const FString& Key, const FString& DefaultValue) const
{
	FScopeLock ScopeLock(&Lock);
	const FString* FoundValue = ConfigValues.Find(Key);
	return FoundValue ? *FoundValue : DefaultValue;
}

void FServiceContext::SetConfigValue(const FString& Key, const FString& Value)
{
	FScopeLock ScopeLock(&Lock);
	ConfigValues.Emplace(Key, Value);
}
