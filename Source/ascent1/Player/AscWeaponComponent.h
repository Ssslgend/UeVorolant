#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AscWeaponComponent.generated.h"

UENUM()
enum class EHitRegion : uint8
{
	Miss,
	Head,
	Body,
	Legs
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ASCENT1_API UAscWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAscWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Shooting")
	void Fire();

	UFUNCTION(BlueprintCallable, Category = "Shooting")
	void StopFiring();

	UFUNCTION(BlueprintCallable, Category = "Shooting")
	bool CanFire() const;

	// ---- Damage values ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float HeadDamage = 160.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float BodyDamage = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float LegsDamage = 34.0f;

	// ---- Fire rate ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|FireRate")
	float FireRate = 9.75f;

	// ---- Spread ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float FirstShotSpread = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float SpreadIncrement = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float MaxSpread = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float SpreadRecoveryRate = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Trace")
	float MaxRange = 10000.0f;

protected:
	float CurrentSpread;
	float TimeSinceLastShot;
	bool bWantsToFire;

	virtual void PostInitProperties() override;

	FVector ApplySpread(const FVector& Direction) const;
	void PerformTrace();
	EHitRegion ProcessHit(AActor* HitActor, const FVector& HitPoint) const;

	float FireInterval;
};
