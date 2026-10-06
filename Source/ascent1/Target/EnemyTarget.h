#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Player/AscWeaponComponent.h"
#include "EnemyTarget.generated.h"

class UCapsuleComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Stationary duel bot (D1: player peeks; bot does not).
 * Provides eye/head/torso anchors for TrainerDuel LOS and headshot checks.
 */
UCLASS()
class ASCENT1_API AEnemyTarget : public AActor
{
	GENERATED_BODY()

public:
	AEnemyTarget();

	virtual void Tick(float DeltaTime) override;

	/** Configure head sphere radius from trainer settings (cm). */
	UFUNCTION(BlueprintCallable, Category = "Bot")
	void ConfigureHitVolumes(float HeadRadiusCm, float EyeHeightCm);
    void SetGroundClearance(float Value) { GroundClearance = FMath::Clamp(Value,0.f,5.f); }

	EHitRegion OnShotByPlayer(const FVector& HitPoint);
    void ShowHitReaction(EHitRegion Region);

	/** Distance from hit point to head sphere center (cm). 0 if head hit. */
	float GetDistanceToHead(const FVector& HitPoint) const;

	float GetHeadRadius() const { return HeadRadius; }
	float GetCapsuleRadius() const { return CapsuleRadius; }

	FVector GetEyeLocation() const;
	/** Alias used by TrainerDuel LOS traces. */
	FVector GetEyeWorldLocation() const { return GetEyeLocation(); }
	FVector GetHeadCenter() const;
	FVector GetTorsoTarget() const;
    bool IntersectShotRay(const FVector& Start, const FVector& Direction, float& Distance, EHitRegion& Region) const;

	UFUNCTION(BlueprintCallable, Category = "Bot")
	void ResetBot();

	// Kept for backward compatibility with older call sites; bot stays put.
	UFUNCTION(BlueprintCallable, Category = "Bot")
	void StartPeek();

	UFUNCTION(BlueprintCallable, Category = "Bot")
	void SetPeekPositions(const FVector& HidePos, const FVector& PeekPos);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	UCapsuleComponent* BodyCollision;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* HeadCollision;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* BodyVisual;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HeadVisual;
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* NeckVisual;
    UPROPERTY() TArray<UStaticMeshComponent*> LimbVisuals;
    UPROPERTY() TArray<class UMaterialInstanceDynamic*> BodyFlashMaterials;
    UPROPERTY() class UMaterialInstanceDynamic* HeadFlashMaterial = nullptr;
    float BodyFlash = 0, HeadFlash = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float StandHalfHeight = 91.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float CrouchHalfHeight = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float CapsuleRadius = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float HeadRadius = 9.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float EyeHeight = 160.0f;
    float GroundClearance = 0.0f;

	EHitRegion ProcessHit(const FVector& HitPoint) const;
};
