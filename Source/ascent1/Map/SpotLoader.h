#pragma once

#include "CoreMinimal.h"
#include "SpotData.h"
#include "SpotLoader.generated.h"

UCLASS(BlueprintType)
class ASCENT1_API USpotLoader : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SpotLoader")
	static TArray<FPrefireSpot> LoadSpotsFromFile(const FString& FilePath);

	static FPrefireSpot ParseSpot(const TSharedPtr<FJsonObject>& SpotObject);
	static FVector ValToUE(const FValVec3D& ValPos);
};
