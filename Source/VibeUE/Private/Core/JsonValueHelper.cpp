// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Core/JsonValueHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TSharedPtr<FJsonValue> FJsonValueHelper::ParseStringToValue(const FString& StringValue)
{
	FString Trimmed = StringValue.TrimStartAndEnd();
	
	// Empty string stays as string
	if (Trimmed.IsEmpty())
	{
		return MakeShared<FJsonValueString>(StringValue);
	}
	
	// Try to parse as JSON if it looks like JSON
	if (LooksLikeJson(Trimmed))
	{
		TSharedPtr<FJsonValue> ParsedValue;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
		
		if (FJsonSerializer::Deserialize(Reader, ParsedValue) && ParsedValue.IsValid())
		{
			return ParsedValue;
		}
	}
	
	// Check for boolean strings
	FString LowerTrimmed = Trimmed.ToLower();
	if (LowerTrimmed == TEXT("true") || LowerTrimmed == TEXT("yes"))
	{
		return MakeShared<FJsonValueBoolean>(true);
	}
	if (LowerTrimmed == TEXT("false") || LowerTrimmed == TEXT("no"))
	{
		return MakeShared<FJsonValueBoolean>(false);
	}
	
	// Check for null
	if (LowerTrimmed == TEXT("null"))
	{
		return MakeShared<FJsonValueNull>();
	}
	
	// Check for number (integer or float)
	if (Trimmed.IsNumeric() || 
	    (Trimmed.StartsWith(TEXT("-")) && Trimmed.Mid(1).IsNumeric()) ||
	    Trimmed.Contains(TEXT(".")))
	{
		double NumValue;
		if (LexTryParseString(NumValue, *Trimmed))
		{
			return MakeShared<FJsonValueNumber>(NumValue);
		}
	}
	
	// Return as string
	return MakeShared<FJsonValueString>(StringValue);
}

TSharedPtr<FJsonValue> FJsonValueHelper::CoerceValue(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return Value;
	}
	
	// Only coerce string values
	if (Value->Type != EJson::String)
	{
		return Value;
	}
	
	return ParseStringToValue(Value->AsString());
}

bool FJsonValueHelper::TryGetArray(const TSharedPtr<FJsonValue>& Value, TArray<TSharedPtr<FJsonValue>>& OutArray)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Direct array
	if (Value->Type == EJson::Array)
	{
		OutArray = Value->AsArray();
		return true;
	}
	
	// String that might be an array
	if (Value->Type == EJson::String)
	{
		TSharedPtr<FJsonValue> Parsed = ParseStringToValue(Value->AsString());
		if (Parsed.IsValid() && Parsed->Type == EJson::Array)
		{
			OutArray = Parsed->AsArray();
			return true;
		}
	}
	
	return false;
}

bool FJsonValueHelper::TryGetObject(const TSharedPtr<FJsonValue>& Value, TSharedPtr<FJsonObject>& OutObject)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Direct object
	if (Value->Type == EJson::Object)
	{
		OutObject = Value->AsObject();
		return true;
	}
	
	// String that might be an object
	if (Value->Type == EJson::String)
	{
		TSharedPtr<FJsonValue> Parsed = ParseStringToValue(Value->AsString());
		if (Parsed.IsValid() && Parsed->Type == EJson::Object)
		{
			OutObject = Parsed->AsObject();
			return true;
		}
	}
	
	return false;
}

bool FJsonValueHelper::TryGetNumberArray(const TSharedPtr<FJsonValue>& Value, TArray<double>& OutNumbers)
{
	TArray<TSharedPtr<FJsonValue>> ArrayValues;
	if (!TryGetArray(Value, ArrayValues))
	{
		return false;
	}
	
	OutNumbers.Empty();
	for (const auto& ArrayValue : ArrayValues)
	{
		double Num;
		if (TryGetNumber(ArrayValue, Num))
		{
			OutNumbers.Add(Num);
		}
		else
		{
			return false;
		}
	}
	
	return true;
}

