// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Pure, side-effect-free colour-string parsing for UWidgetService::SetColor, factored out so the
// automation tests can exercise it headless (no editor, no Widget Blueprint). SetColor routes through
// ParseColor, so the tests and the shipping method share one implementation.
//
// Accepted forms (case-insensitive, surrounding whitespace ignored):
//   "#E53935", "E53935", "#E53935CC"       sRGB hex (RRGGBB / RRGGBBAA)
//   "sRGB(229,57,53)", "rgb(229,57,53)"    sRGB 0-255 channels
//   "rgba(229,57,53,0.5)"                  alpha <= 1 is a 0-1 fraction, otherwise 0-255
//   "linear(0.8,0.04,0.03[,1])"            already-linear 0-1 floats, passed through
//   "(R=0.8,G=0.04,B=0.03,A=1.0)"          FLinearColor export text, passed through
//   "red", "orange", "grey", ...           a small set of named sRGB colours
// Everything except "linear(...)" and the tuple form is sRGB and is converted to linear, because UMG
// colours are linear: a designer's "#8B0000" dark red is roughly (0.258, 0, 0) linear, not (0.545, 0, 0).
namespace WidgetColorParsing
{
	inline bool ParseChannelList(const FString& Inner, TArray<double>& OutValues)
	{
		OutValues.Reset();
		TArray<FString> Parts;
		Inner.ParseIntoArray(Parts, TEXT(","), /*InCullEmpty*/ false);
		for (FString Part : Parts)
		{
			Part.TrimStartAndEndInline();
			if (Part.IsEmpty() || !Part.IsNumeric())
			{
				return false;
			}
			OutValues.Add(FCString::Atod(*Part));
		}
		return OutValues.Num() == 3 || OutValues.Num() == 4;
	}

	inline bool IsHexString(const FString& Text)
	{
		for (const TCHAR Ch : Text)
		{
			if (!FChar::IsHexDigit(Ch))
			{
				return false;
			}
		}
		return !Text.IsEmpty();
	}

	inline bool FindNamedColor(const FString& Name, FColor& OutColor)
	{
		struct FNamed { const TCHAR* Name; FColor Color; };
		static const FNamed Named[] = {
			{ TEXT("white"),       FColor(255, 255, 255, 255) },
			{ TEXT("black"),       FColor(0, 0, 0, 255) },
			{ TEXT("transparent"), FColor(0, 0, 0, 0) },
			{ TEXT("red"),         FColor(255, 0, 0, 255) },
			{ TEXT("green"),       FColor(0, 255, 0, 255) },
			{ TEXT("blue"),        FColor(0, 0, 255, 255) },
			{ TEXT("yellow"),      FColor(255, 255, 0, 255) },
			{ TEXT("cyan"),        FColor(0, 255, 255, 255) },
			{ TEXT("magenta"),     FColor(255, 0, 255, 255) },
			{ TEXT("orange"),      FColor(255, 165, 0, 255) },
			{ TEXT("purple"),      FColor(128, 0, 128, 255) },
			{ TEXT("gray"),        FColor(128, 128, 128, 255) },
			{ TEXT("grey"),        FColor(128, 128, 128, 255) },
		};
		for (const FNamed& Entry : Named)
		{
			if (Name.Equals(Entry.Name, ESearchCase::IgnoreCase))
			{
				OutColor = Entry.Color;
				return true;
			}
		}
		return false;
	}

	inline bool ParseColor(const FString& Input, FLinearColor& OutColor, FString& OutError)
	{
		const FString Text = Input.TrimStartAndEnd();
		if (Text.IsEmpty())
		{
			OutError = TEXT("Colour string is empty");
			return false;
		}

		// FLinearColor export text — already linear.
		if (Text.StartsWith(TEXT("(")))
		{
			FLinearColor Parsed(ForceInit);
			if (!Parsed.InitFromString(Text))
			{
				OutError = FString::Printf(TEXT("'%s' is not a valid \"(R=..,G=..,B=..,A=..)\" tuple"), *Text);
				return false;
			}
			OutColor = Parsed;
			return true;
		}

		// "<func>(a,b,c[,d])" forms.
		const int32 Open = Text.Find(TEXT("("));
		if (Open != INDEX_NONE && Text.EndsWith(TEXT(")")))
		{
			const FString Func = Text.Left(Open).TrimEnd().ToLower();
			const FString Inner = Text.Mid(Open + 1, Text.Len() - Open - 2);
			TArray<double> V;
			if (!ParseChannelList(Inner, V))
			{
				OutError = FString::Printf(TEXT("'%s' must contain 3 or 4 comma-separated numbers"), *Text);
				return false;
			}

			if (Func == TEXT("linear"))
			{
				OutColor = FLinearColor((float)V[0], (float)V[1], (float)V[2], V.Num() == 4 ? (float)V[3] : 1.0f);
				return true;
			}

			if (Func == TEXT("srgb") || Func == TEXT("rgb") || Func == TEXT("rgba"))
			{
				for (int32 i = 0; i < 3; ++i)
				{
					if (V[i] < 0.0 || V[i] > 255.0)
					{
						OutError = FString::Printf(TEXT("'%s': sRGB channels must be 0-255"), *Text);
						return false;
					}
				}
				double Alpha = 255.0;
				if (V.Num() == 4)
				{
					if (V[3] < 0.0 || V[3] > 255.0)
					{
						OutError = FString::Printf(TEXT("'%s': alpha must be 0-1 or 0-255"), *Text);
						return false;
					}
					Alpha = (V[3] <= 1.0) ? V[3] * 255.0 : V[3];
				}
				const FColor SRGB(
					(uint8)FMath::RoundToInt(V[0]),
					(uint8)FMath::RoundToInt(V[1]),
					(uint8)FMath::RoundToInt(V[2]),
					(uint8)FMath::RoundToInt(Alpha));
				OutColor = FLinearColor::FromSRGBColor(SRGB);
				return true;
			}

			OutError = FString::Printf(TEXT("Unknown colour function '%s' — use sRGB(), rgb(), rgba() or linear()"), *Func);
			return false;
		}

		// Hex, with or without '#'.
		const FString Hex = Text.StartsWith(TEXT("#")) ? Text.RightChop(1) : Text;
		if ((Hex.Len() == 6 || Hex.Len() == 8) && IsHexString(Hex))
		{
			OutColor = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
			return true;
		}

		FColor NamedColor;
		if (FindNamedColor(Text, NamedColor))
		{
			OutColor = FLinearColor::FromSRGBColor(NamedColor);
			return true;
		}

		OutError = FString::Printf(
			TEXT("Cannot parse colour '%s' — use \"#RRGGBB\", \"sRGB(r,g,b)\", \"linear(r,g,b)\", \"(R=..,G=..,B=..,A=..)\" or a basic colour name"),
			*Text);
		return false;
	}
}
