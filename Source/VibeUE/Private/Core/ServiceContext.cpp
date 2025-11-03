// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/ServiceContext.h"
#include "AssetRegistry/AssetRegistryModule.h"

DEFINE_LOG_CATEGORY(LogVibeUEServices);

FServiceContext::FServiceContext()
	: bInitialized(false)
	, AssetRegistry(nullptr)
{
}

FServiceContext::~FServiceContext()
{
	Shutdown();
}

void FServiceContext::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	// Get asset registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistry = &AssetRegistryModule.Get();

	bInitialized = true;
	UE_LOG(LogVibeUEServices, Log, TEXT("ServiceContext initialized"));
}

void FServiceContext::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	AssetRegistry = nullptr;
	bInitialized = false;
	UE_LOG(LogVibeUEServices, Log, TEXT("ServiceContext shutdown"));
}
