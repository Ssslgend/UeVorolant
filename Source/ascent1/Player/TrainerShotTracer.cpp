#include "TrainerShotTracer.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATrainerShotTracer::ATrainerShotTracer()
{
    PrimaryActorTick.bCanEverTick=true;
    Beam=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));SetRootComponent(Beam);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/CombatTraining/Targets/M_HitSpark.M_HitSpark"));
    Beam->SetStaticMesh(Cylinder.Object);Beam->SetMaterial(0,Material.Object);
    Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);Beam->SetCastShadow(false);
}

void ATrainerShotTracer::Initialize(const FVector& From,const FVector& To,bool EnemyShot)
{
    TraceStart=From;TraceEnd=To;
    const FVector Delta=To-From;
    SetActorLocation((From+To)*.5f);SetActorRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
    SetActorScale3D(FVector(.0035f,.0035f,Delta.Size()/100.f));
    Tags.Add(EnemyShot?TEXT("EnemyShotTracer"):TEXT("PlayerShotTracer"));
    if(auto Material=Beam->CreateAndSetMaterialInstanceDynamic(0))
        Material->SetVectorParameterValue(TEXT("SparkColor"),EnemyShot?FLinearColor(10000,2200,900):FLinearColor(8500,7000,3800));
    SetLifeSpan(Duration);
}

void ATrainerShotTracer::Tick(float Dt)
{
    Super::Tick(Dt);Age+=Dt;
    const float Width=.0035f*FMath::Clamp(1.f-Age/Duration,0.f,1.f);
    SetActorScale3D(FVector(Width,Width,FVector::Dist(TraceStart,TraceEnd)/100.f));
}
