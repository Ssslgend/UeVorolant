#include "AscWeaponComponent.h"
#include "AscCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UAscWeaponComponent::UAscWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	CurrentSpread = 0.0f;
	TimeSinceLastShot = 0.0f;
	bWantsToFire = false;
	bFrozen = false;
	LastShotStart = FVector::ZeroVector;
	LastShotEnd = FVector::ZeroVector;
}

void UAscWeaponComponent::PostInitProperties()
{
	Super::PostInitProperties();
	FireInterval = (FireRate > 0.0f) ? (1.0f / FireRate) : 0.1f;
	TimeSinceLastShot = FireInterval;
}

void UAscWeaponComponent::ApplySettings(const FTrainerSettings& InSettings)
{
	// Movement accuracy penalty (F8) is a later milestone: keep the hook at zero.
	MovementSpreadPerCms = 0.0f;
}

void UAscWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastShot += DeltaTime;
	CurrentSpread = FMath::Max(0.0f, CurrentSpread - SpreadRecoveryRate * DeltaTime);

	if (bWantsToFire && !bFrozen && CanFire())
	{
		PerformTrace();
	}
}

void UAscWeaponComponent::Fire()
{
	bWantsToFire = true;
}

void UAscWeaponComponent::StopFiring()
{
	bWantsToFire = false;
}

bool UAscWeaponComponent::CanFire() const
{
	return TimeSinceLastShot >= FireInterval;
}

bool UAscWeaponComponent::FireOnceNow()
{
	if (bFrozen)
	{
		return false;
	}
	TimeSinceLastShot = FireInterval;
	return PerformTrace();
}

bool UAscWeaponComponent::PerformTrace()
{
	if (bFrozen || TimeSinceLastShot < FireInterval)
	{
		return false;
	}
	TimeSinceLastShot = 0.0f;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (const APlayerController* PC = Cast<APlayerController>(Owner->GetInstigatorController()))
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		ViewLocation = Character->GetPawnViewLocation();
		ViewRotation = Character->GetViewRotation();
	}
	else
	{
		ViewLocation = Owner->GetActorLocation();
		ViewRotation = Owner->GetActorRotation();
	}

    // A moving first shot is inaccurate until the player counter-strafes/stops.
    const float Speed = Owner->GetVelocity().Size2D();
    const float MovementSpread = Speed > 50.f ? FMath::Min(1.5f, (Speed - 50.f) * 0.004f) : 0.f;
    FRotator ShotRotation = ViewRotation;
    ShotRotation.Yaw += FMath::FRandRange(-MovementSpread, MovementSpread);
    ShotRotation.Pitch += FMath::FRandRange(-MovementSpread, MovementSpread);
	const FVector Direction = ApplySpread(ShotRotation.Vector());
	const FVector End = ViewLocation + Direction * MaxRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainerShot), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(Owner);

	FHitResult Hit;
	const bool bHitWorld = World->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params);

	LastShotStart = ViewLocation;
	LastShotEnd = bHitWorld ? Hit.ImpactPoint : End;

	CurrentSpread = FMath::Min(MaxSpread, CurrentSpread + SpreadIncrement);

	// Full ray (Start->End) plus the world impact so the arena can do analytic head tests.
	OnWeaponShot.Broadcast(ViewLocation, End, LastShotEnd, bHitWorld);
    if(!ShotSound)ShotSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/CombatTraining/Audio/TrainingShot.TrainingShot"));
    if(ShotSound)UGameplayStatics::PlaySound2D(this,ShotSound,.7f);
	return true;
}

FVector UAscWeaponComponent::ApplySpread(const FVector& Direction) const
{
	if (CurrentSpread <= 0.0f)
	{
		return Direction;
	}

	FRotator SpreadRotation = Direction.Rotation();
	SpreadRotation.Yaw += FMath::FRandRange(-CurrentSpread, CurrentSpread);
	SpreadRotation.Pitch += FMath::FRandRange(-CurrentSpread, CurrentSpread);
	return SpreadRotation.Vector();
}
