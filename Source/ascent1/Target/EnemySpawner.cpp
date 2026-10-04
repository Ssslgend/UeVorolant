#include "EnemySpawner.h"
#include "EnemyTarget.h"
#include "Engine/World.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

AEnemyTarget* AEnemySpawner::SpawnBot(const FVector& Location, const FRotator& Rotation)
{
	DespawnAll();

	TSubclassOf<AEnemyTarget> SpawnClass = BotClass;
	if (!SpawnClass)
	{
		SpawnClass = AEnemyTarget::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ActiveBot = GetWorld()->SpawnActor<AEnemyTarget>(SpawnClass, Location, Rotation, Params);
	return ActiveBot;
}

void AEnemySpawner::DespawnAll()
{
	if (ActiveBot)
	{
		ActiveBot->Destroy();
		ActiveBot = nullptr;
	}
}
