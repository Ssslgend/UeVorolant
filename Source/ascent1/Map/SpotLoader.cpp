#include "SpotLoader.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	double ReadNumberField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double Default = 0.0)
	{
		if (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field))
		{
			return Obj->GetNumberField(Field);
		}
		return Default;
	}

	FValVec3D ReadVec3(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		FValVec3D Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr) && Arr && Arr->Num() >= 3)
		{
			Out.X = (*Arr)[0]->AsNumber();
			Out.Y = (*Arr)[1]->AsNumber();
			Out.Z = (*Arr)[2]->AsNumber();
		}
		return Out;
	}

	FGamePosition ReadPositionV1(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Key)
	{
		FGamePosition Pos;
		const TSharedPtr<FJsonObject>* PosObj = nullptr;
		if (!Parent.IsValid() || !Parent->TryGetObjectField(Key, PosObj) || !PosObj->IsValid())
		{
			return Pos;
		}

		Pos.GamePosition = ReadVec3(*PosObj, TEXT("pos"));
		Pos.Yaw = static_cast<float>(ReadNumberField(*PosObj, TEXT("yaw"), 0.0));

		// optional uv for editor round-trip
		const TArray<TSharedPtr<FJsonValue>>* Uv = nullptr;
		if ((*PosObj)->TryGetArrayField(TEXT("uv"), Uv) && Uv && Uv->Num() >= 2)
		{
			Pos.MinimapNormalized.X = (*Uv)[0]->AsNumber();
			Pos.MinimapNormalized.Y = (*Uv)[1]->AsNumber();
		}
		return Pos;
	}
}

