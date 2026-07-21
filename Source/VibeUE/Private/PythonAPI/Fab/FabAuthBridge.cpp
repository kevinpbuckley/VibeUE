// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "FabAuthBridge.h"
#include "FabEndpoints.h"

#include "UObject/UnrealType.h"
#include "UObject/SoftObjectPath.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeFab, Log, All);

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "EOSShared.h"          // LexToString(EOS_EpicAccountId)
#include "eos_sdk.h"
#include "eos_auth.h"
#include "eos_common.h"
#endif

// ---------------------------------------------------------------------------
// Session state (file-static — one login per editor session)
// ---------------------------------------------------------------------------

namespace
{
	enum class EFabAuthState : uint8 { Idle, Pending, LoggedIn, Failed };

	EFabAuthState GState = EFabAuthState::Idle;
	FString       GLastError;

#if WITH_EOS_SDK
	IEOSPlatformHandlePtr GPlatform;         // keeps our EOS platform alive
	EOS_HAuth             GAuth = nullptr;
	EOS_EpicAccountId     GAccountId = nullptr;
	bool                  GExchangeFallbackTried = false;

	struct FEosProdCreds
	{
		FString ProductId, SandboxId, DeploymentId, ClientId, ClientSecret, EncryptionKey;
		bool IsComplete() const { return !ProductId.IsEmpty() && !DeploymentId.IsEmpty() && !ClientId.IsEmpty(); }
	};

	// Read Prod.* off the cooked UEosConstants asset by reflection — no dependency on Fab's private header.
	bool ReadProdCreds(FEosProdCreds& Out, FString& Err)
	{
		UObject* Asset = FSoftObjectPath(VibeUE::Fab::EosCredsAssetPath()).TryLoad();
		if (!Asset)
		{
			Err = TEXT("Fab credentials asset /Fab/Data/FabEos.FabEos not found — is the Fab plugin enabled?");
			return false;
		}
		FStructProperty* ProdProp = FindFProperty<FStructProperty>(Asset->GetClass(), TEXT("Prod"));
		if (!ProdProp)
		{
			Err = TEXT("FabEos asset has no 'Prod' struct property (unexpected Fab plugin layout).");
			return false;
		}
		const void* Prod = ProdProp->ContainerPtrToValuePtr<void>(Asset);
		UScriptStruct* S = ProdProp->Struct;
		auto Read = [&](const TCHAR* Name) -> FString
		{
			if (const FStrProperty* P = FindFProperty<FStrProperty>(S, Name))
			{
				return P->GetPropertyValue_InContainer(Prod);
			}
			return FString();
		};
		Out.ProductId     = Read(TEXT("ProductId"));
		Out.SandboxId     = Read(TEXT("SandboxId"));
		Out.DeploymentId  = Read(TEXT("DeploymentId"));
		Out.ClientId      = Read(TEXT("ClientCredentialsId"));
		Out.ClientSecret  = Read(TEXT("ClientCredentialsSecret"));
		Out.EncryptionKey = Read(TEXT("EncryptionKey"));
		if (!Out.IsComplete())
		{
			Err = TEXT("FabEos Prod credentials are incomplete.");
			return false;
		}
		return true;
	}

	// Copy the current account whose login status is LoggedIn into GAccountId. Returns true if found.
	bool CaptureLoggedInAccount()
	{
		if (!GAuth)
		{
			return false;
		}
		const int32_t Count = EOS_Auth_GetLoggedInAccountsCount(GAuth);
		for (int32_t i = 0; i < Count; ++i)
		{
			const EOS_EpicAccountId Acct = EOS_Auth_GetLoggedInAccountByIndex(GAuth, i);
			if (EOS_Auth_GetLoginStatus(GAuth, Acct) == EOS_ELoginStatus::EOS_LS_LoggedIn)
			{
				GAccountId = Acct;
				return true;
			}
		}
		return false;
	}

	void EOS_CALL OnLoginComplete(const EOS_Auth_LoginCallbackInfo* Data);

