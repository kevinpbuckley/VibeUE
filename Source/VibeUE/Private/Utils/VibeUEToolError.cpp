// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEToolError.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/Stack.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEToolError, Log, All);

namespace VibeUE
{
	namespace ToolErrorPrivate
	{
		// Enough to name the cause; a failing loop must not turn the error text into a log dump.
		constexpr int32 MaxLines = 4;
		constexpr int32 MaxLineLength = 300;

		void Raise(const FString& Message)
		{
			if (FFrame::GetThreadLocalTopStackFrame())
			{
				UKismetSystemLibrary::RaiseScriptError(Message);
			}
			else
			{
				// Plain C++ caller: no script frame, so RaiseScriptError would do nothing at all.
				UE_LOG(LogVibeUEToolError, Error, TEXT("%s"), *Message);
			}
		}
	}

	FToolFailureLog::FToolFailureLog()
	{
		if (GLog)
		{
			GLog->AddOutputDevice(this);
		}
	}

	FToolFailureLog::~FToolFailureLog()
	{
		if (GLog)
		{
			GLog->RemoveOutputDevice(this);
		}
	}

	void FToolFailureLog::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
	{
		const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
		if (Level == ELogVerbosity::NoLogging || Level > ELogVerbosity::Warning || !V || !*V)
		{
			return;
		}
		if (Lines.Num() >= ToolErrorPrivate::MaxLines)
		{
			++Dropped;
			return;
		}
		FString Line = FString(V).TrimStartAndEnd();
		if (Line.Len() > ToolErrorPrivate::MaxLineLength)
		{
			Line = Line.Left(ToolErrorPrivate::MaxLineLength) + TEXT("...");
		}
		Lines.Add(FString::Printf(TEXT("%s: %s"), *Category.ToString(), *Line));
	}

	FString FToolFailureLog::Describe() const
	{
		FString Text = FString::Join(Lines, TEXT("; "));
		if (Dropped > 0)
		{
			Text += FString::Printf(TEXT("; (+%d more)"), Dropped);
		}
		return Text;
	}

	FString DescribeToolFailure(const TCHAR* Tool, const FString& Reason, const FToolFailureLog& Log)
	{
		FString Message = FString::Printf(TEXT("%s: %s"), Tool, *Reason);
		const FString Logged = Log.Describe();
		if (!Logged.IsEmpty())
		{
			Message += FString::Printf(TEXT(". Log: %s"), *Logged);
		}
		return Message;
	}

	void RaiseToolError(const TCHAR* Tool, const FString& Reason, const FToolFailureLog& Log)
	{
		// Build the text first: raising logs an error of its own, which the collector would otherwise pick up.
		const FString Message = DescribeToolFailure(Tool, Reason, Log);
		ToolErrorPrivate::Raise(Message);
	}

	void RaiseToolError(const TCHAR* Tool, const FString& Reason)
	{
		ToolErrorPrivate::Raise(FString::Printf(TEXT("%s: %s"), Tool, *Reason));
	}
}
