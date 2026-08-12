// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "IPythonScriptPlugin.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UObject/UObjectHash.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeServiceDocstringConsistencyTest,
	"VibeUE.PythonAPI.ServiceDocstringConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeServiceDocstringConsistencyTest::RunTest(const FString&)
{
	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(UToolsetDefinition::StaticClass(), DerivedClasses, /*bRecursive*/ true);

	const UPackage* VibeUEPackage = FindPackage(nullptr, TEXT("/Script/VibeUE"));
	TArray<FString> ServiceNames;
	for (const UClass* Class : DerivedClasses)
	{
		if (Class && !Class->HasAnyClassFlags(CLASS_Abstract) &&
			Class->GetOutermost() == VibeUEPackage && Class->GetName().EndsWith(TEXT("Service")))
		{
			ServiceNames.Add(Class->GetName());
		}
	}
	ServiceNames.Sort();

	if (!TestTrue(TEXT("VibeUE service classes were discovered"), !ServiceNames.IsEmpty()))
	{
		return false;
	}

	TArray<FString> QuotedServiceNames;
	QuotedServiceNames.Reserve(ServiceNames.Num());
	for (const FString& ServiceName : ServiceNames)
	{
		QuotedServiceNames.Add(FString::Printf(TEXT("\"%s\""), *ServiceName));
	}

	// Validate against the generated Python wrappers themselves. This deliberately avoids
	// duplicating Unreal's Python name conversion (including ScriptName metadata and acronym
	// handling such as UVs -> u_vs). It checks method-list bullets on each service and every
	// fully qualified unreal.<Service>.<method>(...) example, including cross-service examples.
	FString PythonCode = TEXT(R"PY(
import re
import unreal

services = [__VIBEUE_SERVICES__]
errors = []

for owner_name in services:
    owner = getattr(unreal, owner_name, None)
    if owner is None:
        errors.append("%s is not bound in Python" % owner_name)
        continue

    doc = owner.__doc__ or ""
    claimed = set()
    for line in doc.splitlines():
        bullet = re.match(r"^\s*-\s+(.+)$", line)
        if not bullet:
            continue
        leader = bullet.group(1).split(":", 1)[0]
        for part in leader.split("/"):
            method = re.match(r"^\s*([a-z_][a-z0-9_]*)\s*(?:\(|$)", part)
            if method:
                claimed.add(method.group(1))

    for method_name in sorted(claimed):
        if not hasattr(owner, method_name):
            errors.append("%s advertises missing method %s" % (owner_name, method_name))

    qualified_calls = re.findall(
        r"unreal\.([A-Z][A-Za-z0-9_]*Service)\.([a-z_][a-z0-9_]*)\s*\(", doc)
    for target_name, method_name in qualified_calls:
        target = getattr(unreal, target_name, None)
        if target is None:
            errors.append("%s example references missing class %s" % (owner_name, target_name))
        elif not hasattr(target, method_name):
            errors.append("%s example calls missing %s.%s" %
                          (owner_name, target_name, method_name))

if errors:
    raise AssertionError("Service docstring drift:\n" + "\n".join(sorted(set(errors))))
)PY");
	PythonCode.ReplaceInline(TEXT("__VIBEUE_SERVICES__"), *FString::Join(QuotedServiceNames, TEXT(", ")));

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!TestNotNull(TEXT("PythonScriptPlugin is loaded"), PythonPlugin))
	{
		return false;
	}

	if (!PythonPlugin->IsPythonInitialized())
	{
		PythonPlugin->ForceEnablePythonAtRuntime();
	}
	if (!TestTrue(TEXT("Python is initialized"), PythonPlugin->IsPythonInitialized()))
	{
		return false;
	}

	FPythonCommandEx Command;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = EPythonFileExecutionScope::Private;
	Command.Flags = EPythonCommandFlags::Unattended;
	Command.Command = MoveTemp(PythonCode);

	const bool bExecuted = PythonPlugin->ExecPythonCommandEx(Command);
	if (!bExecuted)
	{
		FString Details = Command.CommandResult;
		for (const FPythonLogOutputEntry& Entry : Command.LogOutput)
		{
			if (Entry.Type == EPythonLogOutputType::Error)
			{
				Details += TEXT("\n") + Entry.Output;
			}
		}
		AddError(FString::Printf(TEXT("Python service docstring audit failed: %s"), *Details));
	}

	return bExecuted;
}

#endif // WITH_AUTOMATION_TESTS
