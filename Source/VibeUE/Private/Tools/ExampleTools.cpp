// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/ExampleTools.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"

FString UExampleTools::Echo(const FString& Message)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("message"), Message);
	Result->SetStringField(TEXT("echoed"), Message);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return OutputString;
}

FString UExampleTools::AddNumbers(int32 A, int32 B)
{
	int32 Sum = A + B;

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("a"), A);
	Result->SetNumberField(TEXT("b"), B);
	Result->SetNumberField(TEXT("sum"), Sum);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return OutputString;
}

FString UExampleTools::GetSystemInfo()
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	
	// Platform info
	Result->SetStringField(TEXT("platform"), FPlatformProperties::PlatformName());
	Result->SetStringField(TEXT("engine_version"), ENGINE_VERSION_STRING);
	
	// Current time
	FDateTime Now = FDateTime::Now();
	Result->SetStringField(TEXT("current_time"), Now.ToString());

	// Memory info
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	Result->SetNumberField(TEXT("used_physical_mb"), MemStats.UsedPhysical / (1024 * 1024));
	Result->SetNumberField(TEXT("available_physical_mb"), MemStats.AvailablePhysical / (1024 * 1024));

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return OutputString;
}

FString UExampleTools::CheckUnrealConnection()
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("connection_status"), TEXT("Connected successfully"));
	Result->SetStringField(TEXT("plugin_status"), TEXT("VibeUE plugin is responding"));
	Result->SetStringField(TEXT("server"), TEXT("Native C++ MCP Server"));
	Result->SetStringField(TEXT("port"), TEXT("8088"));
	Result->SetStringField(TEXT("host"), TEXT("127.0.0.1"));
	Result->SetStringField(TEXT("help_info"), TEXT("Use action='help' on multi-action tools (e.g., manage_level_actors with action='help')"));

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return OutputString;
}

