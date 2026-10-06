#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainerImpactEffect.generated.h"

/** Short, collision-free sparks at the confirmed impact, independent of a killed target. */
UCLASS()
class ASCENT1_API ATrainerImpactEffect : public AActor
{
    GENERATED_BODY()
public:
    ATrainerImpactEffect();
    void Initialize(const FVector& IncomingDirection, bool Headshot);
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TArray<class UStaticMeshComponent*> Sparks;
    UPROPERTY() class UStaticMeshComponent* Core;
    UPROPERTY() class UMaterialInstanceDynamic* Material;
    TArray<FVector> Directions;
    float Age = 0, Duration = .18f, Radius = 12.f, CoreSize = .03f, SparkLength = .055f;
    int32 SparkCount = 7;
};
