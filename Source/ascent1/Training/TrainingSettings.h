#pragma once

#include "CoreMinimal.h"
#include "TrainingSettings.generated.h"

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerMovementSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Movement")
	float RunSpeed = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Movement")
	float WalkMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Movement")
	float CrouchMultiplier = 0.63f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Movement")
	float JumpZVelocity = 370.0f;

	/** Valorant-style instant stop: no inertia, release key = stop this frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Movement")
	bool bInstantStop = true;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerHitSettings
{
	GENERATED_BODY()

	/** Head hit sphere radius in cm (requirement default 4, tunable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Hit")
	float HeadRadiusCm = 4.0f;

	/** Standing eye height above the floor, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Hit")
	float StandEyeHeightCm = 160.0f;

	/** Upper-torso height above the floor used by the bot LOS probe, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Hit")
	float TorsoHeightCm = 125.0f;

	/** Character capsule half height used to convert floor points to actor locations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Hit")
	float CapsuleHalfHeightCm = 91.5f;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerBotSettings
{
	GENERATED_BODY()

	/** Milliseconds from the first LOS frame until the bot kills the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Bot")
	float DelayMs = 250.0f;

	/** Optional tell animation before the kill; excluded from the reaction budget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Bot")
	float WindupMs = 0.0f;

	/** Randomize the bot between standing and crouching per duel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Bot")
	bool bRandomizeStance = false;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerDuelSettings
{
	GENERATED_BODY()

	/** No-action / no-LOS timeout for one duel, in ms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Duel")
	float TimeoutMs = 30000.0f;

	/** How long the RESULT screen stays before auto reset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Duel")
	float ResetDelayMs = 1500.0f;

	/** When false the result screen waits for R instead of auto resetting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Duel")
	bool bAutoReset = true;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerMapSettings
{
	GENERATED_BODY()

	/** Static mesh holding the playable Ascent geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Map")
	FString MeshPath = TEXT("/Game/Ascent/AscentPlayable.AscentPlayable");

	/** Actor scale for the map mesh. 0 = auto detect (meters -> 100, cm -> 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Map")
	float MeshScale = 0.0f;

	/** Ground snap probe distance in cm (ray starts this far above the point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Map")
	float GroundProbeHeightCm = 500.0f;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerInputSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Input")
	bool bDisableSmoothing = true;

	/** Valorant sensitivity multiplier; degrees per mouse count = 0.07 * sens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Input")
	float ValorantSensitivity = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer|Input")
	float MouseDpi = 800.0f;
};

USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
	FTrainerMovementSettings Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
	FTrainerHitSettings Hit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
	FTrainerBotSettings Bot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
	FTrainerDuelSettings Duel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
	FTrainerMapSettings Map;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
	FTrainerInputSettings Input;

	/** Degrees of view rotation per mouse count for Valorant, per the requirement doc. */
	static constexpr float ValorantDegreesPerCount = 0.07f;

	/** cm/360 helper for cross checking the sensitivity against real hardware. */
	static float ComputeCmPer360(float Sensitivity, float Dpi);

	/** Shift a floor point to a capsule-center actor location. */
	FVector FloorToActorLocation(const FVector& FloorPoint, bool bCrouched) const;

	/** Auto/configured map mesh scale from the mesh's largest bound extent (cm). */
	static float ResolveMeshScale(float MeshMaxExtent, float ConfiguredScale);

	/** Clamp every value into a playable range; returns the number of fields changed. */
	int32 Validate();
};

/**
 * Loads trainer gameplay knobs from Content/Data/trainer_settings.json.
 * Missing file or fields fall back to defaults so the game stays playable.
 */
UCLASS()
class ASCENT1_API UTrainingSettings : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Trainer|Settings")
	static FTrainerSettings Load();

	UFUNCTION(BlueprintCallable, Category = "Trainer|Settings")
	static FString GetSettingsFilePath();

	/** Parses a JSON document into Out (starting from defaults) and validates it. */
	static void ApplyTo(FTrainerSettings& Out, const FString& JsonString);
};
