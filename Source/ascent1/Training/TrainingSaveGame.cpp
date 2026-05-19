#include "TrainingSaveGame.h"
#include "Kismet/GameplayStatics.h"

void UTrainingSaveGame::AddRecord(const FTrainingRecord& Record)
{
	Records.Add(Record);
	TotalSessionsCompleted++;
	if (Records.Num() > 100)
	{
		Records.RemoveAt(0);
	}
}

UTrainingSaveGame* UTrainingSaveGame::Load()
{
	UTrainingSaveGame* Save = Cast<UTrainingSaveGame>(
		UGameplayStatics::LoadGameFromSlot(TEXT("TrainingData"), 0));
	if (!Save)
	{
		Save = Cast<UTrainingSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UTrainingSaveGame::StaticClass()));
	}
	return Save;
}

void UTrainingSaveGame::Save()
{
	UGameplayStatics::SaveGameToSlot(this, TEXT("TrainingData"), 0);
}
