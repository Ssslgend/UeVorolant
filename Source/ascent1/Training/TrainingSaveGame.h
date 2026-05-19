#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TrainingSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FTrainingRecord
{
	GENERATED_BODY()

	UPROPERTY() FString Date;
	UPROPERTY() FString MapName;
	UPROPERTY() FString ModeName;
	UPROPERTY() float HeadshotRate;
	UPROPERTY() float HitRate;
	UPROPERTY() float AverageReactionTimeMs;
	UPROPERTY() float BestReactionTimeMs;
	UPROPERTY() int32 TotalShots;
	UPROPERTY() int32 TotalHits;
};

UCLASS()
class ASCENT1_API UTrainingSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FTrainingRecord> Records;

	UPROPERTY()
	float Sensitivity = 1.0f;

	UPROPERTY()
	int32 TotalSessionsCompleted;

	void AddRecord(const FTrainingRecord& Record);
	static UTrainingSaveGame* Load();
	void Save();
};
