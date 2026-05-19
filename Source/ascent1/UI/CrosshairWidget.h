#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrosshairWidget.generated.h"

UCLASS()
class ASCENT1_API UCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void UpdateSpread(float Spread);

	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void ShowHitMarker(bool bHeadshot);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Crosshair")
	float CurrentSpread;

	UPROPERTY(BlueprintReadOnly, Category = "Crosshair")
	bool bShowingHitMarker;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float HitMarkerDuration = 0.3f;

	FTimerHandle HitMarkerTimerHandle;
	void HideHitMarker();
};
