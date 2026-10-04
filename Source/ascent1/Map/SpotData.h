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
};

USTRUCT(BlueprintType)
struct ASCENT1_API FPrefireSpot
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString MapName;

	UPROPERTY()
	FString MapUuid;

	UPROPERTY()
	FGamePosition Player;

	UPROPERTY()
	FGamePosition Enemy;

	UPROPERTY()
	float DistanceUnits = 0.0f;

	FVector GetPlayerUEPos() const;
	FVector GetEnemyUEPos() const;
};
