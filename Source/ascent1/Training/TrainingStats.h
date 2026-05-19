#pragma once

#include "CoreMinimal.h"
#include <limits>
#include "TrainingStats.generated.h"

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainingSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float HeadshotRate;

	UPROPERTY(BlueprintReadOnly)
	float HitRate;

	UPROPERTY(BlueprintReadOnly)
	float AverageReactionTimeMs;

	UPROPERTY(BlueprintReadOnly)
	float BestReactionTimeMs;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalHits;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalShots;

	UPROPERTY(BlueprintReadOnly)
	int32 Headshots;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainingSession
{
	GENERATED_BODY()

	TSet<float> ReactionTimes;
	int32 TotalHits = 0;
	int32 TotalShots = 0;
	int32 Headshots = 0;
	float BestReactionTimeMs = std::numeric_limits<float>::infinity();

	void RecordShot(bool bHit, bool bHeadshot, float ReactionTimeMs);
	FTrainingSummary GetSummary() const;
	void Reset();
};
