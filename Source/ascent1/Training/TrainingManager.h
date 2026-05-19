#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainingManager.generated.h"

class APrefireArena;
class AAscCharacter;

UENUM(BlueprintType)
enum class ETrainingMode : uint8
{
	HeadlinePractice	UMETA(DisplayName = "Headline Practice"),
	PrefirePractice		UMETA(DisplayName = "Prefire Practice"),
	ReactionTest		UMETA(DisplayName = "Reaction Test")
};

UENUM(BlueprintType)
enum class EDifficulty : uint8
{
	Easy	UMETA(DisplayName = "Easy (500ms)"),
	Normal	UMETA(DisplayName = "Normal (250ms)"),
	Hard	UMETA(DisplayName = "Hard (150ms)")
};

UCLASS()
class ASCENT1_API ATrainingManager : public AActor
{
	GENERATED_BODY()

public:
	ATrainingManager();

	UFUNCTION(BlueprintCallable, Category = "Training")
	void StartTraining(ETrainingMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Training")
	void StopTraining();

	UFUNCTION(BlueprintCallable, Category = "Training")
	void SetDifficulty(EDifficulty NewDifficulty);

	UFUNCTION(BlueprintCallable, Category = "Training")
	EDifficulty GetDifficulty() const { return CurrentDifficulty; }

	UFUNCTION(BlueprintCallable, Category = "Training")
	ETrainingMode GetMode() const { return CurrentMode; }

protected:
	virtual void BeginPlay() override;

	void OnSpotCompleted(int32 SpotIndex);
	void OnPlayerKilled(float ReactionTimeMs);

	UPROPERTY()
	APrefireArena* Arena;

	UPROPERTY()
	AAscCharacter* PlayerCharacter;

	ETrainingMode CurrentMode;
	EDifficulty CurrentDifficulty;
};
