#include "AscCharacter.h"
#include "AscWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/CapsuleComponent.h"

AAscCharacter::AAscCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f));
	Camera->bUsePawnControlRotation = true;

	WeaponComponent = CreateDefaultSubobject<UAscWeaponComponent>(TEXT("WeaponComponent"));

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxAcceleration = 100000.0f;
	Movement->BrakingDecelerationWalking = 100000.0f;
	Movement->GroundFriction = 100.0f;
	Movement->AirControl = 0.02f;
	Movement->GravityScale = 1.4f;
	Movement->JumpZVelocity = 370.0f;
	Movement->MaxWalkSpeed = RifleSpeed;

	GetCapsuleComponent()->SetCapsuleHalfHeight(91.5f);
	GetCapsuleComponent()->SetCapsuleRadius(25.0f);

	CurrentWeapon = EWeaponType::Rifle;
	CurrentMoveSpeed = RifleSpeed;
	bIsWalking = false;
	bIsFiring = false;
}

void AAscCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (InputMappingContext)
			{
				Subsystem->AddMappingContext(InputMappingContext, 0);
			}
		}
	}
}

void AAscCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float TargetSpeed = CurrentMoveSpeed;

	switch (CurrentWeapon)
	{
	case EWeaponType::Knife:
		TargetSpeed = KnifeSpeed;
		break;
	case EWeaponType::Rifle:
		TargetSpeed = RifleSpeed;
		break;
	case EWeaponType::Pistol:
		TargetSpeed = KnifeSpeed;
		break;
	}

	if (bIsWalking)
	{
		TargetSpeed *= 0.75f;
	}
	if (GetCharacterMovement()->IsCrouching())
	{
		TargetSpeed *= 0.63f;
	}

	CurrentMoveSpeed = TargetSpeed;
	GetCharacterMovement()->MaxWalkSpeed = CurrentMoveSpeed;
}

void AAscCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AAscCharacter::Move);
		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AAscCharacter::Look);
		EnhancedInput->BindAction(IA_Walk, ETriggerEvent::Started, this, &AAscCharacter::WalkPressed);
		EnhancedInput->BindAction(IA_Walk, ETriggerEvent::Completed, this, &AAscCharacter::WalkReleased);
		EnhancedInput->BindAction(IA_CrouchToggle, ETriggerEvent::Started, this, &AAscCharacter::CrouchPressed);
		EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &AAscCharacter::FirePressed);
		EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AAscCharacter::FireReleased);
	}
}

void AAscCharacter::SetWeaponType(EWeaponType NewType)
{
	CurrentWeapon = NewType;
}

void AAscCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Forward, MovementVector.Y);
		AddMovementInput(Right, MovementVector.X);
	}
}

void AAscCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxis = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(LookAxis.X);
		AddControllerPitchInput(LookAxis.Y);
	}
}

void AAscCharacter::WalkPressed()  { bIsWalking = true; }
void AAscCharacter::WalkReleased() { bIsWalking = false; }

void AAscCharacter::CrouchPressed()
{
	if (GetCharacterMovement()->IsCrouching())
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AAscCharacter::FirePressed()
{
	bIsFiring = true;
	if (WeaponComponent)
	{
		WeaponComponent->Fire();
	}
}

void AAscCharacter::FireReleased()
{
	bIsFiring = false;
	if (WeaponComponent)
	{
		WeaponComponent->StopFiring();
	}
}
