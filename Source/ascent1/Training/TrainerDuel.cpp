#include "TrainerDuel.h"
#include "../Player/AscCharacter.h"
#include "../Player/AscWeaponComponent.h"
#include "../Target/EnemyTarget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Kept as fallback offsets if settings are missing; settings own the real values.
	constexpr float HeadCenterAboveEyeCm = 11.0f;
	constexpr float TorsoBelowEyeCm = 35.0f;
}

ATrainerDuel::ATrainerDuel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
}

ETrainerDuelState ATrainerDuel::NextStateOnHeadshot(ETrainerDuelState Current)
{
	return (Current == ETrainerDuelState::Duel || Current == ETrainerDuelState::Peeking)
		? ETrainerDuelState::Success
		: Current;
}

ETrainerDuelState ATrainerDuel::NextStateOnBotKill(ETrainerDuelState Current)
{
	return (Current == ETrainerDuelState::Duel || Current == ETrainerDuelState::Peeking)
		? ETrainerDuelState::Fail
		: Current;
}

bool ATrainerDuel::ShouldStartLosTimer(ETrainerDuelState Current, bool bHasLos)
{
	// D1 is a real peek: the player must act before the bot may start its clock,
	// so an accidentally exposed spawn in READY never insta-kills the player.
	return bHasLos && Current == ETrainerDuelState::Peeking;
}

float ATrainerDuel::ComputeReactionTimeMs(float T0Seconds, float ShotSeconds)
{
	if (T0Seconds < 0.0f)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, (ShotSeconds - T0Seconds) * 1000.0f);
}

bool ATrainerDuel::ShouldBotFire(float T0Seconds, float NowSeconds, const FTrainerSettings& Settings)
{
	if (T0Seconds < 0.0f)
	{
		return false;
	}
	const float BudgetSeconds = (Settings.Bot.DelayMs + Settings.Bot.WindupMs) / 1000.0f;
	return (NowSeconds - T0Seconds) >= BudgetSeconds;
}

void ATrainerDuel::BeginDuel(AAscCharacter* InPlayer, AEnemyTarget* InBot, const FTrainerSettings& InSettings,
	const FString& InSpotId, const FString& InSpotName)
{
	Player = InPlayer;
	Bot = InBot;
	Settings = InSettings;

	LastResult = FTrainerDuelResult();
	LastResult.SpotId = InSpotId;
	LastResult.SpotName = InSpotName;

	ElapsedSinceReady = 0.0f;
	LosT0Seconds = -1.0f;
	ResultEnterSeconds = -1.0f;
	LastMissDistanceCm = 0.0f;
	bLosLatched = false;
	bPlayerActed = false;

	SetState(ETrainerDuelState::Ready);
}

void ATrainerDuel::ResetToReady()
{
	ElapsedSinceReady = 0.0f;
	LosT0Seconds = -1.0f;
	ResultEnterSeconds = -1.0f;
	LastMissDistanceCm = 0.0f;
	bLosLatched = false;
	bPlayerActed = false;
	SetState(ETrainerDuelState::Ready);

	// Unfreeze the weapon after the RESULT window (S2.2).
	if (Player)
	{
		if (UAscWeaponComponent* Weapon = Player->FindComponentByClass<UAscWeaponComponent>())
		{
			Weapon->SetFrozen(false);
		}
	}
}

void ATrainerDuel::NotifyPlayerMovedOrFired()
{
	if (State == ETrainerDuelState::Ready)
	{
		bPlayerActed = true;
		ElapsedSinceReady = 0.0f;
		SetState(ETrainerDuelState::Peeking);
	}
}

void ATrainerDuel::NotifyPlayerShot(bool bHeadHit, float MissDistanceCm)
{
	LastMissDistanceCm = MissDistanceCm;

	// A shot is itself a player action (prefire against a spot the bot has not seen yet).
	NotifyPlayerMovedOrFired();

	// Same-frame priority (S2.2): a headshot resolves before the bot kill that
	// would otherwise fire later this tick.
	if (State == ETrainerDuelState::Duel || State == ETrainerDuelState::Peeking)
	{
		const ETrainerDuelState Next = NextStateOnHeadshot(State);
		if (Next == ETrainerDuelState::Success && bHeadHit)
		{
			const float Reaction = (LosT0Seconds >= 0.0f)
				? ComputeReactionTimeMs(LosT0Seconds, GetWorld()->GetTimeSeconds())
				: ElapsedSinceReady * 1000.0f;
			EnterResult(true, true, EDuelFailReason::None, Reaction);
			return;
		}
	}

	// Non-head shots still record miss distance but do not end the duel.
}

