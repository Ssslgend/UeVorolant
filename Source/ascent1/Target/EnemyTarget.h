#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Player/AscWeaponComponent.h"
#include "EnemyTarget.generated.h"

class UCapsuleComponent;
class USphereComponent;
class UStaticMeshComponent;
class AAscCharacter;

UENUM(BlueprintType)
enum class EPeekState : uint8
{
	Hidden,
	Peeking,
	Holding,
	Retracting
};

UCLASS()
class ASCENT1_API AEnemyTarget : public AActor
{
	GENERATED_BODY()

public:
	AEnemyTarget();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Bot")
	void StartPeek();

	UFUNCTION(BlueprintCallable, Category = "Bot")
	void SetPeekPositions(const FVector& HidePos, const FVector& PeekPos);

	EHitRegion OnShotByPlayer(const FVector& HitPoint);

	float GetReactionTimeMs() const;

protected:
	virtual void BeginPlay() override;

	// ---- Bot fire ----
	UFUNCTION()
	void BotFire();

	UFUNCTION(BlueprintCallable, Category = "Bot")
	void ResetBot();

	// ---- Collision ----
	UPROPERTY(VisibleAnywhere)
	UCapsuleComponent* BodyCollision;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* HeadCollision;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* BodyVisual;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HeadVisual;

	// ---- Bot fire timing ----
	UPROPERTY(EditDefaultsOnly, Category = "Bot|Timing")
	float FireDelay = 0.25f;

	FTimerHandle FireTimerHandle;

	// ---- Peek params ----
	FVector CoverPosition;
	FVector PeekPosition;
	EPeekState CurrentPeekState;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Peek")
	float PeekDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Peek")
	float PeekSpeed = 400.0f;

	float TimeInState;

	// ---- Body dimensions ----
	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float StandHalfHeight = 91.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float CrouchHalfHeight = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float CapsuleRadius = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float HeadRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot|Dimensions")
	float EyeHeight = 160.0f;

	bool bIsCrouching;
	float SpawnTimeSeconds;

	void UpdatePeek(float DeltaTime);
	EHitRegion ProcessHit(const FVector& HitPoint) const;
};