	void StartExchangeCodeLogin()
	{
		// Read the launcher-provided exchange code from the editor command line (same as the Fab plugin).
		FString ExchangeCode;
		FString AuthType;
		if (FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), AuthType) && AuthType == TEXT("exchangecode"))
		{
			FParse::Value(FCommandLine::Get(), TEXT("AUTH_PASSWORD="), ExchangeCode);
		}
		if (ExchangeCode.IsEmpty())
		{
			GState = EFabAuthState::Failed;
			GLastError = TEXT("Not signed in: persistent auth unavailable and no launcher exchange code on the command line. Open the Fab window or launch the editor from the Epic Games Launcher, then retry.");
			return;
		}

		const FTCHARToUTF8 Utf8Code(*ExchangeCode);
		EOS_Auth_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
		Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_ExchangeCode;
		Credentials.Id = "";
		Credentials.Token = Utf8Code.Get();

		EOS_Auth_LoginOptions LoginOptions = {};
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.Credentials = &Credentials;

		EOS_Auth_Login(GAuth, &LoginOptions, nullptr, OnLoginComplete);
	}

	void EOS_CALL OnLoginComplete(const EOS_Auth_LoginCallbackInfo* Data)
	{
		if (Data->ResultCode == EOS_EResult::EOS_Success)
		{
			if (CaptureLoggedInAccount())
			{
				GState = EFabAuthState::LoggedIn;
				GLastError.Reset();
			}
			else
			{
				GState = EFabAuthState::Failed;
				GLastError = TEXT("Login reported success but no logged-in account was found.");
			}
			return;
		}

		// Persistent-auth failure → fall back to the launcher exchange code exactly once.
		if (!GExchangeFallbackTried)
		{
			GExchangeFallbackTried = true;
			StartExchangeCodeLogin();
			return;
		}

		GState = EFabAuthState::Failed;
		GLastError = FString::Printf(TEXT("EOS login failed: %s"), *FString(EOS_EResult_ToString(Data->ResultCode)));
	}

	// Create our platform (once) and kick the persistent-auth login (once). Returns false + Err on hard failure.
	bool EnsurePlatformAndLogin(FString& Err)
	{
		if (GState == EFabAuthState::LoggedIn || GState == EFabAuthState::Pending)
		{
			return true;
		}
		// A prior Failed state can be retried (e.g. the user signed into Fab since) — reset and try again.
		GState = EFabAuthState::Idle;
		GExchangeFallbackTried = false;

		IEOSSDKManager* SDK = IEOSSDKManager::Get();
		if (!SDK || !SDK->IsInitialized())
		{
			Err = TEXT("EOS SDK is not initialized in this editor process.");
			return false;
		}

		if (!GPlatform)
		{
			FEosProdCreds Creds;
			if (!ReadProdCreds(Creds, Err))
			{
				return false;
			}

			const FTCHARToUTF8 Utf8ProductId(*Creds.ProductId);
			const FTCHARToUTF8 Utf8SandboxId(*Creds.SandboxId);
			const FTCHARToUTF8 Utf8DeploymentId(*Creds.DeploymentId);
			const FTCHARToUTF8 Utf8ClientId(*Creds.ClientId);
			const FTCHARToUTF8 Utf8ClientSecret(*Creds.ClientSecret);
			const FTCHARToUTF8 Utf8EncryptionKey(*Creds.EncryptionKey);

			EOS_Platform_Options PlatformOptions = {};
			PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
			PlatformOptions.ClientCredentials.ClientId = Utf8ClientId.Get();
			PlatformOptions.ClientCredentials.ClientSecret = Utf8ClientSecret.Get();
			PlatformOptions.ProductId = Utf8ProductId.Get();
			PlatformOptions.DeploymentId = Utf8DeploymentId.Get();
			PlatformOptions.SandboxId = Utf8SandboxId.Get();
			PlatformOptions.EncryptionKey = Utf8EncryptionKey.Get();
			PlatformOptions.bIsServer = EOS_FALSE;
			PlatformOptions.Flags = EOS_PF_DISABLE_OVERLAY;
			PlatformOptions.TickBudgetInMilliseconds = 0;

			GPlatform = SDK->CreatePlatform(PlatformOptions);
			if (!GPlatform)
			{
				Err = TEXT("Failed to create the EOS platform for Fab auth.");
				return false;
			}
		}

		GAuth = EOS_Platform_GetAuthInterface(*GPlatform);
		if (!GAuth)
		{
			Err = TEXT("Failed to get the EOS auth interface.");
			return false;
		}

		EOS_Auth_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;
		Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;

		EOS_Auth_LoginOptions LoginOptions = {};
		LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
		LoginOptions.Credentials = &Credentials;

		GState = EFabAuthState::Pending;
		EOS_Auth_Login(GAuth, &LoginOptions, nullptr, OnLoginComplete);
		return true;
	}
