// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

/**
 * Smart JSON value parsing utilities.
 * 
 * Handles the common problem where LLM models send JSON values as escaped strings
 * due to nested JSON encoding (e.g., ParamsJson containing property_value).
 * 
 * Examples of what this handles:
 *   "[1.0, 2.0]" (string) -> [1.0, 2.0] (array)
 *   "{\"x\": 1}" (string) -> {x: 1} (object)
 *   "true" (string) -> true (bool)
 *   "42" (string) -> 42 (number)
 */
class VIBEUE_API FJsonValueHelper
{
public:
	/**
	 * Try to parse a string JSON value into its actual type.
	 * If the string contains a JSON array, object, boolean, number, or null,
	 * returns a new FJsonValue of the appropriate type.
	 * If parsing fails, returns the original string value.
	 * 
	 * @param StringValue The string to attempt to parse
	 * @return Parsed JSON value (array, object, bool, number) or original string
	 */
	static TSharedPtr<FJsonValue> ParseStringToValue(const FString& StringValue);
	
	/**
	 * Smart coerce a JSON value to its "true" type.
	 * If the value is a string that looks like JSON, parse it.
	 * Otherwise return the original value.
	 * 
	 * @param Value The JSON value to coerce
	 * @return Coerced value (may be same as input if no coercion needed)
	 */
	static TSharedPtr<FJsonValue> CoerceValue(const TSharedPtr<FJsonValue>& Value);
	
	/**
	 * Try to get an array from a JSON value, handling string representations.
	 * 
	 * @param Value The JSON value (may be array or string containing array)
	 * @param OutArray Output array of values
	 * @return True if successfully extracted array
	 */
	static bool TryGetArray(const TSharedPtr<FJsonValue>& Value, TArray<TSharedPtr<FJsonValue>>& OutArray);
	
	/**
	 * Try to get an object from a JSON value, handling string representations.
	 * 
	 * @param Value The JSON value (may be object or string containing object)
	 * @param OutObject Output object
	 * @return True if successfully extracted object
	 */
	static bool TryGetObject(const TSharedPtr<FJsonValue>& Value, TSharedPtr<FJsonObject>& OutObject);
	
	/**
	 * Try to get a number array (e.g., [1.0, 2.0]) from a JSON value.
	 * Handles both actual arrays and string representations.
	 * 
	 * @param Value The JSON value
	 * @param OutNumbers Output array of numbers
	 * @return True if successfully extracted number array
	 */
	static bool TryGetNumberArray(const TSharedPtr<FJsonValue>& Value, TArray<double>& OutNumbers);
	
	/**
	 * Try to get a Vector2D from a JSON value.
	 * Accepts: [x, y], {"X": x, "Y": y}, {"x": x, "y": y}, or string versions.
	 * 
	 * @param Value The JSON value
	 * @param OutVector Output vector
	 * @return True if successfully extracted vector
	 */
	static bool TryGetVector2D(const TSharedPtr<FJsonValue>& Value, FVector2D& OutVector);
	