bool FJsonValueHelper::TryGetVector2D(const TSharedPtr<FJsonValue>& Value, FVector2D& OutVector)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Coerce string to actual value
	TSharedPtr<FJsonValue> CoercedValue = CoerceValue(Value);
	
	// Try array [x, y]
	if (CoercedValue->Type == EJson::Array)
	{
		TArray<double> Numbers;
		if (TryGetNumberArray(CoercedValue, Numbers) && Numbers.Num() >= 2)
		{
			OutVector.X = Numbers[0];
			OutVector.Y = Numbers[1];
			return true;
		}
	}
	
	// Try object {X: x, Y: y} or {x: x, y: y}
	if (CoercedValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject>& Obj = CoercedValue->AsObject();
		double X = 0, Y = 0;
		bool bHasX = false, bHasY = false;
		
		if (Obj->TryGetNumberField(TEXT("X"), X) || Obj->TryGetNumberField(TEXT("x"), X))
		{
			bHasX = true;
		}
		if (Obj->TryGetNumberField(TEXT("Y"), Y) || Obj->TryGetNumberField(TEXT("y"), Y))
		{
			bHasY = true;
		}
		
		if (bHasX && bHasY)
		{
			OutVector.X = X;
			OutVector.Y = Y;
			return true;
		}
	}
	
	return false;
}

bool FJsonValueHelper::TryGetVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Coerce string to actual value
	TSharedPtr<FJsonValue> CoercedValue = CoerceValue(Value);
	
	// Try array [x, y, z]
	if (CoercedValue->Type == EJson::Array)
	{
		TArray<double> Numbers;
		if (TryGetNumberArray(CoercedValue, Numbers) && Numbers.Num() >= 3)
		{
			OutVector.X = Numbers[0];
			OutVector.Y = Numbers[1];
			OutVector.Z = Numbers[2];
			return true;
		}
	}
	
	// Try object {X: x, Y: y, Z: z}
	if (CoercedValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject>& Obj = CoercedValue->AsObject();
		double X = 0, Y = 0, Z = 0;
		bool bHasX = false, bHasY = false, bHasZ = false;
		
		if (Obj->TryGetNumberField(TEXT("X"), X) || Obj->TryGetNumberField(TEXT("x"), X))
		{
			bHasX = true;
		}
		if (Obj->TryGetNumberField(TEXT("Y"), Y) || Obj->TryGetNumberField(TEXT("y"), Y))
		{
			bHasY = true;
		}
		if (Obj->TryGetNumberField(TEXT("Z"), Z) || Obj->TryGetNumberField(TEXT("z"), Z))
		{
			bHasZ = true;
		}
		
		if (bHasX && bHasY && bHasZ)
		{
			OutVector.X = X;
			OutVector.Y = Y;
			OutVector.Z = Z;
			return true;
		}
	}
	
	return false;
}

bool FJsonValueHelper::TryGetRotator(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Coerce string to actual value
	TSharedPtr<FJsonValue> CoercedValue = CoerceValue(Value);
	
	// Try array [pitch, yaw, roll]
	if (CoercedValue->Type == EJson::Array)
	{
		TArray<double> Numbers;
		if (TryGetNumberArray(CoercedValue, Numbers) && Numbers.Num() >= 3)
		{
			OutRotator.Pitch = Numbers[0];
			OutRotator.Yaw = Numbers[1];
			OutRotator.Roll = Numbers[2];
			return true;
		}
	}
	
	// Try object {Pitch: p, Yaw: y, Roll: r}
	if (CoercedValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject>& Obj = CoercedValue->AsObject();
		double Pitch = 0, Yaw = 0, Roll = 0;
		bool bHasPitch = false, bHasYaw = false, bHasRoll = false;
		
		if (Obj->TryGetNumberField(TEXT("Pitch"), Pitch) || Obj->TryGetNumberField(TEXT("pitch"), Pitch) ||
		    Obj->TryGetNumberField(TEXT("P"), Pitch) || Obj->TryGetNumberField(TEXT("p"), Pitch) ||
		    Obj->TryGetNumberField(TEXT("X"), Pitch) || Obj->TryGetNumberField(TEXT("x"), Pitch))
		{
			bHasPitch = true;
		}
		if (Obj->TryGetNumberField(TEXT("Yaw"), Yaw) || Obj->TryGetNumberField(TEXT("yaw"), Yaw) ||
		    Obj->TryGetNumberField(TEXT("Y"), Yaw) || Obj->TryGetNumberField(TEXT("y"), Yaw))
		{
			bHasYaw = true;
		}
		if (Obj->TryGetNumberField(TEXT("Roll"), Roll) || Obj->TryGetNumberField(TEXT("roll"), Roll) ||
		    Obj->TryGetNumberField(TEXT("R"), Roll) || Obj->TryGetNumberField(TEXT("r"), Roll) ||
		    Obj->TryGetNumberField(TEXT("Z"), Roll) || Obj->TryGetNumberField(TEXT("z"), Roll))
		{
			bHasRoll = true;
		}
		
		if (bHasPitch && bHasYaw && bHasRoll)
		{
			OutRotator.Pitch = Pitch;
			OutRotator.Yaw = Yaw;
			OutRotator.Roll = Roll;
			return true;
		}
	}
	
	return false;
}

