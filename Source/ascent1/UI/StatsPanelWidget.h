#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Training/TrainingStats.h"
#include "StatsPanelWidget.generated.h"

UCLASS()
class ASCENT1_API UStatsPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Refresh(const FTrainingSummary& Stats);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ToggleVisibility();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float HeadshotRate;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float HitRate;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float AverageReactionTimeMs;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float BestReactionTimeMs;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalHits;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalShots;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Headshots;
};
