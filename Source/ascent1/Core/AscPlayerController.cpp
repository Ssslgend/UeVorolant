#include "AscPlayerController.h"
#include "AscGameState.h"
#include "../Training/TrainingStats.h"
#include "../UI/CrosshairWidget.h"
#include "../UI/StatsPanelWidget.h"
#include "Blueprint/UserWidget.h"

void AAscPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UCrosshairWidget>(this, CrosshairWidgetClass);
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToViewport(0);
		}
	}

	if (StatsPanelWidgetClass)
	{
		StatsPanelWidget = CreateWidget<UStatsPanelWidget>(this, StatsPanelWidgetClass);
		if (StatsPanelWidget)
		{
			StatsPanelWidget->AddToViewport(1);
			StatsPanelWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AAscPlayerController::ShowCrosshair(bool bShow)
{
	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void AAscPlayerController::UpdateCrosshairSpread(float Spread)
{
	if (CrosshairWidget)
	{
		CrosshairWidget->UpdateSpread(Spread);
	}
}

void AAscPlayerController::ShowStatsPanel(bool bShow)
{
	if (StatsPanelWidget)
	{
		StatsPanelWidget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void AAscPlayerController::RefreshStatsPanel()
{
	if (StatsPanelWidget)
	{
		AAscGameState* GS = GetWorld()->GetGameState<AAscGameState>();
		if (GS)
		{
			FTrainingSummary Summary;
			Summary.HeadshotRate = GS->GetHeadshotRate();
			Summary.HitRate = GS->GetHitRate();
			Summary.Headshots = GS->Headshots;
			Summary.TotalHits = GS->TotalHits;
			Summary.TotalShots = GS->TotalShots;
			Summary.BestReactionTimeMs = GS->BestReactionTimeMs;
			StatsPanelWidget->Refresh(Summary);
		}
	}
}
