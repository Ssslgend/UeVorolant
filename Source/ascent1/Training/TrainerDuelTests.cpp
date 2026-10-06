#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Training/TrainerDuel.h"
#include "Training/TrainerDuelTypes.h"
#include "Training/TrainingSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainerDuelStateTransitionTest,
	"ascent1.Trainer.Duel.StateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerDuelStateTransitionTest::RunTest(const FString& Parameters)
{
	// Headshot only wins inside PEEKING/DUEL (real walk peek already started).
	TestEqual(TEXT("headshot in Duel -> Success"),
		ATrainerDuel::NextStateOnHeadshot(ETrainerDuelState::Duel), ETrainerDuelState::Success);
	TestEqual(TEXT("headshot in Peeking -> Success"),
		ATrainerDuel::NextStateOnHeadshot(ETrainerDuelState::Peeking), ETrainerDuelState::Success);
	TestEqual(TEXT("headshot in Ready is ignored"),
		ATrainerDuel::NextStateOnHeadshot(ETrainerDuelState::Ready), ETrainerDuelState::Ready);

	// Bot kill ends an active duel only.
	TestEqual(TEXT("bot kill in Duel -> Fail"),
		ATrainerDuel::NextStateOnBotKill(ETrainerDuelState::Duel), ETrainerDuelState::Fail);
	TestEqual(TEXT("bot kill in Peeking -> Fail"),
		ATrainerDuel::NextStateOnBotKill(ETrainerDuelState::Peeking), ETrainerDuelState::Fail);
	TestEqual(TEXT("bot kill in Ready is ignored"),
		ATrainerDuel::NextStateOnBotKill(ETrainerDuelState::Ready), ETrainerDuelState::Ready);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainerDuelLosTimerTest,
	"ascent1.Trainer.Duel.LosTimerGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerDuelLosTimerTest::RunTest(const FString& Parameters)
{
	// D1: READY must not arm the bot even if the spawn is accidentally exposed.
	TestFalse(TEXT("LOS in Ready does not start the clock"),
		ATrainerDuel::ShouldStartLosTimer(ETrainerDuelState::Ready, true));
	TestTrue(TEXT("LOS in Peeking starts the clock"),
		ATrainerDuel::ShouldStartLosTimer(ETrainerDuelState::Peeking, true));
	TestFalse(TEXT("no LOS never starts the clock"),
		ATrainerDuel::ShouldStartLosTimer(ETrainerDuelState::Peeking, false));
	TestFalse(TEXT("LOS in Duel does not restart (T0 is latched)"),
		ATrainerDuel::ShouldStartLosTimer(ETrainerDuelState::Duel, true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainerDuelTimingTest,
	"ascent1.Trainer.Duel.ReactionAndBotFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerDuelTimingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("reaction is shot - T0 in ms"),
		ATrainerDuel::ComputeReactionTimeMs(10.0f, 10.25f), 250.0f);
	TestEqual(TEXT("reaction clamps at zero"),
		ATrainerDuel::ComputeReactionTimeMs(10.25f, 10.0f), 0.0f);
	TestEqual(TEXT("missing T0 yields zero reaction"),
		ATrainerDuel::ComputeReactionTimeMs(-1.0f, 5.0f), 0.0f);

	FTrainerSettings Settings;
	Settings.Bot.DelayMs = 250.0f;
	Settings.Bot.WindupMs = 0.0f;

	TestFalse(TEXT("bot does not fire before delay"),
		ATrainerDuel::ShouldBotFire(1.0f, 1.20f, Settings));
	TestTrue(TEXT("bot fires at delay"),
		ATrainerDuel::ShouldBotFire(1.0f, 1.25f, Settings));

	Settings.Bot.WindupMs = 100.0f;
	TestFalse(TEXT("windup extends the budget"),
		ATrainerDuel::ShouldBotFire(1.0f, 1.25f, Settings));
	TestTrue(TEXT("bot fires after delay+windup"),
		ATrainerDuel::ShouldBotFire(1.0f, 1.35f, Settings));

	TestFalse(TEXT("no T0 means never fire"),
		ATrainerDuel::ShouldBotFire(-1.0f, 10.0f, Settings));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainerSettingsDefaultsTest,
	"ascent1.Trainer.Settings.DefaultsAndCm360",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	FTrainerSettings Settings;
	TestEqual(TEXT("default bot delay is 250ms"), Settings.Bot.DelayMs, 250.0f);
	TestEqual(TEXT("default head radius is 4cm"), Settings.Hit.HeadRadiusCm, 4.0f);
	TestEqual(TEXT("default run speed is 540"), Settings.Movement.RunSpeed, 540.0f);

	// cm/360 = 360 / (0.07 * sens * dpi) * 2.54
	const float Cm = FTrainerSettings::ComputeCmPer360(0.4f, 800.0f);
	const float Expected = 360.0f / (0.07f * 0.4f * 800.0f) * 2.54f;
	TestTrue(TEXT("cm/360 matches the requirement formula"), FMath::IsNearlyEqual(Cm, Expected, 0.01f));

	const FVector Floor(100.0f, 200.0f, 500.0f);
	const FVector Actor = Settings.FloorToActorLocation(Floor, false);
	TestTrue(TEXT("floor -> actor adds capsule half height"),
		FMath::IsNearlyEqual(Actor.Z, 500.0f + 91.5f, 0.1f));

	TestEqual(TEXT("invalid input yields zero cm/360"), FTrainerSettings::ComputeCmPer360(0.0f, 800.0f), 0.0f);
	return true;
}

#endif
