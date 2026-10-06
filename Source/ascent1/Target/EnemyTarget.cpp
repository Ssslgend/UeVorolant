#include "EnemyTarget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Training/TrainerSession.h"

AEnemyTarget::AEnemyTarget()
{
	PrimaryActorTick.bCanEverTick = true;

	BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
	BodyCollision->SetCapsuleHalfHeight(StandHalfHeight);
	BodyCollision->SetCapsuleRadius(CapsuleRadius);
    BodyCollision->SetCollisionProfileName(TEXT("Pawn"));
	SetRootComponent(BodyCollision);

	HeadCollision = CreateDefaultSubobject<USphereComponent>(TEXT("HeadCollision"));
	HeadCollision->SetupAttachment(BodyCollision);
	HeadCollision->SetSphereRadius(HeadRadius);
	// Head center near eye height so head-line practice matches stand pose
	HeadCollision->SetRelativeLocation(FVector(0.0f, 0.0f, EyeHeight - StandHalfHeight));

	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
	BodyVisual->SetupAttachment(BodyCollision);
	BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyVisual->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.6f));

	HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
	HeadVisual->SetupAttachment(HeadCollision);
	HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVisual->SetRelativeScale3D(FVector(0.15f));

    NeckVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NeckVisual"));
    NeckVisual->SetupAttachment(BodyCollision);
    NeckVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BodyVisual->SetStaticMesh(CylinderMesh.Object);
        NeckVisual->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		HeadVisual->SetStaticMesh(SphereMesh.Object);
	}
    for(int I=0;I<4;++I)
    {
        auto Limb=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Limb%d"),I));
        Limb->SetupAttachment(BodyCollision);Limb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if(CylinderMesh.Succeeded())Limb->SetStaticMesh(CylinderMesh.Object);
        LimbVisuals.Add(Limb);
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMaterial(TEXT("/Game/CombatTraining/Targets/M_TargetBody.M_TargetBody"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HeadMaterial(TEXT("/Game/CombatTraining/Targets/M_TargetHead.M_TargetHead"));
    if(BodyMaterial.Succeeded()){BodyVisual->SetMaterial(0,BodyMaterial.Object);NeckVisual->SetMaterial(0,BodyMaterial.Object);for(auto Limb:LimbVisuals)Limb->SetMaterial(0,BodyMaterial.Object);}
    if(HeadMaterial.Succeeded())HeadVisual->SetMaterial(0,HeadMaterial.Object);
}

void AEnemyTarget::BeginPlay()
{
	Super::BeginPlay();
	ConfigureHitVolumes(HeadRadius, EyeHeight);
}

void AEnemyTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    BodyFlash=FMath::Max(0.f,BodyFlash-DeltaTime*8.f);
    HeadFlash=FMath::Max(0.f,HeadFlash-DeltaTime*8.f);
    for(auto Material:BodyFlashMaterials)if(Material)Material->SetScalarParameterValue(TEXT("ImpactFlash"),BodyFlash);
    if(HeadFlashMaterial)HeadFlashMaterial->SetScalarParameterValue(TEXT("ImpactFlash"),HeadFlash);
}

void AEnemyTarget::ShowHitReaction(EHitRegion Region)
{
    if(Region==EHitRegion::Miss)return;
    if(Region==EHitRegion::Head)
    {
        if(!HeadFlashMaterial)HeadFlashMaterial=HeadVisual->CreateAndSetMaterialInstanceDynamic(0);
        HeadFlash=1.f;
        if(HeadFlashMaterial)HeadFlashMaterial->SetScalarParameterValue(TEXT("ImpactFlash"),HeadFlash);
        return;
    }
    if(BodyFlashMaterials.IsEmpty())
    {
        TArray<UStaticMeshComponent*> Meshes=LimbVisuals;Meshes.Add(BodyVisual);Meshes.Add(NeckVisual);
        for(auto Mesh:Meshes)BodyFlashMaterials.Add(Mesh->CreateAndSetMaterialInstanceDynamic(0));
    }
    BodyFlash=1.f;
    for(auto Material:BodyFlashMaterials)if(Material)Material->SetScalarParameterValue(TEXT("ImpactFlash"),BodyFlash);
}

void AEnemyTarget::ConfigureHitVolumes(float HeadRadiusCm, float EyeHeightCm)
{
	HeadRadius = FMath::Max(1.0f, HeadRadiusCm);
	EyeHeight = FMath::Max(50.0f, EyeHeightCm);
    BodyVisual->SetRelativeLocation(FVector(0,0,EyeHeight*.66f-StandHalfHeight-GroundClearance));
    BodyVisual->SetRelativeScale3D(FVector(.44f,.44f,EyeHeight*.0055f));
    // Derive both neck ends from the actual torso top and head bottom.
    // Two centimeters of overlap at each end keeps standing/crouched poses seated.
    const float TorsoTop=EyeHeight*.66f+EyeHeight*.0055f*50.f;
    const float HeadBottom=EyeHeight-HeadRadius;
    const float NeckHeight=FMath::Abs(HeadBottom-TorsoTop)+4.f;
    NeckVisual->SetRelativeScale3D(FVector(.10f,.10f,NeckHeight/100.f));
    NeckVisual->SetRelativeLocation(FVector(0,0,(TorsoTop+HeadBottom)*.5f-StandHalfHeight-GroundClearance));
    for(int I=0;I<LimbVisuals.Num();++I)
    {
        const bool Leg=I<2;const float Height=EyeHeight*(Leg?.38f:.36f);
        LimbVisuals[I]->SetRelativeScale3D(FVector(.14,.14,Height/100));
        LimbVisuals[I]->SetRelativeLocation(FVector(0,(I%2?1:-1)*(Leg?11.f:29.f),(Leg?Height*.5f:EyeHeight*.65f)-StandHalfHeight-GroundClearance));
    }

	if (HeadCollision)
	{
		HeadCollision->SetSphereRadius(HeadRadius);
		HeadCollision->SetRelativeLocation(FVector(0.0f, 0.0f, EyeHeight - StandHalfHeight - GroundClearance));
	}
	if (HeadVisual)
	{
		// Engine sphere is 100 units diameter at scale 1
		const float Scale = (HeadRadius * 2.0f) / 100.0f;
		HeadVisual->SetRelativeScale3D(FVector(Scale));
	}
}

FVector AEnemyTarget::GetEyeLocation() const
{
	// Actor origin is capsule center (floor + StandHalfHeight). EyeHeight is from floor.
	return GetActorLocation() + FVector(0.0f, 0.0f, EyeHeight - StandHalfHeight - GroundClearance);
}

FVector AEnemyTarget::GetHeadCenter() const
{
	return HeadCollision ? HeadCollision->GetComponentLocation() : GetEyeLocation();
}

FVector AEnemyTarget::GetTorsoTarget() const
{
	// Upper chest ~110cm from floor
	return GetActorLocation() + FVector(0.0f, 0.0f, EyeHeight * .66f - StandHalfHeight - GroundClearance);
}

bool AEnemyTarget::IntersectShotRay(const FVector& Start, const FVector& Direction, float& Distance, EHitRegion& Region) const
{
    Distance=TNumericLimits<float>::Max();Region=EHitRegion::Miss;
    float HeadDistance=0;
    if(ATrainerSession::RaySphere(Start,Direction,GetHeadCenter(),HeadRadius,HeadDistance))
    {Distance=HeadDistance;Region=EHitRegion::Head;}
    // Engine cylinders have local radius/half-height 50. Transform the ray
    // without normalizing it, so each intersection parameter stays in world cm.
    const auto Cylinder=[&](const UStaticMeshComponent* Mesh,EHitRegion HitRegion)
    {
        const FTransform Transform=Mesh->GetComponentTransform();
        const FVector O=Transform.InverseTransformPosition(Start);
        const FVector D=Transform.InverseTransformVector(Direction);
        const auto Accept=[&](double T)
        {
            if(T>=0 && T<Distance){Distance=static_cast<float>(T);Region=HitRegion;}
        };
        const double A=D.X*D.X+D.Y*D.Y,B=2*(O.X*D.X+O.Y*D.Y),C=O.X*O.X+O.Y*O.Y-2500;
        const double Disc=B*B-4*A*C;
        if(A>1e-12 && Disc>=0)
            for(const double T:{(-B-FMath::Sqrt(Disc))/(2*A),(-B+FMath::Sqrt(Disc))/(2*A)})
                if(FMath::Abs(O.Z+D.Z*T)<=50)Accept(T);
        if(FMath::Abs(D.Z)>1e-12)
            for(const double Z:{-50.,50.})
            {
                const double T=(Z-O.Z)/D.Z;
                if(FMath::Square(O.X+D.X*T)+FMath::Square(O.Y+D.Y*T)<=2500)Accept(T);
            }
    };
    Cylinder(BodyVisual,EHitRegion::Body);Cylinder(NeckVisual,EHitRegion::Body);
    for(int32 I=0;I<LimbVisuals.Num();++I)Cylinder(LimbVisuals[I],I<2?EHitRegion::Legs:EHitRegion::Body);
    return Region!=EHitRegion::Miss;
}

float AEnemyTarget::GetDistanceToHead(const FVector& HitPoint) const
{
	return FVector::Dist(HitPoint, GetHeadCenter());
}

EHitRegion AEnemyTarget::OnShotByPlayer(const FVector& HitPoint)
{
    const EHitRegion Region=ProcessHit(HitPoint);ShowHitReaction(Region);return Region;
}

void AEnemyTarget::StartPeek()
{
	// No-op: bot holds position (real walk peek is player-driven).
}

void AEnemyTarget::SetPeekPositions(const FVector& HidePos, const FVector& PeekPos)
{
	// Backward compatible no-op; placement is done by arena/loader.
}

void AEnemyTarget::ResetBot()
{
	// Position is owned by the arena for the current spot.
}

EHitRegion AEnemyTarget::ProcessHit(const FVector& HitPoint) const
{
	const FVector HeadCenter = GetHeadCenter();
	const float DistToHead = FVector::Dist(HitPoint, HeadCenter);

	if (DistToHead <= HeadRadius)
	{
		return EHitRegion::Head;
	}

	const FVector BodyOrigin = BodyCollision ? BodyCollision->GetComponentLocation() : GetActorLocation();
	const float HalfHeight = BodyCollision ? BodyCollision->GetScaledCapsuleHalfHeight() : StandHalfHeight;
	const float HitFraction = (HitPoint.Z - (BodyOrigin.Z - HalfHeight)) / (HalfHeight * 2.0f);

	if (HitFraction > 0.6f)
	{
		return EHitRegion::Body;
	}
	return EHitRegion::Legs;
}
