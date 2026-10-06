#include "TrainerImpactEffect.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATrainerImpactEffect::ATrainerImpactEffect()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SparkMaterial(TEXT("/Game/CombatTraining/Targets/M_HitSpark.M_HitSpark"));
    for(int32 I=0;I<7;++I)
    {
        auto Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Spark%d"),I));
        Mesh->SetupAttachment(RootComponent);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);Mesh->SetStaticMesh(Cylinder.Object);Mesh->SetMaterial(0,SparkMaterial.Object);
        Mesh->SetRelativeScale3D(FVector(.009f,.009f,.035f));Sparks.Add(Mesh);
    }
    Core=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
    Core->SetupAttachment(RootComponent);Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Core->SetCastShadow(false);Core->SetStaticMesh(Sphere.Object);Core->SetMaterial(0,SparkMaterial.Object);
    Core->SetRelativeScale3D(FVector(.03f));
}

void ATrainerImpactEffect::Initialize(const FVector& IncomingDirection, bool Headshot)
{
    Duration=Headshot?.24f:.14f;Radius=Headshot?22.f:6.f;SparkCount=Headshot?7:4;
    CoreSize=Headshot?.05f:.03f;SparkLength=Headshot?.085f:.055f;
    Core->SetRelativeScale3D(FVector(CoreSize));
    const FVector Normal=-IncomingDirection.GetSafeNormal();
    FVector Right,Up;Normal.FindBestAxisVectors(Right,Up);
    Material=Core->CreateAndSetMaterialInstanceDynamic(0);
    if(Material)Material->SetVectorParameterValue(TEXT("SparkColor"),Headshot?FLinearColor(12000,6500,1200):FLinearColor(6500,7200,8000));
    for(int32 I=0;I<Sparks.Num();++I)
    {
        if(Material)Sparks[I]->SetMaterial(0,Material);
        Sparks[I]->SetVisibility(I<SparkCount);
        Sparks[I]->SetRelativeScale3D(FVector(.009f,.009f,SparkLength));
        const float Angle=2*PI*I/SparkCount+.25f;
        const FVector Direction=(Right*FMath::Cos(Angle)+Up*FMath::Sin(Angle)+Normal*.35f).GetSafeNormal();
        Directions.Add(Direction);
        Sparks[I]->SetRelativeRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
        Sparks[I]->SetRelativeLocation(Direction*2.f);
    }
    SetLifeSpan(Duration+.03f);
}

void ATrainerImpactEffect::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);Age+=DeltaSeconds;
    const float Progress=FMath::Clamp(Age/Duration,0.f,1.f),Fade=1.f-Progress;
    Core->SetRelativeScale3D(FVector(CoreSize*Fade));
    for(int32 I=0;I<Sparks.Num()&&I<Directions.Num();++I)
    {
        Sparks[I]->SetRelativeLocation(Directions[I]*(2.f+Progress*Radius));
        Sparks[I]->SetRelativeScale3D(FVector(.009f*Fade,.009f*Fade,SparkLength*Fade));
    }
}
