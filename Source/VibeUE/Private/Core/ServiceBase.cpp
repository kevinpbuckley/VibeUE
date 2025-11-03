#include "Core/ServiceBase.h"
#include "Core/ServiceContext.h"

void FServiceBase::LogInfo(const FString& Message) const
{
	if (Context.IsValid())
	{
		Context->LogInfo(Message, GetServiceName());
	}
}

void FServiceBase::LogWarning(const FString& Message) const
{
	if (Context.IsValid())
	{
		Context->LogWarning(Message, GetServiceName());
	}
}

void FServiceBase::LogError(const FString& Message) const
{
	if (Context.IsValid())
	{
		Context->LogError(Message, GetServiceName());
	}
}