FSpotLoadResult USpotLoader::LoadSpotsFromFile(const FString& FilePath)
{
	FSpotLoadResult Result;

	FString FullPath = FilePath;
	if (FPaths::IsRelative(FilePath))
	{
		FullPath = FPaths::ProjectContentDir() / TEXT("Data") / FilePath;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
	{
		Result.Warnings.Add(FString::Printf(TEXT("SpotLoader: failed to load %s"), *FullPath));
		UE_LOG(LogTemp, Error, TEXT("SpotLoader: Failed to load %s"), *FullPath);
		return Result;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Result.Warnings.Add(FString::Printf(TEXT("SpotLoader: failed to parse %s"), *FullPath));
		UE_LOG(LogTemp, Error, TEXT("SpotLoader: Failed to parse JSON from %s"), *FullPath);
		return Result;
	}

	const int32 Version = static_cast<int32>(ReadNumberField(RootObject, TEXT("version"), 0.0));
	Result.Version = Version;

	// Schema v1: { version: 1, points: [...] }
	const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
	if (Version >= 1 && RootObject->TryGetArrayField(TEXT("points"), PointsArray))
	{
		for (const auto& PointValue : *PointsArray)
		{
			const TSharedPtr<FJsonObject> PointObj = PointValue->AsObject();
			if (!PointObj.IsValid())
			{
				continue;
			}
			FPrefireSpot Spot = ParseSpotV1(PointObj);
			if (Spot.Id.IsEmpty() && Spot.Name.IsEmpty())
			{
				Result.Warnings.Add(TEXT("SpotLoader: skipped point with empty id/name"));
				continue;
			}
			Result.Spots.Add(Spot);
		}
	}
	// Legacy: { spots: [...] }
	else if (RootObject->TryGetArrayField(TEXT("spots"), PointsArray))
	{
		for (const auto& SpotValue : *PointsArray)
		{
			const TSharedPtr<FJsonObject> SpotObj = SpotValue->AsObject();
			if (SpotObj.IsValid())
			{
				Result.Spots.Add(ParseSpot(SpotObj));
			}
		}
	}
	else
	{
		Result.Warnings.Add(TEXT("SpotLoader: JSON has neither 'points' (v1) nor 'spots' (legacy)"));
	}

	UE_LOG(LogTemp, Log, TEXT("SpotLoader: Loaded %d spots (version %d) from %s"),
		Result.Spots.Num(), Version, *FullPath);
	return Result;
}

FPrefireSpot USpotLoader::ParseSpotV1(const TSharedPtr<FJsonObject>& SpotObject)
{
	FPrefireSpot Spot;

	Spot.Id = SpotObject->HasField(TEXT("id")) ? SpotObject->GetStringField(TEXT("id")) : TEXT("");
	Spot.Name = SpotObject->HasField(TEXT("name")) ? SpotObject->GetStringField(TEXT("name")) : TEXT("Unnamed");
	Spot.Type = SpotObject->HasField(TEXT("type")) ? SpotObject->GetStringField(TEXT("type")) : TEXT("prefire");
	Spot.Notes = SpotObject->HasField(TEXT("notes")) ? SpotObject->GetStringField(TEXT("notes")) : TEXT("");
	Spot.Difficulty = static_cast<int32>(ReadNumberField(SpotObject, TEXT("difficulty"), 1.0));

	Spot.Player = ReadPositionV1(SpotObject, TEXT("player"));
	Spot.Enemy = ReadPositionV1(SpotObject, TEXT("enemy"));

	const TSharedPtr<FJsonObject>* BotObj = nullptr;
	if (SpotObject->TryGetObjectField(TEXT("bot"), BotObj) && BotObj->IsValid())
	{
		Spot.Bot.DelayMs = static_cast<float>(ReadNumberField(*BotObj, TEXT("delayMs"), 250.0));
		if ((*BotObj)->HasField(TEXT("fireMode")))
		{
			Spot.Bot.FireMode = (*BotObj)->GetStringField(TEXT("fireMode"));
		}
		if ((*BotObj)->HasTypedField<EJson::Boolean>(TEXT("losTrigger")))
		{
			Spot.Bot.bLosTrigger = (*BotObj)->GetBoolField(TEXT("losTrigger"));
		}
		if ((*BotObj)->HasField(TEXT("stance")))
		{
			Spot.Bot.Stance = (*BotObj)->GetStringField(TEXT("stance"));
		}
	}

	// map block may carry display name
	const TSharedPtr<FJsonObject>* MapObj = nullptr;
	if (SpotObject->TryGetObjectField(TEXT("map"), MapObj) && MapObj->IsValid())
	{
		if ((*MapObj)->HasField(TEXT("id")))
		{
			Spot.MapUuid = (*MapObj)->GetStringField(TEXT("id"));
		}
		if ((*MapObj)->HasField(TEXT("displayName")))
		{
			Spot.MapName = (*MapObj)->GetStringField(TEXT("displayName"));
		}
	}

	// Prefer top-level map fields when present (flat writer)
	if (SpotObject->HasField(TEXT("map")))
	{
		const FString MapField = SpotObject->GetStringField(TEXT("map"));
		if (!MapField.IsEmpty() && MapField.StartsWith(TEXT("{")) == false && Spot.MapName.IsEmpty())
		{
			Spot.MapName = MapField;
		}
	}

	return Spot;
}

FPrefireSpot USpotLoader::ParseSpot(const TSharedPtr<FJsonObject>& SpotObject)
{
	FPrefireSpot Spot;

	Spot.Name = SpotObject->HasField(TEXT("name")) ? SpotObject->GetStringField(TEXT("name")) : TEXT("Unnamed");
	Spot.MapName = SpotObject->HasField(TEXT("map")) ? SpotObject->GetStringField(TEXT("map")) : TEXT("");
	Spot.MapUuid = SpotObject->HasField(TEXT("mapUuid")) ? SpotObject->GetStringField(TEXT("mapUuid")) : TEXT("");
	Spot.DistanceUnits = static_cast<float>(ReadNumberField(SpotObject, TEXT("distanceUnits"), 0.0));
	Spot.Id = Spot.Name;

	auto ParsePosition = [](const TSharedPtr<FJsonObject>& Obj, const FString& Key) -> FGamePosition
	{
		FGamePosition Pos;
		const TSharedPtr<FJsonObject>* PosObj;
		if (Obj->TryGetObjectField(Key, PosObj))
		{
			const TSharedPtr<FJsonObject>* GamePos;
			if ((*PosObj)->TryGetObjectField(TEXT("gamePosition"), GamePos))
			{
				Pos.GamePosition.X = ReadNumberField(*GamePos, TEXT("x"));
				Pos.GamePosition.Y = ReadNumberField(*GamePos, TEXT("y"));
				Pos.GamePosition.Z = ReadNumberField(*GamePos, TEXT("z"));
			}
			const TSharedPtr<FJsonObject>* NormPos;
			if ((*PosObj)->TryGetObjectField(TEXT("minimapNormalized"), NormPos))
			{
				Pos.MinimapNormalized.X = ReadNumberField(*NormPos, TEXT("x"));
				Pos.MinimapNormalized.Y = ReadNumberField(*NormPos, TEXT("y"));
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
		static_cast<float>(ValPos.Z) + 91.5f);
}

bool USpotLoader::ResolveGroundedPosition(UWorld* World, const FVector& InPos, FVector& OutPos)
{
	// Returns a FLOOR point near the authored Z. Callers add capsule half height.
	OutPos = InPos;
	if (!World)
	{
		return false;
	}

	// Probe just above the authored floor, not from the sky: a long drop hits
	// rooftops/awnings first and spawns the player on top of the building shell.
	const float AuthoredZ = InPos.Z;
	const float StartZ = AuthoredZ + 250.0f;
	const float EndZ = AuthoredZ - 800.0f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpotGroundSnap), false);
	const bool bHit = World->LineTraceSingleByChannel(
		Hit, FVector(InPos.X, InPos.Y, StartZ), FVector(InPos.X, InPos.Y, EndZ), ECC_Visibility, Params);

	if (bHit)
	{
		// Only accept a surface close to the authored height.
		if (FMath::Abs(Hit.ImpactPoint.Z - AuthoredZ) <= 400.0f)
		{
			OutPos = FVector(InPos.X, InPos.Y, Hit.ImpactPoint.Z);
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("SpotLoader: ground snap %.0f too far from authored %.0f at (%.0f,%.0f)"),
			Hit.ImpactPoint.Z, AuthoredZ, InPos.X, InPos.Y);
	}

	// Fall back to the authored height (from the playable map report).
	return AuthoredZ != 0.0f;
}
