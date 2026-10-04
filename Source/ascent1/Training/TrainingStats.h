#pragma once

#include "CoreMinimal.h"
#include <limits>
#include "TrainingStats.generated.h"

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainingSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float HeadshotRate = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float HitRate = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float AverageReactionTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float BestReactionTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalHits = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalShots = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Headshots = 0;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainingSession
{
	GENERATED_BODY()

	TArray<float> ReactionTimes;
	int32 TotalHits = 0;
	int32 TotalShots = 0;
	int32 Headshots = 0;
	float BestReactionTimeMs = std::numeric_limits<float>::infinity();

	void RecordShot(bool bHit, bool bHeadshot, float ReactionTimeMs);
	FTrainingSummary GetSummary() const;
	void Reset();
};
