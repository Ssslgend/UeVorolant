#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpotData.h"
#include "Training/TrainerDuelTypes.h"
#include "PrefireArena.generated.h"

class AAscCharacter;
class AEnemyTarget;
class AEnemySpawner;
class ATrainerDuel;

USTRUCT()
struct FSpotResolution
{
	GENERATED_BODY()

	FVector PlayerPos = FVector::ZeroVector;
	FVector EnemyPos = FVector::ZeroVector;
	FRotator PlayerRot = FRotator::ZeroRotator;
	FRotator EnemyRot = FRotator::ZeroRotator;
	bool bPlayerGrounded = false;
	bool bEnemyGrounded = false;
};

UCLASS()
class ASCENT1_API APrefireArena : public AActor
{
	GENERATED_BODY()

public:
	APrefireArena();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void LoadAndBuild(const FString& JsonPath);

	/** Spawn the user-exported AscentPlayable mesh + basic lighting if missing. */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	bool EnsurePlayableMap();

	/**
	 * Mesh import re-centers geometry; spots use Blender absolute cm.
	 * Translate the map so a probe above the first spot lands on its floor Z.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	bool AlignMapToSpots(const TArray<FPrefireSpot>& InSpots);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void StartSpot(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void BotFire();

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void OnPlayerHitBot(AAscCharacter* Player, const FVector& HitPoint, bool bHeadHit, float DistanceToHeadCm);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void NextSpot();

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void RestartSpot();

	UFUNCTION(BlueprintCallable, Category = "Arena")
	int32 GetSpotCount() const { return Spots.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Arena")
	int32 GetCurrentSpotIndex() const { return CurrentSpotIndex; }

	UFUNCTION(BlueprintCallable, Category = "Arena")
	ETrainerDuelState GetDuelState() const;

	UFUNCTION(BlueprintCallable, Category = "Arena")
	ATrainerDuel* GetActiveDuel() const { return ActiveDuel; }

	/** Resolves one weapon ray against the active bot and feeds TrainerDuel. */
	void HandleWeaponShot(const FVector& RayStart, const FVector& RayEnd, const FVector& ImpactPoint, bool bHitWorld);

	/** Ground-snap helper used by loader path. Exposed for tests. */
	static bool ResolveSpotTransforms(UWorld* World, const FPrefireSpot& Spot, FSpotResolution& Out);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpotCompleted, int32, SpotIndex);
	UPROPERTY(BlueprintAssignable)
	FOnSpotCompleted OnSpotCompleted;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerKilled, float, ReactionTimeMs);
	UPROPERTY(BlueprintAssignable)
	FOnPlayerKilled OnPlayerKilled;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSpotResult, int32, SpotIndex, bool, bSuccess, float, ReactionTimeMs);
	// Note: full outcome (miss distance / fail reason) is on ATrainerDuel::OnDuelResult.
	UPROPERTY(BlueprintAssignable)
	FOnSpotResult OnSpotResult;

protected:
	virtual void BeginPlay() override;

	void SpawnCoverWall(const FVector& PlayerPos, const FVector& EnemyPos);
	void TeleportPlayer(const FVector& Location, const FRotator& Rotation);
	void ClearCurrentSpot();

	UFUNCTION()
	void HandleDuelResult(FTrainerDuelResult Result);

	void BindWeaponShot();
	void UnbindWeaponShot();

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Bot")
	float BotFireDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Cover")
	TSubclassOf<AActor> CoverWallClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Cover")
	float CoverDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Cover")
	float CoverHeight = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Spots")
	FString DefaultSpotFile = TEXT("trainer_spots_v1.json");

	TArray<FPrefireSpot> Spots;
	int32 CurrentSpotIndex = -1;

	UPROPERTY()
	AEnemyTarget* ActiveBot = nullptr;

	UPROPERTY()
	AActor* ActiveCoverWall = nullptr;

	UPROPERTY()
	ATrainerDuel* ActiveDuel = nullptr;

	FTimerHandle BotFireTimerHandle;
	float SpotStartTime = 0.0f;

	TArray<FString> LoadWarnings;
};
