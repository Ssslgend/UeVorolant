#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainerDuelTypes.h"
#include "TrainingSettings.h"
#include "TrainerDuel.generated.h"

class AAscCharacter;
class AEnemyTarget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDuelStateChanged, ETrainerDuelState, NewState, ETrainerDuelState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDuelResult, FTrainerDuelResult, Result);

/**
 * Single-peek duel state machine (requirement doc 1.3 / D1 / D2).
 *
 * READY --(player moves or fires)--> PEEKING --(bot eye has LOS)--> DUEL
 * DUEL  --(player headshot)--> SUCCESS      DUEL --(T0 + delayMs)--> FAIL
 * SUCCESS/FAIL --> RESULT --(R or resetDelayMs)--> READY
 *
 * T0 is the first frame the bot eye has an unobstructed trace to the player head
 * OR the player torso. T0 is latched: re-occluding the player never restarts it.
 */
UCLASS()
class ASCENT1_API ATrainerDuel : public AActor
{
	GENERATED_BODY()

public:
	ATrainerDuel();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	void BeginDuel(AAscCharacter* InPlayer, AEnemyTarget* InBot, const FTrainerSettings& InSettings, const FString& InSpotId, const FString& InSpotName);

	/** Back to READY with a cleared timer; used by R and by the arena on spot change. */
	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	void ResetToReady();

	/** Player started moving or fired: READY -> PEEKING. */
	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	void NotifyPlayerMovedOrFired();

	/** A resolved player shot. Head hits win inside PEEKING/DUEL. */
	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	void NotifyPlayerShot(bool bHeadHit, float MissDistanceCm);

	/** Force a failure (bad spot, missing bot, ...). */
	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	void AbortDuel(EDuelFailReason Reason);

	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	ETrainerDuelState GetState() const { return State; }

	/** Seconds since T0, in ms; 0 before LOS. */
	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	float GetReactionTimeMs() const;

	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	float GetLastMissDistanceCm() const { return LastMissDistanceCm; }

	/** True while the result screen is up: player weapon and bot are frozen. */
	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	bool IsFrozen() const { return State == ETrainerDuelState::Result || State == ETrainerDuelState::Success || State == ETrainerDuelState::Fail; }

	/** True once the bot can see the player (T0 latched). */
	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	bool HasLatchedLos() const { return bLosLatched; }

	UFUNCTION(BlueprintCallable, Category = "Trainer|Duel")
	const FTrainerDuelResult& GetLastResult() const { return LastResult; }

	// ---- Pure helpers (unit tested, no world needed) ----

	/** Next state after a headshot resolved in Current. */
	static ETrainerDuelState NextStateOnHeadshot(ETrainerDuelState Current);
	/** Next state after the bot kill lands in Current. */
	static ETrainerDuelState NextStateOnBotKill(ETrainerDuelState Current);
	/** Whether an unobstructed trace in Current may latch T0. */
	static bool ShouldStartLosTimer(ETrainerDuelState Current, bool bHasLos);
	/** Reaction time in ms between the latched T0 and the shot, clamped at 0. */
	static float ComputeReactionTimeMs(float T0Seconds, float ShotSeconds);
	/** True when the bot should fire: delay + windup elapsed since T0. */
	static bool ShouldBotFire(float T0Seconds, float NowSeconds, const FTrainerSettings& Settings);

	UPROPERTY(BlueprintAssignable, Category = "Trainer|Duel")
	FOnDuelStateChanged OnDuelStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Trainer|Duel")
	FOnDuelResult OnDuelResult;

protected:
	void SetState(ETrainerDuelState NewState);
	void EnterResult(bool bSuccess, bool bHeadshot, EDuelFailReason Reason, float ReactionTimeMs);
	bool CheckLos() const;
	void OnBotKill();

	UPROPERTY()
	AAscCharacter* Player = nullptr;

	UPROPERTY()
	AEnemyTarget* Bot = nullptr;

	FTrainerSettings Settings;
	ETrainerDuelState State = ETrainerDuelState::Idle;
	FTrainerDuelResult LastResult;

	float ElapsedSinceReady = 0.0f;
	float LosT0Seconds = -1.0f;
	float ResultEnterSeconds = -1.0f;
	float LastMissDistanceCm = 0.0f;
	bool bLosLatched = false;
	bool bPlayerActed = false;
};
