// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FabService (#517) auth bridge — obtains an Epic Games access token for fab.com by standing up our
 * OWN EOS platform with the engine Fab plugin's baked credentials (read reflectively from the cooked
 * /Fab/Data/FabEos.FabEos asset) and logging in with EOS_LCT_PersistentAuth, falling back to the
 * launcher exchange code on the command line. This silently reuses the Epic login the editor/launcher
 * already holds — no separate sign-in — and yields a token carrying fab.com's client scopes.
 *
 * We deliberately create a second same-ProductId platform rather than reusing the Fab plugin's private
 * one: the engine's IEOSSDKManager allows it (no dedupe on the raw CreatePlatform overload), auto-ticks
 * it, and this avoids depending on Fab's private auth state or on the Fab UI ever having been opened.
 * All EOS calls are compiled only under WITH_EOS_SDK (defined =1 by the EOSSDK module).
 */
class FVibeFabAuth
{
public:
	/** True on desktop editor builds where the EOS SDK is compiled in and initialized. */
	static bool IsSupported();

	/**
	 * Ensure a login is started and, pumping EOS for up to TimeoutSeconds, try to reach logged-in.
	 * Idempotent: creates the platform once, kicks login once, re-polls on later calls. Returns true
	 * once logged in. On any hard failure sets OutError and returns false.
	 */
	static bool EnsureLoggedIn(double TimeoutSeconds, FString& OutError);

	/** Current EOS login status for our platform. */
	static bool IsLoggedIn();

	/** The current EOS user access token (Bearer for fab.com), or empty if not logged in. */
	static FString GetAccessToken();

	/** The signed-in Epic account id as a string (used in the library URL path), or empty. */
	static FString GetEpicAccountId();

	/** "unsupported" | "idle" | "pending" | "logged_in" | "failed". */
	static FString GetStateString();

	/** Last hard error encountered (creds missing, SDK not initialized, login failed, …). */
	static FString GetLastError();
};
