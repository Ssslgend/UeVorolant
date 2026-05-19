#include "EnemyTarget.h"
#include "../Player/AscCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
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

	CurrentPeekState = EPeekState::Hidden;
	bIsCrouching = false;
	TimeInState = 0.0f;
}

void AEnemyTarget::BeginPlay()
{
	Super::BeginPlay();
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

void AEnemyTarget::OnShotByPlayer(const FVector& HitPoint)
{
	EHitRegion Region = ProcessHit(HitPoint);

	FireTimerHandle.Invalidate();

	if (Region == EHitRegion::Head)
	{
		Destroy();
		return;
	}

	ResetBot();
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