	/**
	 * Try to get a Vector2D from a field in a JSON object.
	 * Convenience wrapper around TryGetVector2D that handles field extraction.
	 * 
	 * @param Object The JSON object containing the field
	 * @param FieldName The name of the field to extract
	 * @param OutVector Output vector
	 * @return True if field exists and was successfully parsed as a Vector2D
	 */
	static bool TryGetVector2DField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FVector2D& OutVector);
	
	/**
	 * Try to get a Vector from a JSON value.
	 * Accepts: [x, y, z], {"X": x, "Y": y, "Z": z}, or string versions.
	 * 
	 * @param Value The JSON value
	 * @param OutVector Output vector
	 * @return True if successfully extracted vector
	 */
	static bool TryGetVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector);
	
	/**
	 * Try to get a Vector from a field in a JSON object.
	 * Convenience wrapper around TryGetVector that handles field extraction.
	 * 
	 * @param Object The JSON object containing the field
	 * @param FieldName The name of the field to extract
	 * @param OutVector Output vector
	 * @return True if field exists and was successfully parsed as a Vector
	 */
	static bool TryGetVectorField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FVector& OutVector);
	
	/**
	 * Try to get a Rotator from a JSON value.
	 * Accepts: [pitch, yaw, roll], {"Pitch": p, "Yaw": y, "Roll": r}, or string versions.
	 * Also accepts lowercase and P/Y/R abbreviations.
	 * 
	 * @param Value The JSON value
	 * @param OutRotator Output rotator
	 * @return True if successfully extracted rotator
	 */
	static bool TryGetRotator(const TSharedPtr<FJsonValue>& Value, FRotator& OutRotator);
	
	/**
	 * Try to get a Margin from a JSON value.
	 * Accepts: [left, top, right, bottom], single number (uniform), 
	 * [horizontal, vertical], or object {"Left": l, "Top": t, "Right": r, "Bottom": b}.
	 * 
	 * @param Value The JSON value
	 * @param OutMargin Output margin (as 4 floats: left, top, right, bottom)
	 * @return True if successfully extracted margin
	 */
	static bool TryGetMargin(const TSharedPtr<FJsonValue>& Value, float& OutLeft, float& OutTop, float& OutRight, float& OutBottom);
	
	/**
	 * Try to get an FMargin from a JSON value.
	 * Accepts: [left, top, right, bottom], single number (uniform), 
	 * [horizontal, vertical], or object {"Left": l, "Top": t, "Right": r, "Bottom": b}.
	 * 
	 * @param Value The JSON value
	 * @param OutMargin Output margin
	 * @return True if successfully extracted margin
	 */
	static bool TryGetMargin(const TSharedPtr<FJsonValue>& Value, FMargin& OutMargin);
	
	/**
	 * Try to get an FMargin from a field in a JSON object.
	 * Convenience wrapper around TryGetMargin that handles field extraction.
	 * 
	 * @param Object The JSON object containing the field
	 * @param FieldName The name of the field to extract
	 * @param OutMargin Output margin
	 * @return True if field exists and was successfully parsed as a margin
	 */
	static bool TryGetMarginField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FMargin& OutMargin);
	
	/**
	 * Try to get a string from a JSON value.
	 * Numbers and booleans are converted to strings.
	 * 
	 * @param Value The JSON value
	 * @param OutString Output string
	 * @return True if successfully extracted string
	 */
	static bool TryGetString(const TSharedPtr<FJsonValue>& Value, FString& OutString);
	
	/**
	 * Try to get a linear color from a JSON value.
	 * Accepts: [r, g, b, a], {"R": r, "G": g, "B": b, "A": a}, 
	 * hex strings like "#FF0000", or color names like "red", "white".
	 * 
	 * @param Value The JSON value
	 * @param OutColor Output color
	 * @return True if successfully extracted color
	 */
	static bool TryGetLinearColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor);
	
	/**
	 * Try to parse a linear color from a string.
	 * Accepts: hex strings "#FF0000" or "#FF8800FF", color names like "red", "warm",
	 * Unreal format "(R=1.0,G=0.5,B=0.0,A=1.0)", or comma-separated "1.0,0.5,0.0,1.0".
	 * 
	 * @param ColorString The string to parse
	 * @param OutColor Output color
	 * @return True if successfully parsed color
	 */
	static bool TryParseLinearColor(const FString& ColorString, FLinearColor& OutColor);
	
	/**
	 * Try to get a linear color from a field in a JSON object.
	 * Convenience wrapper around TryGetLinearColor that handles field extraction.
	 * Supports all color formats: arrays, objects, hex strings, and named colors.
	 * 
	 * @param Object The JSON object containing the field
	 * @param FieldName The name of the field to extract
	 * @param OutColor Output color
	 * @return True if field exists and was successfully parsed as a color
	 */
	static bool TryGetLinearColorField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, FLinearColor& OutColor);
	
	/**
	 * Try to get a boolean from a JSON value.
	 * Handles: true, false, "true", "false", 1, 0, "1", "0", "yes", "no"
	 * 
	 * @param Value The JSON value
	 * @param OutBool Output boolean
	 * @return True if successfully extracted boolean
	 */
	static bool TryGetBool(const TSharedPtr<FJsonValue>& Value, bool& OutBool);
	
	/**
	 * Try to get a number from a JSON value.
	 * Handles: 42, 3.14, "42", "3.14"
	 * 
	 * @param Value The JSON value
	 * @param OutNumber Output number
	 * @return True if successfully extracted number
	 */
	static bool TryGetNumber(const TSharedPtr<FJsonValue>& Value, double& OutNumber);
	
	/**
	 * Create an array JSON value from a number array.
	 */
	static TSharedPtr<FJsonValue> MakeArrayValue(const TArray<double>& Numbers);
	
	/**
	 * Create an array JSON value from a Vector2D.
	 */
	static TSharedPtr<FJsonValue> MakeArrayValue(const FVector2D& Vector);
	
	/**
	 * Create an array JSON value from a Vector.
	 */
	static TSharedPtr<FJsonValue> MakeArrayValue(const FVector& Vector);
	
	// ═══════════════════════════════════════════════════════════════════
	// Unreal Property Format String Conversion
	// These convert to the format Unreal expects for property parsing
	// ═══════════════════════════════════════════════════════════════════
	
	/**
	 * Convert FColor to Unreal property string format.
	 * @return String like "(R=255,G=128,B=0,A=255)"
	 */
	static FString FColorToPropertyString(const FColor& Color);
	
	/**
	 * Convert FLinearColor to Unreal FColor property string format.
	 * Values are scaled from 0.0-1.0 to 0-255.
	 * @return String like "(R=255,G=128,B=0,A=255)"
	 */
	static FString LinearColorToFColorPropertyString(const FLinearColor& Color);
	
	/**
	 * Convert FLinearColor to Unreal FLinearColor property string format.
	 * @return String like "(R=1.0,G=0.5,B=0.0,A=1.0)"
	 */
	static FString LinearColorToPropertyString(const FLinearColor& Color);
	
	/**
	 * Convert FVector to Unreal property string format.
	 * @return String like "(X=100.0,Y=200.0,Z=300.0)"
	 */
	static FString VectorToPropertyString(const FVector& Vector);
	
	/**
	 * Convert FRotator to Unreal property string format.
	 * @return String like "(Pitch=0.0,Yaw=90.0,Roll=0.0)"
	 */
	static FString RotatorToPropertyString(const FRotator& Rotator);
	
	/**
	 * Try to parse a JSON value and convert to appropriate Unreal property string.
	 * Handles colors, vectors, rotators, and simple values.
	 * @param Value The JSON value (from LLM input)
	 * @param OutPropertyString Output string in Unreal property format
	 * @return True if successfully converted
	 */
	static bool TryConvertToPropertyString(const TSharedPtr<FJsonValue>& Value, FString& OutPropertyString);

private:
	/** Check if a string looks like it might be a JSON value */
	static bool LooksLikeJson(const FString& Str);
	
	/** Try to parse a hex color string */
	static bool TryParseHexColor(const FString& HexStr, FLinearColor& OutColor);
	
	/** Try to parse a named color */
	static bool TryParseNamedColor(const FString& ColorName, FLinearColor& OutColor);
};
