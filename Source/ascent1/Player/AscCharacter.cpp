#include "AscCharacter.h"
#include "AscWeaponComponent.h"
#include "../Core/AscGameMode.h"
#include "../Map/PrefireArena.h"
#include "../Training/TrainingSettings.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"

AAscCharacter::AAscCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	// Eye at 160cm from floor; capsule center is at +91.5cm
	Camera->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f - 91.5f));
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
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;

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
	ApplyPreferences(UTrainingPreferences::Load());

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
		TargetSpeed *= WalkMultiplier;
	}
	if (GetCharacterMovement()->IsCrouching())
	{
		TargetSpeed *= CrouchMultiplier;
	}

	CurrentMoveSpeed = TargetSpeed;
	GetCharacterMovement()->MaxWalkSpeed = CurrentMoveSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CurrentMoveSpeed;
}

void AAscCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	NormalizeMouseInput();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move) EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AAscCharacter::Move);
		if (IA_Look) EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AAscCharacter::Look);
		if (IA_Walk)
		{
			EnhancedInput->BindAction(IA_Walk, ETriggerEvent::Started, this, &AAscCharacter::WalkPressed);
			EnhancedInput->BindAction(IA_Walk, ETriggerEvent::Completed, this, &AAscCharacter::WalkReleased);
		}
		if (IA_CrouchToggle) EnhancedInput->BindAction(IA_CrouchToggle, ETriggerEvent::Started, this, &AAscCharacter::CrouchPressed);
		if (IA_Jump) EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
		if (IA_Fire)
		{
			EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &AAscCharacter::FirePressed);
			EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Completed, this, &AAscCharacter::FireReleased);
		}
		if (IA_Reset)
		{
			EnhancedInput->BindAction(IA_Reset, ETriggerEvent::Started, this, &AAscCharacter::ResetDuel);
		}
	}

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AAscCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AAscCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AAscCharacter::Turn);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AAscCharacter::LookUp);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction(TEXT("Walk"), IE_Pressed, this, &AAscCharacter::WalkPressed);
	PlayerInputComponent->BindAction(TEXT("Walk"), IE_Released, this, &AAscCharacter::WalkReleased);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AAscCharacter::CrouchPressed);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AAscCharacter::FirePressed);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &AAscCharacter::FireReleased);
	PlayerInputComponent->BindAction(TEXT("Reset"), IE_Pressed, this, &AAscCharacter::ResetDuel);
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
		// Enhanced mouse look uses raw counts; legacy Turn/LookUp already receive the 0.07 axis scale.
		AddControllerYawInput(LookAxis.X * FTrainerSettings::ValorantDegreesPerCount * MouseSensitivity);
		AddControllerPitchInput(LookAxis.Y * FTrainerSettings::ValorantDegreesPerCount * MouseSensitivity);
	}
}

void AAscCharacter::MoveForward(float Value)
{
	if (Controller && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
	}
}

void AAscCharacter::MoveRight(float Value)
{
	if (Controller && FMath::Abs(Value) > KINDA_SMALL_NUMBER)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
	}
}

void AAscCharacter::Turn(float Value)
{
	AddControllerYawInput(Value * MouseSensitivity);
}

void AAscCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value * MouseSensitivity);
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

FVector AAscCharacter::GetCameraLocation() const
{
	return Camera ? Camera->GetComponentLocation() : GetActorLocation();
}

void AAscCharacter::ResetDuel()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			// Prefer game-mode restart so the whole spot (bot + duel) is rebuilt.
			if (AGameModeBase* GM = World->GetAuthGameMode())
			{
				if (GM->IsA<AAscGameMode>())
				{
					Cast<AAscGameMode>(GM)->RestartCurrentSpot();
					return;
				}
			}
		}
	}

	// Fallback: ask any arena in the world.
	for (TActorIterator<APrefireArena> It(GetWorld()); It; ++It)
	{
		It->RestartSpot();
		return;
	}
}

void AAscCharacter::ApplyTrainerSettings(const FTrainerSettings& Settings)
{
	RifleSpeed = Settings.Movement.RunSpeed;
	WalkMultiplier = Settings.Movement.WalkMultiplier;
	CrouchMultiplier = Settings.Movement.CrouchMultiplier;
	WalkSpeed = Settings.Movement.RunSpeed * Settings.Movement.WalkMultiplier;
	CrouchSpeed = Settings.Movement.RunSpeed * Settings.Movement.CrouchMultiplier;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement)
	{
		Movement->JumpZVelocity = Settings.Movement.JumpZVelocity;
		Movement->MaxWalkSpeed = RifleSpeed;
		if (Settings.Movement.bInstantStop)
		{
			Movement->MaxAcceleration = 100000.0f;
			Movement->BrakingDecelerationWalking = 100000.0f;
			Movement->GroundFriction = 100.0f;
		}
	}

	NormalizeMouseInput();

	if (Camera)
	{
		Camera->SetRelativeLocation(FVector(0.0f, 0.0f, Settings.Hit.StandEyeHeightCm - Settings.Hit.CapsuleHalfHeightCm));
	}
}

void AAscCharacter::ApplyPreferences(const FTrainerPreferences& Preferences)
{
	FString Error;
	if (!UTrainingPreferences::Validate(Preferences, Error)) return;
	MouseSensitivity = Preferences.Sensitivity;
	NormalizeMouseInput();
}

void AAscCharacter::NormalizeMouseInput()
{
	// PlayerInput reads these flags from InputSettings each frame. Disable all hidden mouse scaling.
	UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
	InputSettings->bEnableMouseSmoothing = false;
	InputSettings->bEnableFOVScaling = false;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		PC->SetDeprecatedInputYawScale(1.0f);
		PC->SetDeprecatedInputPitchScale(1.0f);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		if (UPlayerInput* Input = PC->PlayerInput)
		{
			FInputAxisProperties Axis;
			Axis.DeadZone = 0.0f;
			Axis.Exponent = 1.0f;
			Axis.Sensitivity = FTrainerSettings::ValorantDegreesPerCount;
			Axis.bInvert = false;
			Input->SetAxisProperties(EKeys::MouseX, Axis);
			Input->SetAxisProperties(EKeys::MouseY, Axis);
			Input->SetAxisProperties(EKeys::Mouse2D, Axis);
			Input->ClearSmoothing();
		}
	}
}
