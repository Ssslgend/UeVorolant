#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TrainingHUD.generated.h"

UCLASS()
class ASCENT1_API ATrainingHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
private:
    UPROPERTY() class UTexture2D* Minimap = nullptr;
    void DrawFeedback(const class ATrainerSession* Session);
};