#endif // WITH_EOS_SDK
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool FVibeFabAuth::IsSupported()
{
#if WITH_EOS_SDK
	IEOSSDKManager* SDK = IEOSSDKManager::Get();
	return SDK != nullptr && SDK->IsInitialized();
#else
	return false;
#endif
}

bool FVibeFabAuth::EnsureLoggedIn(double TimeoutSeconds, FString& OutError)
{
#if WITH_EOS_SDK
	if (GState == EFabAuthState::LoggedIn)
	{
		return true;
	}
	if (!EnsurePlatformAndLogin(OutError))
	{
		GState = EFabAuthState::Failed;
		GLastError = OutError;
		return false;
	}

	// Pump EOS on this (game) thread until the async login callback resolves or we time out. During
	// this synchronous call the SDK manager's auto-ticker is NOT running, so we tick the platform
	// ourselves. Bounded — consistent with the plugin's synchronous-HTTP waits; never an infinite loop.
	const double Start = FPlatformTime::Seconds();
	while (GState == EFabAuthState::Pending && (FPlatformTime::Seconds() - Start) < TimeoutSeconds)
	{
		if (GPlatform)
		{
			EOS_Platform_Tick(*GPlatform);
		}
		FPlatformProcess::Sleep(0.02f);
	}

	if (GState == EFabAuthState::LoggedIn)
	{
		return true;
	}
	if (GState == EFabAuthState::Failed)
	{
		OutError = GLastError;
		return false;
	}
	// Still pending after the timeout — not an error; the caller can re-poll on a later request.
	OutError = TEXT("Login still in progress.");
	return false;
#else
	OutError = TEXT("EOS SDK not compiled into this build.");
	return false;
#endif
}

bool FVibeFabAuth::IsLoggedIn()
{
#if WITH_EOS_SDK
	return GState == EFabAuthState::LoggedIn && GAuth && GAccountId
		&& EOS_Auth_GetLoginStatus(GAuth, GAccountId) == EOS_ELoginStatus::EOS_LS_LoggedIn;
#else
	return false;
#endif
}

FString FVibeFabAuth::GetAccessToken()
{
#if WITH_EOS_SDK
	if (!IsLoggedIn())
	{
		return FString();
	}
	EOS_Auth_Token* Token = nullptr;
	EOS_Auth_CopyUserAuthTokenOptions Opts = {};
	Opts.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;
	if (EOS_Auth_CopyUserAuthToken(GAuth, &Opts, GAccountId, &Token) == EOS_EResult::EOS_Success)
	{
		const FString Access = FString(Token->AccessToken);
		EOS_Auth_Token_Release(Token);
		return Access;
	}
	return FString();
#else
	return FString();
#endif
}

FString FVibeFabAuth::GetEpicAccountId()
{
#if WITH_EOS_SDK
	if (GAccountId && IsLoggedIn())
	{
		return LexToString(GAccountId);
	}
	return FString();
#else
	return FString();
#endif
}

FString FVibeFabAuth::GetStateString()
{
#if WITH_EOS_SDK
	switch (GState)
	{
	case EFabAuthState::LoggedIn: return TEXT("logged_in");
	case EFabAuthState::Pending:  return TEXT("pending");
	case EFabAuthState::Failed:   return TEXT("failed");
	default:                      return TEXT("idle");
	}
#else
	return TEXT("unsupported");
#endif
}

FString FVibeFabAuth::GetLastError()
{
	return GLastError;
}
