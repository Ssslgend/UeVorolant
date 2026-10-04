#if WITH_DEV_AUTOMATION_TESTS

#include "TrainingStats.h"
#include "TrainingHubBuilder.h"
#include "Engine/TextureCube.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingSessionStatsTest,
	"ascent1.Training.Stats.RecordShotSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingSessionStatsTest::RunTest(const FString& Parameters)
{
	FTrainingSession Session;
	Session.RecordShot(false, false, 0.0f);
	Session.RecordShot(true, false, 320.0f);
	Session.RecordShot(true, true, 320.0f);
	Session.RecordShot(true, true, 180.0f);

	const FTrainingSummary Summary = Session.GetSummary();
	TestEqual(TEXT("Total shots include misses"), Summary.TotalShots, 4);
	TestEqual(TEXT("Total hits count body and head hits"), Summary.TotalHits, 3);
	TestEqual(TEXT("Headshots count head hits"), Summary.Headshots, 2);
	TestEqual(TEXT("Hit rate is hits divided by shots"), Summary.HitRate, 75.0f);
	TestEqual(TEXT("Headshot rate is headshots divided by hits"), Summary.HeadshotRate, 66.666672f);
	TestEqual(TEXT("Duplicate reaction times are included in average"), Summary.AverageReactionTimeMs, 273.333344f);
	TestEqual(TEXT("Best reaction time is minimum hit reaction"), Summary.BestReactionTimeMs, 180.0f);

	Session.Reset();
	const FTrainingSummary ResetSummary = Session.GetSummary();
	TestEqual(TEXT("Reset clears shots"), ResetSummary.TotalShots, 0);
	TestEqual(TEXT("Reset clears hits"), ResetSummary.TotalHits, 0);
	TestEqual(TEXT("Reset clears average reaction"), ResetSummary.AverageReactionTimeMs, 0.0f);
	TestEqual(TEXT("Reset clears best reaction"), ResetSummary.BestReactionTimeMs, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingRegionFacingRotationTest,
	"ascent1.Training.RegionFacingRotation.FacesSpawnedMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingRegionFacingRotationTest::RunTest(const FString& Parameters)
{
	const FRotator Rotation = ATrainingHubBuilder::GetTrainingRegionFacingRotation();

	TestTrue(TEXT("Region spawn rotation yaw faces positive Y"), FMath::IsNearlyEqual(Rotation.Yaw, 90.0));
	TestTrue(TEXT("Region spawn rotation pitch is level"), FMath::IsNearlyEqual(Rotation.Pitch, 0.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingRegionGroundingTest,
	"ascent1.Training.RegionPlacement.GroundsImportedMeshBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingRegionGroundingTest::RunTest(const FString& Parameters)
{
	const FVector DesiredRegionLocation(1000.0, 2000.0, 0.0);
	const FBoxSphereBounds ImportedBounds(
		FVector(3.0, 18.0, 80.0),
		FVector(50.0, 49.0, 22.0),
		75.0);
	const float Scale = 100.0f;

	const FVector ActorLocation = ATrainingHubBuilder::GetGroundedRegionActorLocation(
		DesiredRegionLocation,
		ImportedBounds,
		Scale,
		70.0f);

	TestTrue(TEXT("Actor location offsets imported mesh center X"), FMath::IsNearlyEqual(ActorLocation.X, 700.0));
	TestTrue(TEXT("Actor location offsets imported mesh center Y"), FMath::IsNearlyEqual(ActorLocation.Y, 200.0));
	TestTrue(TEXT("Actor location puts gameplay source floor on region floor"), FMath::IsNearlyEqual(ActorLocation.Z, -7000.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingRegionPlayerSpawnBoundsTest,
	"ascent1.Training.RegionPlacement.PlayerSpawnStaysInsideRegionBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingRegionPlayerSpawnBoundsTest::RunTest(const FString& Parameters)
{
	const FBoxSphereBounds ImportedBounds(
		FVector(3.0, 18.0, 80.0),
		FVector(50.0, 49.0, 22.0),
		75.0);
	const float Scale = 100.0f;
	const FVector PlayerOffset = ATrainingHubBuilder::GetTrainingPlayerOffset();

	TestTrue(TEXT("Player X spawn is inside imported mesh horizontal bounds"),
		FMath::Abs(PlayerOffset.X) < ImportedBounds.BoxExtent.X * Scale);
	TestTrue(TEXT("Player Y spawn is inside imported mesh horizontal bounds"),
		FMath::Abs(PlayerOffset.Y) < ImportedBounds.BoxExtent.Y * Scale);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingRegionFallbackFloorTest,
	"ascent1.Training.RegionPlacement.FallbackFloorCoversImportedBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingRegionFallbackFloorTest::RunTest(const FString& Parameters)
{
	const FBoxSphereBounds ImportedBounds(
		FVector(0.0, 0.0, 80.0),
		FVector(50.0, 49.0, 22.0),
		75.0);
	const FVector FloorExtent = ATrainingHubBuilder::GetFallbackFloorExtent(ImportedBounds, 100.0f);

	TestTrue(TEXT("Fallback floor covers imported mesh X bounds"), FloorExtent.X >= 5000.0);
	TestTrue(TEXT("Fallback floor covers imported mesh Y bounds"), FloorExtent.Y >= 4900.0);
	TestTrue(TEXT("Fallback floor has blocking thickness"), FloorExtent.Z >= 100.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingRegionPlatformLocationTest,
	"ascent1.Training.RegionPlacement.PlatformTopMatchesRegionFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingRegionPlatformLocationTest::RunTest(const FString& Parameters)
{
	const FVector RegionLocation(100.0, 200.0, 0.0);
	const float PlatformHalfThickness = ATrainingHubBuilder::GetFallbackFloorExtent(
		FBoxSphereBounds(FVector::ZeroVector, FVector(50.0, 50.0, 20.0), 80.0),
		100.0f).Z;
	const FVector PlatformLocation = ATrainingHubBuilder::GetFallbackFloorLocation(RegionLocation);

	TestTrue(TEXT("Platform top is aligned with region floor"),
		FMath::IsNearlyEqual(PlatformLocation.Z + PlatformHalfThickness, RegionLocation.Z));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingLightingIntensityTest,
	"ascent1.Training.Lighting.UsesPlayableTrainingBrightness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingLightingIntensityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Sun intensity avoids blowing out the scene"), ATrainingHubBuilder::GetTrainingSunIntensity() <= 20.0f);
	TestTrue(TEXT("Sun intensity still lights the training area"), ATrainingHubBuilder::GetTrainingSunIntensity() >= 3.0f);
	TestTrue(TEXT("Skylight intensity avoids white fog"), ATrainingHubBuilder::GetTrainingSkyLightIntensity() <= 1.0f);
	TestTrue(TEXT("Skylight still provides ambient visibility"), ATrainingHubBuilder::GetTrainingSkyLightIntensity() >= 0.2f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingSkyLightCubemapTest,
	"ascent1.Training.Lighting.UsesLoadableAmbientCubemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingSkyLightCubemapTest::RunTest(const FString& Parameters)
{
	UTextureCube* AmbientCubemap = LoadObject<UTextureCube>(
		nullptr,
		ATrainingHubBuilder::GetTrainingSkyLightCubemapPath());

	TestNotNull(TEXT("Training skylight cubemap can be loaded"), AmbientCubemap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTrainingFallbackFloorVisibilityTest,
	"ascent1.Training.RegionPlacement.FallbackFloorHiddenByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFallbackFloorVisibilityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Fallback floor should not fill the camera view"), ATrainingHubBuilder::ShouldHideFallbackFloorInGame());
	return true;
}

#endif
