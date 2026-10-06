#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AscGameMode.generated.h"

class USpotLoader;
class APrefireArena;

UCLASS()
class ASCENT1_API AAscGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAscGameMode();

	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training")
	FString SpotDataFilePath;

	UFUNCTION(BlueprintCallable, Category = "Training")
	void StartTraining();

	UFUNCTION(BlueprintCallable, Category = "Training")
	void NextSpot();

	UFUNCTION(BlueprintCallable, Category = "Training")
	void RestartCurrentSpot();

	int32 GetCurrentSpotIndex() const;

protected:
	UPROPERTY()
	APrefireArena* Arena;

	int32 CurrentSpotIndex;
};
