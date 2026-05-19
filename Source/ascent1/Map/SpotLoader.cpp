#include "SpotLoader.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TArray<FPrefireSpot> USpotLoader::LoadSpotsFromFile(const FString& FilePath)
{
	TArray<FPrefireSpot> Spots;

	FString FullPath = FPaths::ProjectContentDir() / TEXT("Data") / FilePath;
	FString JsonString;

	if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("SpotLoader: Failed to load %s"), *FullPath);
		return Spots;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("SpotLoader: Failed to parse JSON from %s"), *FullPath);
		return Spots;
	}

	const TArray<TSharedPtr<FJsonValue>>* SpotsArray;
	if (RootObject->TryGetArrayField(TEXT("spots"), SpotsArray))
	{
		for (const auto& SpotValue : *SpotsArray)
		{
			TSharedPtr<FJsonObject> SpotObj = SpotValue->AsObject();
			if (SpotObj.IsValid())
			{
				Spots.Add(ParseSpot(SpotObj));
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("SpotLoader: Loaded %d spots from %s"), Spots.Num(), *FilePath);
	return Spots;
}

FPrefireSpot USpotLoader::ParseSpot(const TSharedPtr<FJsonObject>& SpotObject)
{
	FPrefireSpot Spot;

	Spot.Name = SpotObject->HasField(TEXT("name")) ? SpotObject->GetStringField(TEXT("name")) : TEXT("Unnamed");
	Spot.MapName = SpotObject->HasField(TEXT("map")) ? SpotObject->GetStringField(TEXT("map")) : TEXT("");
	Spot.MapUuid = SpotObject->HasField(TEXT("mapUuid")) ? SpotObject->GetStringField(TEXT("mapUuid")) : TEXT("");
	Spot.DistanceUnits = SpotObject->HasTypedField<EJson::Number>(TEXT("distanceUnits"))
		? SpotObject->GetNumberField(TEXT("distanceUnits")) : 0.0f;

	auto ParsePosition = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key) -> FGamePosition
	{
		FGamePosition Pos;
		const TSharedPtr<FJsonObject>* PosObj;
		if (Obj->TryGetObjectField(Key, PosObj))
		{
			const TSharedPtr<FJsonObject>* GamePos;
			if ((*PosObj)->TryGetObjectField(TEXT("gamePosition"), GamePos))
			{
				Pos.GamePosition.X = (*GamePos)->HasTypedField<EJson::Number>(TEXT("x")) ? (*GamePos)->GetNumberField(TEXT("x")) : 0.0;
				Pos.GamePosition.Y = (*GamePos)->HasTypedField<EJson::Number>(TEXT("y")) ? (*GamePos)->GetNumberField(TEXT("y")) : 0.0;
				Pos.GamePosition.Z = (*GamePos)->HasTypedField<EJson::Number>(TEXT("z")) ? (*GamePos)->GetNumberField(TEXT("z")) : 0.0;
			}
			const TSharedPtr<FJsonObject>* NormPos;
			if ((*PosObj)->TryGetObjectField(TEXT("minimapNormalized"), NormPos))
			{
				Pos.MinimapNormalized.X = (*NormPos)->HasTypedField<EJson::Number>(TEXT("x")) ? (*NormPos)->GetNumberField(TEXT("x")) : 0.0;
				Pos.MinimapNormalized.Y = (*NormPos)->HasTypedField<EJson::Number>(TEXT("y")) ? (*NormPos)->GetNumberField(TEXT("y")) : 0.0;
			}
		}
		return Pos;
	};

	Spot.Player = ParsePosition(SpotObject, TEXT("player"));
	Spot.Enemy = ParsePosition(SpotObject, TEXT("enemy"));

	return Spot;
}

FVector USpotLoader::ValToUE(const FValVec3D& ValPos)
{
	return FVector(
		static_cast<float>(ValPos.X),
		static_cast<float>(ValPos.Y),
		static_cast<float>(ValPos.Z) + 91.5f
	);
}
