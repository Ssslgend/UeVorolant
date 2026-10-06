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
	UPROPERTY() float HeadshotRate = 0.0f;
	UPROPERTY() float HitRate = 0.0f;
	UPROPERTY() float AverageReactionTimeMs = 0.0f;
	UPROPERTY() float BestReactionTimeMs = 0.0f;
	UPROPERTY() int32 TotalShots = 0;
	UPROPERTY() int32 TotalHits = 0;
    UPROPERTY() int32 Headshots = 0;
    UPROPERTY() bool bWon = false;
    UPROPERTY() float StoppedShotRate = 0;
    UPROPERTY() float AverageAimErrorDegrees = 0;
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
	int32 TotalSessionsCompleted = 0;

	void AddRecord(const FTrainingRecord& Record);
	static UTrainingSaveGame* Load();
	void Save();
};
