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

	// Bot delay now lives in Content/Data/trainer_settings.json (bot.delayMs); the
	// difficulty enum only picks the spot list preset for the legacy manager path.
	UE_LOG(LogTemp, Log, TEXT("TrainingManager: starting mode %d difficulty %d"),
		static_cast<int32>(CurrentMode), static_cast<int32>(CurrentDifficulty));

	Arena->LoadAndBuild(TEXT("trainer_spots_v1.json"));
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
