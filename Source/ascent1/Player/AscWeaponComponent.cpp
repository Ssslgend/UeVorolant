#include "AscWeaponComponent.h"
#include "AscCharacter.h"
#include "../Target/EnemyTarget.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UAscWeaponComponent::UAscWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	CurrentSpread = 0.0f;
	TimeSinceLastShot = 0.0f;
	bWantsToFire = false;
}

void UAscWeaponComponent::PostInitProperties()
{
	Super::PostInitProperties();
	FireInterval = (FireRate > 0.0f) ? (1.0f / FireRate) : 0.1f;
	TimeSinceLastShot = FireInterval;
}

void UAscWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastShot += DeltaTime;

	CurrentSpread = FMath::Max(0.0f, CurrentSpread - SpreadRecoveryRate * DeltaTime);

	if (bWantsToFire && CanFire())
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

void UAscWeaponComponent::PerformTrace()
{
	if (TimeSinceLastShot < FireInterval) return;
	TimeSinceLastShot = 0.0f;

	AAscCharacter* Owner = Cast<AAscCharacter>(GetOwner());
	if (!Owner) return;

	APlayerController* PC = Cast<APlayerController>(Owner->GetController());
	if (!PC) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	FVector Direction = ApplySpread(CameraRotation.Vector());
	FVector End = CameraLocation + Direction * MaxRange;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);

	bool bHit = World->LineTraceSingleByChannel(Hit, CameraLocation, End, ECC_Visibility, Params);

	if (bHit)
	{
		AEnemyTarget* Target = Cast<AEnemyTarget>(Hit.GetActor());
		if (Target)
		{
			Target->OnShotByPlayer(Hit.Location);
		}
	}

	CurrentSpread = FMath::Min(MaxSpread, CurrentSpread + SpreadIncrement);
}

FVector UAscWeaponComponent::ApplySpread(const FVector& Direction) const
{
	if (CurrentSpread <= 0.0f) return Direction;

	FRotator SpreadRotation = Direction.Rotation();
	float RandYaw = FMath::FRandRange(-CurrentSpread, CurrentSpread);
	float RandPitch = FMath::FRandRange(-CurrentSpread, CurrentSpread);
	SpreadRotation.Yaw += RandYaw;
	SpreadRotation.Pitch += RandPitch;
	return SpreadRotation.Vector();
}

EHitRegion UAscWeaponComponent::ProcessHit(AActor* HitActor, const FVector& HitPoint) const
{
	AEnemyTarget* Target = Cast<AEnemyTarget>(HitActor);
	if (!Target) return EHitRegion::Miss;
	// Region detection is done inside EnemyTarget::OnShotByPlayer
	return EHitRegion::Miss;
}
