#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Training/TrainingPreferences.h"
#include "AscPlayerController.generated.h"

class UCrosshairWidget;
class UStatsPanelWidget;
class ATrainerSession;
class SWidget;

UCLASS()
class ASCENT1_API AAscPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(Exec) void StartPractice(int32 Mode, FString Region, int32 Difficulty, int32 Point = 0);
    UFUNCTION(BlueprintCallable) void ToggleMenu();
    UFUNCTION(BlueprintPure) bool IsMenuOpen() const { return bMenuOpen; }
    ATrainerSession* GetSession() const;
    const FTrainerPreferences& GetPreferences() const { return Preferences; }

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
private:
    bool bMenuOpen = false;
    int32 MenuPage = 0;
    int32 SelectedMode = 1, SelectedDifficulty = 1, SelectedPoint = 0;
    FString SelectedRegion = TEXT("All");
    FString MenuMessage;
    FTrainerPreferences Preferences;
    TSharedPtr<SWidget> MenuRoot;
    void OpenMenu();
    void CloseMenu();
    void BuildMenu();
    void ApplyPreferences();
    void RestartPractice();
    void NextPractice();
    void CapturePlayerPoint();
    void CaptureEnemyPoint();
    void SavePersonalPoint();
};
