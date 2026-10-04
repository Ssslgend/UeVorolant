#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainingHubBuilder.generated.h"

class AEnemyTarget;
class UBoxComponent;

USTRUCT()
struct FTrainingRegionDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FName RegionName;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FSoftObjectPath MeshPath;

	UPROPERTY()
	float SourceFloorZ = 0.0f;
};

UCLASS()
class ASCENT1_API ATrainingHubBuilder : public AActor
{
	GENERATED_BODY()

public:
	ATrainingHubBuilder();

	virtual void BeginPlay() override;

	static FRotator GetTrainingRegionFacingRotation();
	static FVector GetGroundedRegionActorLocation(
		const FVector& RegionLocation,
		const FBoxSphereBounds& MeshBounds,
		float MeshScale,
		float SourceFloorZ);
	static FVector GetTrainingPlayerOffset();
	static FVector GetFallbackFloorExtent(const FBoxSphereBounds& MeshBounds, float MeshScale);
	static FVector GetFallbackFloorLocation(const FVector& RegionLocation);
	static float GetTrainingSunIntensity();
	static float GetTrainingSkyLightIntensity();
	static const TCHAR* GetTrainingSkyLightCubemapPath();
	static bool ShouldHideFallbackFloorInGame();

protected:
	UPROPERTY(EditAnywhere, Category = "Training")
	float RegionScale = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Training")
	float PlayerDistance = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Training")
	float TargetNearDistance = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Training")
	float TargetDeepDistance = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Training")
	TSubclassOf<AEnemyTarget> TargetClass;

private:
	TArray<FTrainingRegionDefinition> BuildRegionDefinitions() const;
	void SpawnRegion(const FTrainingRegionDefinition& Region);
	void SpawnTarget(const FVector& Location, const FString& Label);
	void SpawnFallbackFloor(const FTrainingRegionDefinition& Region, const FBoxSphereBounds& MeshBounds);
	void SpawnLighting();
	void MovePlayerToFirstRegion(const TArray<FTrainingRegionDefinition>& Regions);
};