bool FJsonValueHelper::TryGetMargin(const TSharedPtr<FJsonValue>& Value, float& OutLeft, float& OutTop, float& OutRight, float& OutBottom)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Coerce string to actual value
	TSharedPtr<FJsonValue> CoercedValue = CoerceValue(Value);
	
	// Single number = uniform margin
	double SingleNum;
	if (TryGetNumber(CoercedValue, SingleNum))
	{
		OutLeft = OutTop = OutRight = OutBottom = static_cast<float>(SingleNum);
		return true;
	}
	
	// Try array
	if (CoercedValue->Type == EJson::Array)
	{
		TArray<double> Numbers;
		if (TryGetNumberArray(CoercedValue, Numbers))
		{
			if (Numbers.Num() == 1)
			{
				// [uniform]
				OutLeft = OutTop = OutRight = OutBottom = static_cast<float>(Numbers[0]);
				return true;
			}
			else if (Numbers.Num() == 2)
			{
				// [horizontal, vertical]
				OutLeft = OutRight = static_cast<float>(Numbers[0]);
				OutTop = OutBottom = static_cast<float>(Numbers[1]);
				return true;
			}
			else if (Numbers.Num() >= 4)
			{
				// [left, top, right, bottom]
				OutLeft = static_cast<float>(Numbers[0]);
				OutTop = static_cast<float>(Numbers[1]);
				OutRight = static_cast<float>(Numbers[2]);
				OutBottom = static_cast<float>(Numbers[3]);
				return true;
			}
		}
	}
	
	// Try object {Left: l, Top: t, Right: r, Bottom: b}
	if (CoercedValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject>& Obj = CoercedValue->AsObject();
		double Left = 0, Top = 0, Right = 0, Bottom = 0;
		
		Obj->TryGetNumberField(TEXT("Left"), Left) || Obj->TryGetNumberField(TEXT("left"), Left) ||
		    Obj->TryGetNumberField(TEXT("L"), Left) || Obj->TryGetNumberField(TEXT("l"), Left);
		Obj->TryGetNumberField(TEXT("Top"), Top) || Obj->TryGetNumberField(TEXT("top"), Top) ||
		    Obj->TryGetNumberField(TEXT("T"), Top) || Obj->TryGetNumberField(TEXT("t"), Top);
		Obj->TryGetNumberField(TEXT("Right"), Right) || Obj->TryGetNumberField(TEXT("right"), Right) ||
		    Obj->TryGetNumberField(TEXT("R"), Right) || Obj->TryGetNumberField(TEXT("r"), Right);
		Obj->TryGetNumberField(TEXT("Bottom"), Bottom) || Obj->TryGetNumberField(TEXT("bottom"), Bottom) ||
		    Obj->TryGetNumberField(TEXT("B"), Bottom) || Obj->TryGetNumberField(TEXT("b"), Bottom);
		
		OutLeft = static_cast<float>(Left);
		OutTop = static_cast<float>(Top);
		OutRight = static_cast<float>(Right);
		OutBottom = static_cast<float>(Bottom);
		return true;
	}
	
	return false;
}

bool FJsonValueHelper::TryGetString(const TSharedPtr<FJsonValue>& Value, FString& OutString)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Direct string
	if (Value->Type == EJson::String)
	{
		OutString = Value->AsString();
		return true;
	}
	
	// Number to string
	if (Value->Type == EJson::Number)
	{
		OutString = FString::Printf(TEXT("%g"), Value->AsNumber());
		return true;
	}
	
	// Boolean to string
	if (Value->Type == EJson::Boolean)
	{
		OutString = Value->AsBool() ? TEXT("true") : TEXT("false");
		return true;
	}
	
	// Null to empty string
	if (Value->Type == EJson::Null)
	{
		OutString = TEXT("");
		return true;
	}
	
	return false;
}

