#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainerShotTracer.generated.h"

/** Visual-only short-lived beam for the actual resolved hitscan shot. */
UCLASS()
class ASCENT1_API ATrainerShotTracer : public AActor
{
    GENERATED_BODY()
public:
    ATrainerShotTracer();
    void Initialize(const FVector& From,const FVector& To,bool EnemyShot=false);
    virtual void Tick(float DeltaSeconds) override;
    FVector GetTraceStart() const { return TraceStart; }
    FVector GetTraceEnd() const { return TraceEnd; }
private:
    UPROPERTY() class UStaticMeshComponent* Beam;
    FVector TraceStart,TraceEnd;
    float Age=0,Duration=.09f;
};
