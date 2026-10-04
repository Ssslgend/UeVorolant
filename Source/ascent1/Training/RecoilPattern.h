#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RecoilPattern.generated.h"

USTRUCT(BlueprintType)
struct FRecoilStep
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	float PitchOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly)
	float YawOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly)
	float TimeToRecover = 0.0f;
};

UCLASS(BlueprintType)
class ASCENT1_API URecoilPattern : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	FString WeaponName;

	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	TArray<FRecoilStep> RecoilSteps;

	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float RecoveryRate = 5.0f;

	int32 CurrentStep = 0;
	FVector2D CurrentRecoil = FVector2D::ZeroVector;

	FVector2D GetNextRecoilOffset();
	void Reset();
};