bool FJsonValueHelper::TryGetLinearColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Handle string colors first (hex, named)
	if (Value->Type == EJson::String)
	{
		FString ColorStr = Value->AsString().TrimStartAndEnd();
		
		// Try hex color
		if (ColorStr.StartsWith(TEXT("#")))
		{
			if (TryParseHexColor(ColorStr, OutColor))
			{
				return true;
			}
		}
		
		// Try named color
		if (TryParseNamedColor(ColorStr.ToLower(), OutColor))
		{
			return true;
		}
	}
	
	// Coerce string to actual value (for JSON arrays/objects)
	TSharedPtr<FJsonValue> CoercedValue = CoerceValue(Value);
	
	// Try array [r, g, b] or [r, g, b, a]
	if (CoercedValue->Type == EJson::Array)
	{
		TArray<double> Numbers;
		if (TryGetNumberArray(CoercedValue, Numbers) && Numbers.Num() >= 3)
		{
			OutColor.R = Numbers[0];
			OutColor.G = Numbers[1];
			OutColor.B = Numbers[2];
			OutColor.A = Numbers.Num() >= 4 ? Numbers[3] : 1.0;
			return true;
		}
	}
	
	// Try object {R: r, G: g, B: b, A: a}
	if (CoercedValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject>& Obj = CoercedValue->AsObject();
		double R = 0, G = 0, B = 0, A = 1.0;
		bool bHasR = false, bHasG = false, bHasB = false;
		
		if (Obj->TryGetNumberField(TEXT("R"), R) || Obj->TryGetNumberField(TEXT("r"), R) || 
		    Obj->TryGetNumberField(TEXT("Red"), R) || Obj->TryGetNumberField(TEXT("red"), R))
		{
			bHasR = true;
		}
		if (Obj->TryGetNumberField(TEXT("G"), G) || Obj->TryGetNumberField(TEXT("g"), G) ||
		    Obj->TryGetNumberField(TEXT("Green"), G) || Obj->TryGetNumberField(TEXT("green"), G))
		{
			bHasG = true;
		}
		if (Obj->TryGetNumberField(TEXT("B"), B) || Obj->TryGetNumberField(TEXT("b"), B) ||
		    Obj->TryGetNumberField(TEXT("Blue"), B) || Obj->TryGetNumberField(TEXT("blue"), B))
		{
			bHasB = true;
		}
		Obj->TryGetNumberField(TEXT("A"), A) || Obj->TryGetNumberField(TEXT("a"), A) ||
		Obj->TryGetNumberField(TEXT("Alpha"), A) || Obj->TryGetNumberField(TEXT("alpha"), A);
		
		if (bHasR && bHasG && bHasB)
		{
			OutColor.R = R;
			OutColor.G = G;
			OutColor.B = B;
			OutColor.A = A;
			return true;
		}
	}
	
	return false;
}

bool FJsonValueHelper::TryGetBool(const TSharedPtr<FJsonValue>& Value, bool& OutBool)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Direct boolean
	if (Value->Type == EJson::Boolean)
	{
		OutBool = Value->AsBool();
		return true;
	}
	
	// Number (0 = false, non-zero = true)
	if (Value->Type == EJson::Number)
	{
		OutBool = Value->AsNumber() != 0.0;
		return true;
	}
	
	// String
	if (Value->Type == EJson::String)
	{
		FString StrValue = Value->AsString().TrimStartAndEnd().ToLower();
		if (StrValue == TEXT("true") || StrValue == TEXT("yes") || StrValue == TEXT("1") || StrValue == TEXT("on"))
		{
			OutBool = true;
			return true;
		}
		if (StrValue == TEXT("false") || StrValue == TEXT("no") || StrValue == TEXT("0") || StrValue == TEXT("off"))
		{
			OutBool = false;
			return true;
		}
	}
	
	return false;
}

bool FJsonValueHelper::TryGetNumber(const TSharedPtr<FJsonValue>& Value, double& OutNumber)
{
	if (!Value.IsValid())
	{
		return false;
	}
	
	// Direct number
	if (Value->Type == EJson::Number)
	{
		OutNumber = Value->AsNumber();
		return true;
	}
	
	// Boolean (true = 1, false = 0)
	if (Value->Type == EJson::Boolean)
	{
		OutNumber = Value->AsBool() ? 1.0 : 0.0;
		return true;
	}
	
	// String
	if (Value->Type == EJson::String)
	{
		FString StrValue = Value->AsString().TrimStartAndEnd();
		if (LexTryParseString(OutNumber, *StrValue))
		{
			return true;
		}
	}
	
	return false;
}

