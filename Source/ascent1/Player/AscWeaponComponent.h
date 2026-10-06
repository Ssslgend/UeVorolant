#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/TrainingSettings.h"
#include "AscWeaponComponent.generated.h"

UENUM()
enum class EHitRegion : uint8
{
	Miss,
	Head,
	Body,
	Legs
};

/** Broadcast for every bullet actually fired; the arena resolves the target. */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnWeaponShot, const FVector& /*RayStart*/, const FVector& /*RayEnd*/, const FVector& /*ImpactPoint*/, bool /*bHitWorld*/);

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

	/** Result screen freeze: shots are ignored while frozen. */
	UFUNCTION(BlueprintCallable, Category = "Shooting")
	void SetFrozen(bool bInFrozen) { bFrozen = bInFrozen; }

	UFUNCTION(BlueprintCallable, Category = "Shooting")
	bool IsFrozen() const { return bFrozen; }

	UFUNCTION(BlueprintCallable, Category = "Shooting")
	void ApplySettings(const FTrainerSettings& InSettings);

	/** Fires immediately, ignoring the fire rate gate (self test / scripted shots). */
	UFUNCTION(BlueprintCallable, Category = "Shooting")
	bool FireOnceNow();

	UFUNCTION(BlueprintCallable, Category = "Shooting")
	float GetCurrentSpread() const { return CurrentSpread; }

	/** Last ray traced by this weapon. */
	void GetLastShotRay(FVector& OutStart, FVector& OutEnd) const { OutStart = LastShotStart; OutEnd = LastShotEnd; }
    void SetResolvedShotImpact(const FVector& Point,AActor* Target) { LastShotEnd=Point;ResolvedShotTarget=Target; }

	FOnWeaponShot OnWeaponShot;

	// ---- Damage values (single shot kill on head) ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float HeadDamage = 160.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float BodyDamage = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float LegsDamage = 34.0f;

	// ---- Fire rate ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|FireRate")
	float FireRate = 9.75f;

	// ---- Spread (movement penalty lands in a later milestone; interface kept) ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float FirstShotSpread = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float SpreadIncrement = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float MaxSpread = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float SpreadRecoveryRate = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Trace")
	float MaxRange = 20000.0f;

	/** Movement speed above which the crosshair blooms (F8 hook, 0 disables). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float MovementSpreadPerCms = 0.0f;

protected:
	float CurrentSpread;
	float TimeSinceLastShot;
	bool bWantsToFire;
	bool bFrozen;
	float FireInterval;

	FVector LastShotStart;
	FVector LastShotEnd;
    UPROPERTY() class USoundBase* ShotSound = nullptr;
    UPROPERTY() AActor* ResolvedShotTarget = nullptr;

	virtual void PostInitProperties() override;

	FVector ApplySpread(const FVector& Direction) const;
	bool PerformTrace();
};
