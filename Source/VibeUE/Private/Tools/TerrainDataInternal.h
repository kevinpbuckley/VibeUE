// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Math/Vector2D.h"
#include "Dom/JsonObject.h"

struct FTerrainHttpResult
{
	bool bSuccess = false;
	bool bNoApiKey = false;
	int32 ResponseCode = 0;
	TArray<uint8> Content;
	TMap<FString, FString> Headers;
	FString ContentType;
	FString ErrorMessage;
	bool HasContentType(const FString& Expected) const;
};

namespace VibeUE::Terrain
{
	using FParams = TMap<FString, FString>;
	using FBody = TSharedRef<FJsonObject>;

	FString Param(const FParams& Params, const FString& Name, const FString& Default = FString());
	double Number(const FParams& Params, const FString& Name, double Default);
	int32 Integer(const FParams& Params, const FString& Name, int32 Default);
	bool Boolean(const FParams& Params, const FString& Name, bool Default);
	FString Serialize(const FBody& Data);
	FString Error(const FString& Code, const FString& Message);
	FString SavePath(const FString& RequestedPath, const FString& Filename);
	bool SaveBytes(TArrayView64<const uint8> Bytes, const FString& FilePath);
	bool SaveText(const FString& Text, const FString& FilePath);
	FTerrainHttpResult TerrainHttpPost(const FString& Path, const FString& JsonBody, float TimeoutSeconds = 30.0f,
		float TransportGraceSeconds = 2.0f);

	FBody LocationBody(const FParams& Params);
	FBody PreviewBody(const FParams& Params);
	FBody WaterBody(const FParams& Params);
	FBody HeightBody(const FParams& Params, bool bVista);
	TSharedPtr<FJsonObject> MapBody(const FParams& Params);
	FBody MaskBody(const FParams& Params, const FString& Layer);
	FBody OrthoBody(const FParams& Params, const FString& Source, const FString& Band);
	FBody TileBody(const FJsonObject& Template, const FString& Band, int32 Col, int32 Row);
	TArray<FString> MaskLayers(const FParams& Params);
	TArray<FString> OrthoBands(const FParams& Params);

	struct FPlacement
	{
		FVector2D Vertex;
		FVector2D Centimetres;
	};
	FPlacement Project(double Lng, double Lat, double CenterLng, double CenterLat, double SideKm, int32 Samples);
	void AddCoordinates(FJsonObject& Feature, double CenterLng, double CenterLat, double SideKm);
	bool StitchTile(TArrayView64<uint8> Canvas, int32 Side, TArrayView64<const uint8> Tile, int32 TileSide,
		int32 BytesPerPixel, int32 Col, int32 Row);

	FString GetMapFeatures(const FParams& Params);
	FString GetFeatureMasks(const FParams& Params);
	FString GetOrthoImage(const FParams& Params);
	FString GenerateVista(const FParams& Params);
#if WITH_AUTOMATION_TESTS
	FString RunWithoutKey(const FParams& Params);
#endif
}
