#include "AscGameMode.h"
#include "AscGameState.h"
#include "AscPlayerController.h"
#include "../Map/PrefireArena.h"
#include "../Player/AscCharacter.h"
#include "../Training/TrainingHubBuilder.h"
#include "../UI/TrainingHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Training/TrainerSession.h"
#include "Training/TrainerRuntimeVerifier.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AAscGameMode::AAscGameMode()
{
	DefaultPawnClass = AAscCharacter::StaticClass();
	PlayerControllerClass = AAscPlayerController::StaticClass();
	GameStateClass = AAscGameState::StaticClass();
	HUDClass = ATrainingHUD::StaticClass();
	SpotDataFilePath = TEXT("trainer_spots_v1.json");
}

void AAscGameMode::BeginPlay()
{
	Super::BeginPlay();

    GetWorld()->SpawnActor<ATrainerSession>();
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("TrainerVerify")))GetWorld()->SpawnActor<ATrainerRuntimeVerifier>();
#endif
}

void AAscGameMode::StartTraining()
{
    if(auto PC=Cast<AAscPlayerController>(UGameplayStatics::GetPlayerController(this,0)))PC->StartPractice(1,TEXT("All"),1,0);
}

void AAscGameMode::NextSpot()
{
    if(auto PC=Cast<AAscPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
    if(auto Session=PC->GetSession())
    {
        int32 Count=0;for(const auto& P:Session->GetPoints())if(Session->SelectedRegion==TEXT("All") || P.Region==Session->SelectedRegion)++Count;
        CurrentSpotIndex=Count?(Session->SelectedPoint+1)%Count:0;
        PC->StartPractice(static_cast<int32>(Session->Mode),Session->SelectedRegion,Session->Difficulty,CurrentSpotIndex);
    }
}

void AAscGameMode::RestartCurrentSpot()
{
    if(auto PC=Cast<AAscPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
    if(auto Session=PC->GetSession())PC->StartPractice(static_cast<int32>(Session->Mode),Session->SelectedRegion,Session->Difficulty,Session->SelectedPoint);
}

int32 AAscGameMode::GetCurrentSpotIndex() const
{
    if(auto Session=Cast<ATrainerSession>(UGameplayStatics::GetActorOfClass(this,ATrainerSession::StaticClass())))return Session->SelectedPoint;
    return CurrentSpotIndex;
}
