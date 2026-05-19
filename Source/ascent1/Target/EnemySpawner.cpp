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

	if (!BotClass) return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ActiveBot = GetWorld()->SpawnActor<AEnemyTarget>(BotClass, Location, Rotation, Params);
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
