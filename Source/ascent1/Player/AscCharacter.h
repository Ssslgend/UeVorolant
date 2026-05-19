#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
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

	// ---- Enhanced Input handlers ----
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void WalkPressed();
	void WalkReleased();
	void CrouchPressed();
	void FirePressed();
	void FireReleased();

	// Valorant-style instant stop movement params
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float RifleSpeed = 540.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float KnifeSpeed = 675.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float WalkSpeed = 405.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement|Valorant")
	float CrouchSpeed = 340.0f;

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

	EWeaponType CurrentWeapon;
	float CurrentMoveSpeed;
	bool bIsWalking;
	bool bIsFiring;
};
