#include "EnemyTarget.h"
#include "../Player/AscCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

AEnemyTarget::AEnemyTarget()
{
	PrimaryActorTick.bCanEverTick = true;

	BodyCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyCollision"));
	BodyCollision->SetCapsuleHalfHeight(StandHalfHeight);
	BodyCollision->SetCapsuleRadius(CapsuleRadius);
	SetRootComponent(BodyCollision);

	HeadCollision = CreateDefaultSubobject<USphereComponent>(TEXT("HeadCollision"));
	HeadCollision->SetupAttachment(BodyCollision);
	HeadCollision->SetSphereRadius(HeadRadius);
	HeadCollision->SetRelativeLocation(FVector(0.0f, 0.0f, StandHalfHeight - HeadRadius));

	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
	BodyVisual->SetupAttachment(BodyCollision);
	BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyVisual->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.6f));

	HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
	HeadVisual->SetupAttachment(HeadCollision);
	HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVisual->SetRelativeScale3D(FVector(0.25f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BodyVisual->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		HeadVisual->SetStaticMesh(SphereMesh.Object);
	}

	CurrentPeekState = EPeekState::Hidden;
	bIsCrouching = false;
	TimeInState = 0.0f;
	SpawnTimeSeconds = 0.0f;
}

void AEnemyTarget::BeginPlay()
{
	Super::BeginPlay();
	SpawnTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void AEnemyTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdatePeek(DeltaTime);
}

void AEnemyTarget::StartPeek()
{
	CurrentPeekState = EPeekState::Peeking;
	TimeInState = 0.0f;
	FireTimerHandle.Invalidate();
}

void AEnemyTarget::SetPeekPositions(const FVector& HidePos, const FVector& PeekPos)
{
	CoverPosition = HidePos;
	PeekPosition = PeekPos;
}

void AEnemyTarget::UpdatePeek(float DeltaTime)
{
	if (CoverPosition == PeekPosition) return;

	UWorld* World = GetWorld();
	if (!World) return;

	TimeInState += DeltaTime;

	switch (CurrentPeekState)
	{
	case EPeekState::Peeking:
	{
		FVector ToTarget = PeekPosition - CoverPosition;
		float Dist = ToTarget.Size();
		if (Dist > 0.0f)
		{
			FVector Step = ToTarget.GetSafeNormal() * PeekSpeed * DeltaTime;
			if (Step.Size() >= Dist)
			{
				SetActorLocation(PeekPosition);
				CurrentPeekState = EPeekState::Holding;
				TimeInState = 0.0f;
				World->GetTimerManager().SetTimer(FireTimerHandle, this, &AEnemyTarget::BotFire, FireDelay, false);
			}
			else
			{
				SetActorLocation(GetActorLocation() + Step);
			}
		}
		break;
	}
	case EPeekState::Holding:
	{
		if (TimeInState >= PeekDuration)
		{
			CurrentPeekState = EPeekState::Retracting;
			TimeInState = 0.0f;
		}
		break;
	}
	case EPeekState::Retracting:
	{
		FVector ToTarget = CoverPosition - PeekPosition;
		float Dist = ToTarget.Size();
		if (Dist > 0.0f)
		{
			FVector Step = ToTarget.GetSafeNormal() * PeekSpeed * DeltaTime;
			if (Step.Size() >= Dist)
			{
				SetActorLocation(CoverPosition);
				CurrentPeekState = EPeekState::Hidden;
			}
			else
			{
				SetActorLocation(GetActorLocation() + Step);
			}
		}
		else
		{
			CurrentPeekState = EPeekState::Hidden;
		}
		break;
	}
	default:
		break;
	}
}

EHitRegion AEnemyTarget::OnShotByPlayer(const FVector& HitPoint)
{
	EHitRegion Region = ProcessHit(HitPoint);

	FireTimerHandle.Invalidate();

	if (Region == EHitRegion::Head)
	{
		Destroy();
		return Region;
	}

	ResetBot();
	return Region;
}

float AEnemyTarget::GetReactionTimeMs() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	return (World->GetTimeSeconds() - SpawnTimeSeconds) * 1000.0f;
}

void AEnemyTarget::BotFire()
{
	// Notify game mode / arena that player is killed
	// The arena listens for this via delegate or direct call
	ResetBot();
}

void AEnemyTarget::ResetBot()
{
	SetActorLocation(CoverPosition);
	CurrentPeekState = EPeekState::Hidden;
	TimeInState = 0.0f;
}

EHitRegion AEnemyTarget::ProcessHit(const FVector& HitPoint) const
{
	FVector HeadCenter = HeadCollision->GetComponentLocation();
	float DistToHead = FVector::Dist(HitPoint, HeadCenter);

	if (DistToHead <= HeadRadius)
	{
		return EHitRegion::Head;
	}

	FVector BodyOrigin = BodyCollision->GetComponentLocation();
	FVector BodyExtent = BodyCollision->GetScaledCapsuleHalfHeight() * FVector::UpVector;
	FVector HitRelative = HitPoint - (BodyOrigin - BodyExtent);
	float TotalHeight = BodyExtent.Z * 2.0f;
	float HitFraction = HitRelative.Z / TotalHeight;

	if (HitFraction > 0.6f)
	{
		return EHitRegion::Body;
	}
	return EHitRegion::Legs;
}
