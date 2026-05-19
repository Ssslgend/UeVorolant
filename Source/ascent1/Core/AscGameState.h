#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include <limits>
#include "AscGameState.generated.h"

UCLASS()
class ASCENT1_API AAscGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Training")
	int32 TotalSpots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Training")
	int32 CompletedSpots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Training")
	int32 TotalHits;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Training")
	int32 TotalShots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Training")
	int32 Headshots;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Training")
	float BestReactionTimeMs = std::numeric_limits<float>::infinity();

	UFUNCTION(BlueprintCallable, Category = "Training")
	void RecordShot(bool bHit, bool bHeadshot, float ReactionTimeMs);

	UFUNCTION(BlueprintCallable, Category = "Training")
	float GetHeadshotRate() const;

	UFUNCTION(BlueprintCallable, Category = "Training")
	float GetHitRate() const;
};
