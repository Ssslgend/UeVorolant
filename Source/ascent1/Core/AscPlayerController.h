#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AscPlayerController.generated.h"

class UCrosshairWidget;
class UStatsPanelWidget;

UCLASS()
class ASCENT1_API AAscPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowCrosshair(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void UpdateCrosshairSpread(float Spread);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowStatsPanel(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RefreshStatsPanel();

	UFUNCTION(BlueprintCallable, Category = "Training")
	void TeleportToTrainingRegion(int32 RegionIndex);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UCrosshairWidget> CrosshairWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UStatsPanelWidget> StatsPanelWidgetClass;

	UPROPERTY()
	UCrosshairWidget* CrosshairWidget;

	UPROPERTY()
	UStatsPanelWidget* StatsPanelWidget;

	void RegionOne();
	void RegionTwo();
	void RegionThree();
	void RegionFour();
	void RegionFive();
	void RegionSix();
};
