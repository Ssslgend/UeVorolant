#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpotData.h"
#include "PrefireArena.generated.h"

class AAscCharacter;
class AEnemyTarget;
class AEnemySpawner;

UCLASS()
class ASCENT1_API APrefireArena : public AActor
{
	GENERATED_BODY()

public:
	APrefireArena();

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void LoadAndBuild(const FString& JsonPath);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void StartSpot(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void BotFire();

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void OnPlayerHitBot(AAscCharacter* Player, const FVector& HitPoint);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void NextSpot();

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void RestartSpot();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpotCompleted, int32, SpotIndex);
	UPROPERTY(BlueprintAssignable)
	FOnSpotCompleted OnSpotCompleted;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerKilled, float, ReactionTimeMs);
	UPROPERTY(BlueprintAssignable)
	FOnPlayerKilled OnPlayerKilled;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Bot")
	float BotFireDelay = 0.25f;

protected:
	virtual void BeginPlay() override;

	void SpawnCoverWall(const FVector& PlayerPos, const FVector& EnemyPos);
	void TeleportPlayer(const FVector& Location, const FRotator& Rotation);
	void ClearCurrentSpot();

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Cover")
	TSubclassOf<AActor> CoverWallClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Cover")
	float CoverDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Cover")
	float CoverHeight = 200.0f;

	TArray<FPrefireSpot> Spots;
	int32 CurrentSpotIndex;

	UPROPERTY()
	AEnemyTarget* ActiveBot;

	UPROPERTY()
	AActor* ActiveCoverWall;

	FTimerHandle BotFireTimerHandle;
	float SpotStartTime;
};
