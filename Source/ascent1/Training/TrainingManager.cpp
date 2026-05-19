#include "TrainingManager.h"
#include "../Map/PrefireArena.h"
#include "../Player/AscCharacter.h"
#include "Kismet/GameplayStatics.h"

ATrainingManager::ATrainingManager()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentMode = ETrainingMode::PrefirePractice;
	CurrentDifficulty = EDifficulty::Normal;
}

void ATrainingManager::BeginPlay()
{
	Super::BeginPlay();

	Arena = Cast<APrefireArena>(UGameplayStatics::GetActorOfClass(GetWorld(), APrefireArena::StaticClass()));
	if (Arena)
	{
		Arena->OnSpotCompleted.AddDynamic(this, &ATrainingManager::OnSpotCompleted);
		Arena->OnPlayerKilled.AddDynamic(this, &ATrainingManager::OnPlayerKilled);
	}

	PlayerCharacter = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
}

void ATrainingManager::StartTraining(ETrainingMode Mode)
{
	CurrentMode = Mode;

	if (!Arena)
	{
		UE_LOG(LogTemp, Error, TEXT("TrainingManager: No PrefireArena found in level"));
		return;
	}

	switch (CurrentDifficulty)
	{
	case EDifficulty::Easy:   Arena->BotFireDelay = 0.5f;  break;
	case EDifficulty::Normal: Arena->BotFireDelay = 0.25f; break;
	case EDifficulty::Hard:   Arena->BotFireDelay = 0.15f; break;
	}

	Arena->LoadAndBuild(TEXT("prefire_spots.json"));
}

void ATrainingManager::StopTraining()
{
	// Cleanup delegated to Arena
}

void ATrainingManager::SetDifficulty(EDifficulty NewDifficulty)
{
	CurrentDifficulty = NewDifficulty;
}

void ATrainingManager::OnSpotCompleted(int32 SpotIndex)
{
	UE_LOG(LogTemp, Log, TEXT("Training: Spot %d completed!"), SpotIndex);
}

void ATrainingManager::OnPlayerKilled(float ReactionTimeMs)
{
	UE_LOG(LogTemp, Log, TEXT("Training: Player killed! Reaction: %.1fms"), ReactionTimeMs);
}
