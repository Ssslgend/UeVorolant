#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class AEnemyTarget;

UCLASS()
class ASCENT1_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	AEnemyTarget* SpawnBot(const FVector& Location, const FRotator& Rotation);

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void DespawnAll();

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	AEnemyTarget* GetActiveBot() const { return ActiveBot; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<AEnemyTarget> BotClass;

	UPROPERTY()
	AEnemyTarget* ActiveBot;
};
