#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Training/TrainingPreferences.h"
#include "AscCharacter.generated.h"

class UCameraComponent;
class UAscWeaponComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Knife	UMETA(DisplayName = "Knife"),
	Rifle	UMETA(DisplayName = "Rifle"),
	Pistol	UMETA(DisplayName = "Pistol")
};

UCLASS()
class ASCENT1_API AAscCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAscCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetMoveSpeed() const { return CurrentMoveSpeed; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponType(EWeaponType NewType);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	EWeaponType GetWeaponType() const { return CurrentWeapon; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	FVector GetCameraLocation() const;
    FVector GetGunMuzzleLocation() const;
    void PlayViewModelShot();

	UFUNCTION(BlueprintCallable, Category = "Training")
	void ResetDuel();

	/** Apply trainer settings (speeds, smoothing). */
	UFUNCTION(BlueprintCallable, Category = "Training")
	void ApplyTrainerSettings(const struct FTrainerSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Training")
	void ApplyPreferences(const FTrainerPreferences& Preferences);

	// ---- Enhanced Input handlers ----
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void WalkPressed();
	void WalkReleased();
	void CrouchPressed();
	void FirePressed();
	void FireReleased();
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);

	// Valorant-style instant stop movement params
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float RifleSpeed = 540.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float KnifeSpeed = 675.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float WalkSpeed = 405.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float CrouchSpeed = 340.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float WalkMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float CrouchMultiplier = 0.63f;

protected:
	UPROPERTY(VisibleAnywhere)
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere)
	UAscWeaponComponent* WeaponComponent;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputMappingContext* InputMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* IA_Move;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* IA_Look;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* IA_Walk;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* IA_CrouchToggle;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* IA_Jump;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* IA_Fire;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* IA_Reset;

	EWeaponType CurrentWeapon;
	float CurrentMoveSpeed;
	bool bIsWalking;
	bool bIsFiring;
	float MouseSensitivity = 0.4f;
	void NormalizeMouseInput();
    UPROPERTY() class USceneComponent* RifleRoot = nullptr;
    UPROPERTY() class USceneComponent* RifleMuzzle = nullptr;
    UPROPERTY() class UStaticMeshComponent* RifleMuzzleFlash = nullptr;
    UPROPERTY() TArray<class UStaticMeshComponent*> ViewModelParts;
    float ViewModelKick = 0, MuzzleFlashRemaining = 0, ViewModelBobTime = 0;
    void BuildViewModel();
    void TickViewModel(float DeltaTime);
};
