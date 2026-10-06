#pragma once

#include "CoreMinimal.h"
#include "SpotData.generated.h"

USTRUCT(BlueprintType)
struct ASCENT1_API FValVec3D
{
	GENERATED_BODY()

	UPROPERTY()
	double X;

	UPROPERTY()
	double Y;

	UPROPERTY()
	double Z;

	FValVec3D() : X(0.0), Y(0.0), Z(0.0) {}
	FValVec3D(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
};

USTRUCT(BlueprintType)
struct ASCENT1_API FGamePosition
{
	GENERATED_BODY()

	UPROPERTY()
	FValVec3D GamePosition;

	UPROPERTY()
	FValVec3D MinimapNormalized;

	/** Facing yaw in degrees (UE: 0 = +X). */
	UPROPERTY()
	float Yaw = 0.0f;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerBotSpotConfig
{
	GENERATED_BODY()

	UPROPERTY()
	float DelayMs = 250.0f;

	UPROPERTY()
	FString FireMode = TEXT("instant_kill");

	UPROPERTY()
	bool bLosTrigger = true;

	UPROPERTY()
	FString Stance = TEXT("stand");
};

USTRUCT(BlueprintType)
struct ASCENT1_API FPrefireSpot
{
	GENERATED_BODY()

	UPROPERTY()
	FString Id;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString MapName;

	UPROPERTY()
	FString MapUuid;

	UPROPERTY()
	FString Type = TEXT("prefire");

	UPROPERTY()
	FGamePosition Player;

	UPROPERTY()
	FGamePosition Enemy;

	UPROPERTY()
	FTrainerBotSpotConfig Bot;

	UPROPERTY()
	float DistanceUnits = 0.0f;

	UPROPERTY()
	int32 Difficulty = 1;

	UPROPERTY()
	FString Notes;

	FVector GetPlayerUEPos() const;
	FVector GetEnemyUEPos() const;
};
