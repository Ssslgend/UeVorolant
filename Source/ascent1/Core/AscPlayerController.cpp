#include "AscPlayerController.h"
#include "AscGameState.h"
#include "../Training/TrainingStats.h"
#include "../Training/TrainingHubBuilder.h"
#include "../UI/CrosshairWidget.h"
#include "../UI/StatsPanelWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"

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

void AAscPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AAscPlayerController::RegionOne);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AAscPlayerController::RegionTwo);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AAscPlayerController::RegionThree);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AAscPlayerController::RegionFour);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AAscPlayerController::RegionFive);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AAscPlayerController::RegionSix);
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

void AAscPlayerController::TeleportToTrainingRegion(int32 RegionIndex)
{
	constexpr float Spacing = 18000.0f;
	const float RegionOffsets[] = {
		-2.5f * Spacing,
		-1.5f * Spacing,
		-0.5f * Spacing,
		0.5f * Spacing,
		1.5f * Spacing,
		2.5f * Spacing,
	};

	if (RegionIndex < 0 || RegionIndex >= UE_ARRAY_COUNT(RegionOffsets))
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	ControlledPawn->SetActorLocation(
		FVector(RegionOffsets[RegionIndex], 0.0f, 0.0f) + ATrainingHubBuilder::GetTrainingPlayerOffset(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetControlRotation(ATrainingHubBuilder::GetTrainingRegionFacingRotation());
}

void AAscPlayerController::RegionOne() { TeleportToTrainingRegion(0); }
void AAscPlayerController::RegionTwo() { TeleportToTrainingRegion(1); }
void AAscPlayerController::RegionThree() { TeleportToTrainingRegion(2); }
void AAscPlayerController::RegionFour() { TeleportToTrainingRegion(3); }
void AAscPlayerController::RegionFive() { TeleportToTrainingRegion(4); }
void AAscPlayerController::RegionSix() { TeleportToTrainingRegion(5); }
