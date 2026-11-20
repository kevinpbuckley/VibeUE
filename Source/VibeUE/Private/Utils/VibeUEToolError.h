// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// How a VibeUE tool reports that it failed.
//
// A reflected (AICallable) function must hand its result back as its RETURN VALUE. Epic's ToolsetRegistry publishes
// an output reference parameter (FString& OutX, FInfo& OutInfo) as a required INPUT of the MCP tool and returns only
// the return value, so a tool written as `bool Fn(..., FInfo& OutInfo)` cannot be called the natural way over MCP and
// loses its data when it is called (issue #687). So a tool returns its value, and when it has none to return it fails
// the way Epic's own toolsets do: UKismetSystemLibrary::RaiseScriptError. A Python caller gets an exception and an
// MCP caller gets an error result, both carrying the text.
//
// Module-private (Private/, no VIBEUE_API).

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

namespace VibeUE
{
	/**
	 * Collects the Warning and Error lines logged while it is alive, so that a tool which fails can say why: most
	 * service code reports the reason in the log and returns false. Game thread only; create it on the stack around
	 * the call. Lines other threads logged meanwhile can be collected too, which is why they are quoted as log lines.
	 */
	class FToolFailureLog final : public FOutputDevice
	{
	public:
		FToolFailureLog();
		virtual ~FToolFailureLog() override;

		FToolFailureLog(const FToolFailureLog&) = delete;
		FToolFailureLog& operator=(const FToolFailureLog&) = delete;

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;

		/** The collected lines joined with "; " (the first few only); empty when nothing was logged. */
		FString Describe() const;

	private:
		TArray<FString> Lines;
		int32 Dropped = 0;
	};

	/** "<Tool>: <Reason>", followed by what the log said while the tool ran, when it said anything. */
	FString DescribeToolFailure(const TCHAR* Tool, const FString& Reason, const FToolFailureLog& Log);

	/**
	 * Fail the tool call that is running: a script error with "<Tool>: <Reason>" (plus the collected log lines).
	 * It does not unwind: return a default value right after it. Called from plain C++ with no script frame (not
	 * through Python, MCP or Blueprint) the engine has nowhere to deliver a script error, so it is logged instead.
	 */
	void RaiseToolError(const TCHAR* Tool, const FString& Reason, const FToolFailureLog& Log);
	void RaiseToolError(const TCHAR* Tool, const FString& Reason);
}