void ATrainerDuel::AbortDuel(EDuelFailReason Reason)
{
	if (State == ETrainerDuelState::Ready || State == ETrainerDuelState::Peeking || State == ETrainerDuelState::Duel)
	{
		EnterResult(false, false, Reason, GetReactionTimeMs());
	}
}

float ATrainerDuel::GetReactionTimeMs() const
{
	const UWorld* World = GetWorld();
	if (!World || LosT0Seconds < 0.0f)
	{
		return 0.0f;
	}
	return ComputeReactionTimeMs(LosT0Seconds, World->GetTimeSeconds());
}

void ATrainerDuel::SetState(ETrainerDuelState NewState)
{
	if (State == NewState)
	{
		return;
	}
	const ETrainerDuelState OldState = State;
	State = NewState;
	OnDuelStateChanged.Broadcast(NewState, OldState);
}

void ATrainerDuel::EnterResult(bool bSuccess, bool bHeadshot, EDuelFailReason Reason, float ReactionTimeMs)
{
	const UWorld* World = GetWorld();
	ResultEnterSeconds = World ? World->GetTimeSeconds() : 0.0f;

	LastResult.bSuccess = bSuccess;
	LastResult.bHeadshot = bHeadshot;
	LastResult.ReactionTimeMs = ReactionTimeMs;
	LastResult.MissDistanceCm = LastMissDistanceCm;
	LastResult.FailReason = Reason;

	SetState(bSuccess ? ETrainerDuelState::Success : ETrainerDuelState::Fail);
	SetState(ETrainerDuelState::Result);

	UE_LOG(LogTemp, Log, TEXT("TrainerDuel: %s"), *LastResult.ToLogLine());
	OnDuelResult.Broadcast(LastResult);
}

bool ATrainerDuel::CheckLos() const
{
	UWorld* World = GetWorld();
	if (!Player || !Bot || !World)
	{
		return false;
	}

	const FVector BotEye = Bot->GetEyeWorldLocation();
	const FVector PlayerEye = Player->GetCameraLocation();
	const float EyeHeight = Settings.Hit.StandEyeHeightCm;
	const float CapsuleHalf = Settings.Hit.CapsuleHalfHeightCm;
	const FVector HeadTarget = Player->GetActorLocation() + FVector(0.0f, 0.0f, EyeHeight - CapsuleHalf + 11.0f);
	const FVector TorsoTarget = Player->GetActorLocation() + FVector(0.0f, 0.0f, Settings.Hit.TorsoHeightCm - CapsuleHalf);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainerLos), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(Bot);
	Params.AddIgnoredActor(Player);

	auto ClearTo = [&](const FVector& End) -> bool
	{
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, BotEye, End, ECC_Visibility, Params);
		return !bBlocked;
	};

	// Dual probe (4.2): head OR upper torso unobstructed means the bot sees the player.
	return ClearTo(HeadTarget) || ClearTo(TorsoTarget);
}

void ATrainerDuel::OnBotKill()
{
	// The requirement kills the player with no bullet simulation; reaction time is
	// the full budget the player failed to beat.
	const float BudgetMs = Settings.Bot.DelayMs + Settings.Bot.WindupMs;
	EnterResult(false, false, EDuelFailReason::BotFired, BudgetMs);
}

void ATrainerDuel::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	if (State == ETrainerDuelState::Idle)
	{
		return;
	}

	if (!Player || !Bot)
	{
		AbortDuel(EDuelFailReason::BadSpot);
		return;
	}

	// RESULT window: frozen until R (manual) or resetDelayMs (auto).
	if (State == ETrainerDuelState::Result)
	{
		if (Settings.Duel.bAutoReset && ResultEnterSeconds >= 0.0f &&
			(Now - ResultEnterSeconds) * 1000.0f >= Settings.Duel.ResetDelayMs)
		{
			ResetToReady();
		}
		return;
	}

	ElapsedSinceReady += DeltaTime;

	if (State == ETrainerDuelState::Ready)
	{
		// Idle guard: the player never acted.
		if (ElapsedSinceReady * 1000.0f >= Settings.Duel.TimeoutMs)
		{
			EnterResult(false, false, EDuelFailReason::Idle, 0.0f);
		}
		return;
	}

	// PEEKING / DUEL
	if (!bLosLatched && ShouldStartLosTimer(State, CheckLos()))
	{
		bLosLatched = true;
		LosT0Seconds = Now;
		SetState(ETrainerDuelState::Duel);
	}

	if (State == ETrainerDuelState::Duel && ShouldBotFire(LosT0Seconds, Now, Settings))
	{
		OnBotKill();
		return;
	}

	if (ElapsedSinceReady * 1000.0f >= Settings.Duel.TimeoutMs)
	{
		EnterResult(false, false, EDuelFailReason::Timeout, GetReactionTimeMs());
	}
}
