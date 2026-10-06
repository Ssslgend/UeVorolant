#pragma once

#include "CoreMinimal.h"
#include "TrainerDuelTypes.generated.h"

UENUM(BlueprintType)
enum class ETrainerDuelState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Ready		UMETA(DisplayName = "Ready"),
	Peeking		UMETA(DisplayName = "Peeking"),
	Duel		UMETA(DisplayName = "Duel"),
	Success		UMETA(DisplayName = "Success"),
	Fail		UMETA(DisplayName = "Fail"),
	Result		UMETA(DisplayName = "Result")
};

/** Why a duel ended in failure. */
UENUM(BlueprintType)
enum class EDuelFailReason : uint8
{
	None		UMETA(DisplayName = "None"),
	BotFired	UMETA(DisplayName = "Bot Fired"),
	Timeout		UMETA(DisplayName = "Timeout"),
	Idle		UMETA(DisplayName = "Idle / No Action"),
	BadSpot		UMETA(DisplayName = "Bad Spot")
};

/** One duel outcome, broadcast on every READY -> RESULT transition. */
USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerDuelResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trainer|Duel")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Trainer|Duel")
	bool bHeadshot = false;

	/** T0 (first LOS frame) -> player shot, in ms. */
	UPROPERTY(BlueprintReadOnly, Category = "Trainer|Duel")
	float ReactionTimeMs = 0.0f;

	/** Closest approach of the last player shot to the bot head centre, in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Trainer|Duel")
	float MissDistanceCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Trainer|Duel")
	EDuelFailReason FailReason = EDuelFailReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Trainer|Duel")
	FString SpotId;

	UPROPERTY(BlueprintReadOnly, Category = "Trainer|Duel")
	FString SpotName;

	FString ToLogLine() const
	{
		return FString::Printf(
			TEXT("spot=%s result=%s headshot=%d reactionMs=%.1f missCm=%.2f reason=%d"),
			*SpotId, bSuccess ? TEXT("SUCCESS") : TEXT("FAIL"), bHeadshot ? 1 : 0,
			ReactionTimeMs, MissDistanceCm, static_cast<int32>(FailReason));
	}
};
