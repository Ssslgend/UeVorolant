#pragma once

#include "CoreMinimal.h"
#include "SpotData.h"
#include "SpotLoader.generated.h"

USTRUCT(BlueprintType)
struct ASCENT1_API FSpotLoadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SpotLoader")
	TArray<FPrefireSpot> Spots;

	UPROPERTY(BlueprintReadOnly, Category = "SpotLoader")
	TArray<FString> Warnings;

	UPROPERTY(BlueprintReadOnly, Category = "SpotLoader")
	int32 Version = 0;
};

UCLASS(BlueprintType)
class ASCENT1_API USpotLoader : public UObject
{
	GENERATED_BODY()

public:
	/** Loads Content/Data/<FilePath>. Supports schema version 1 (trainer) and legacy "spots". */
	UFUNCTION(BlueprintCallable, Category = "SpotLoader")
	static FSpotLoadResult LoadSpotsFromFile(const FString& FilePath);

	/** Parse a single point object (schema v1). */
	static FPrefireSpot ParseSpotV1(const TSharedPtr<FJsonObject>& SpotObject);

	/** Legacy prefire_spots.json entry. */
	static FPrefireSpot ParseSpot(const TSharedPtr<FJsonObject>& SpotObject);

	static FVector ValToUE(const FValVec3D& ValPos);

	/**
	 * Z snap: if stored Z is 0, raycast down from Z+500 and land on floor.
	 * Returns false (and leaves OutPos unchanged) when the ray misses.
	 */
	static bool ResolveGroundedPosition(UWorld* World, const FVector& InPos, FVector& OutPos);
};
