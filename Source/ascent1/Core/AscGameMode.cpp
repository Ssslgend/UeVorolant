#include "AscGameMode.h"
#include "AscGameState.h"
#include "AscPlayerController.h"
#include "../Map/PrefireArena.h"
#include "../Player/AscCharacter.h"
#include "../Training/TrainingHubBuilder.h"
#include "../UI/TrainingHUD.h"
#include "Kismet/GameplayStatics.h"

AAscGameMode::AAscGameMode()
{
	DefaultPawnClass = AAscCharacter::StaticClass();
	PlayerControllerClass = AAscPlayerController::StaticClass();
	GameStateClass = AAscGameState::StaticClass();
	HUDClass = ATrainingHUD::StaticClass();
	SpotDataFilePath = TEXT("prefire_spots.json");
}

void AAscGameMode::BeginPlay()
{
	Super::BeginPlay();

	Arena = Cast<APrefireArena>(UGameplayStatics::GetActorOfClass(GetWorld(), APrefireArena::StaticClass()));
	if (!Arena)
	{
		Arena = GetWorld()->SpawnActor<APrefireArena>();
	}

	if (!UGameplayStatics::GetActorOfClass(GetWorld(), ATrainingHubBuilder::StaticClass()))
	{
		GetWorld()->SpawnActor<ATrainingHubBuilder>();
	}
}

void AAscGameMode::StartTraining()
{
	CurrentSpotIndex = 0;
	if (Arena)
	{
		Arena->LoadAndBuild(SpotDataFilePath);
	}
}

void AAscGameMode::NextSpot()
{
	CurrentSpotIndex++;
	if (Arena)
	{
		Arena->NextSpot();
	}
}

void AAscGameMode::RestartCurrentSpot()
{
	if (Arena)
	{
		Arena->RestartSpot();
	}
}