TSharedPtr<FJsonValue> FJsonValueHelper::MakeArrayValue(const TArray<double>& Numbers)
{
	TArray<TSharedPtr<FJsonValue>> ArrayValues;
	for (double Num : Numbers)
	{
		ArrayValues.Add(MakeShared<FJsonValueNumber>(Num));
	}
	return MakeShared<FJsonValueArray>(ArrayValues);
}

TSharedPtr<FJsonValue> FJsonValueHelper::MakeArrayValue(const FVector2D& Vector)
{
	TArray<TSharedPtr<FJsonValue>> ArrayValues;
	ArrayValues.Add(MakeShared<FJsonValueNumber>(Vector.X));
	ArrayValues.Add(MakeShared<FJsonValueNumber>(Vector.Y));
	return MakeShared<FJsonValueArray>(ArrayValues);
}

TSharedPtr<FJsonValue> FJsonValueHelper::MakeArrayValue(const FVector& Vector)
{
	TArray<TSharedPtr<FJsonValue>> ArrayValues;
	ArrayValues.Add(MakeShared<FJsonValueNumber>(Vector.X));
	ArrayValues.Add(MakeShared<FJsonValueNumber>(Vector.Y));
	ArrayValues.Add(MakeShared<FJsonValueNumber>(Vector.Z));
	return MakeShared<FJsonValueArray>(ArrayValues);
}

bool FJsonValueHelper::LooksLikeJson(const FString& Str)
{
	FString Trimmed = Str.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}
	
	TCHAR FirstChar = Trimmed[0];
	TCHAR LastChar = Trimmed[Trimmed.Len() - 1];
	
	// Array: [...]
	if (FirstChar == TEXT('[') && LastChar == TEXT(']'))
	{
		return true;
	}
	
	// Object: {...}
	if (FirstChar == TEXT('{') && LastChar == TEXT('}'))
	{
		return true;
	}
	
	// Quoted string: "..."
	if (FirstChar == TEXT('"') && LastChar == TEXT('"'))
	{
		return true;
	}
	
	return false;
}

bool FJsonValueHelper::TryParseHexColor(const FString& HexStr, FLinearColor& OutColor)
{
	FString Hex = HexStr;
	if (Hex.StartsWith(TEXT("#")))
	{
		Hex = Hex.Mid(1);
	}
	
	// Support 3, 6, or 8 character hex
	if (Hex.Len() == 3)
	{
		// Expand #RGB to #RRGGBB
		Hex = FString::Printf(TEXT("%c%c%c%c%c%c"), 
			Hex[0], Hex[0], Hex[1], Hex[1], Hex[2], Hex[2]);
	}
	
	if (Hex.Len() == 6)
	{
		Hex += TEXT("FF"); // Add full alpha
	}
	
	if (Hex.Len() != 8)
	{
		return false;
	}
	
	// Parse RRGGBBAA using FColor
	FColor ParsedColor = FColor::FromHex(Hex);
	OutColor = FLinearColor(ParsedColor);
	return true;
}

bool FJsonValueHelper::TryParseNamedColor(const FString& ColorName, FLinearColor& OutColor)
{
	// Common color names
	if (ColorName == TEXT("white"))
	{
		OutColor = FLinearColor::White;
		return true;
	}
	if (ColorName == TEXT("black"))
	{
		OutColor = FLinearColor::Black;
		return true;
	}
	if (ColorName == TEXT("red"))
	{
		OutColor = FLinearColor::Red;
		return true;
	}
	if (ColorName == TEXT("green"))
	{
		OutColor = FLinearColor::Green;
		return true;
	}
	if (ColorName == TEXT("blue"))
	{
		OutColor = FLinearColor::Blue;
		return true;
	}
	if (ColorName == TEXT("yellow"))
	{
		OutColor = FLinearColor::Yellow;
		return true;
	}
	if (ColorName == TEXT("cyan"))
	{
		OutColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);
		return true;
	}
	if (ColorName == TEXT("magenta") || ColorName == TEXT("purple"))
	{
		OutColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
		return true;
	}
	if (ColorName == TEXT("orange"))
	{
		OutColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
		return true;
	}
	if (ColorName == TEXT("gray") || ColorName == TEXT("grey"))
	{
		OutColor = FLinearColor::Gray;
		return true;
	}
	if (ColorName == TEXT("transparent"))
	{
		OutColor = FLinearColor::Transparent;
		return true;
	}
	
	return false;
}
