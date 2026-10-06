#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/AscWeaponComponent.h"
#include "TrainerSession.generated.h"
class AEnemyTarget;
class AAscCharacter;
class UAscWeaponComponent;
class UTrainingSaveGame;
class AStaticMeshActor;

struct FTrainerHitFeedback
{
    EHitRegion Region = EHitRegion::Miss;
    FVector WorldImpact = FVector::ZeroVector;
    float Damage = 0;
    float RemainingHealth = 100;
    float HitRemaining = 0;
    float KillRemaining = 0;
    bool bKilled = false;
    bool bHeadshotKill = false;
};

UENUM(BlueprintType)
enum class ECombatPracticeMode : uint8 { Explore, PreAim, Hold, Flash, Defense, Combat };

USTRUCT()
struct FDefenseApproach
{
    GENERATED_BODY()
    UPROPERTY() FVector Hide = FVector::ZeroVector;
    UPROPERTY() FVector Peek = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FCombatTrainingPoint
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Region;
    UPROPERTY(BlueprintReadOnly) FVector PlayerFloor = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector EnemyFloor = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector PeekFloor = FVector::ZeroVector;
    UPROPERTY() TArray<FDefenseApproach> DefenseApproaches;
};

/** Owns a local practice session. Geometry and route positions remain in native map space. */
UCLASS()
class ASCENT1_API ATrainerSession : public AActor
{
    GENERATED_BODY()
public:
    ATrainerSession();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) bool Start(int32 ModeIndex, const FString& Region, int32 DifficultyIndex, int32 PointIndex = 0);
    UFUNCTION(BlueprintCallable) void Restart();
    UFUNCTION(BlueprintCallable) void Next();
    UFUNCTION(BlueprintCallable) void Stop();
    UFUNCTION(BlueprintCallable) void Pause(bool Paused);
    UFUNCTION(BlueprintCallable) void CapturePlayer();
    UFUNCTION(BlueprintCallable) void CaptureEnemy();
    UFUNCTION(BlueprintCallable) bool SavePersonalPoint();
    UFUNCTION(BlueprintPure) bool IsRunning() const { return bRunning; }
    UFUNCTION(BlueprintPure) bool IsFinished() const { return bFinished; }
    UFUNCTION(BlueprintPure) bool IsPaused() const { return bPaused; }
    UFUNCTION(BlueprintPure) float GetFlashAlpha() const { return FlashAlpha; }
    UFUNCTION(BlueprintPure) FString GetStatus() const { return Status; }
    UFUNCTION(BlueprintPure) FVector GetTargetFloor() const;
    UFUNCTION(BlueprintPure) FString GetPointName() const;
    UFUNCTION(BlueprintPure) FString GetModeName() const { return ModeName(static_cast<int32>(Mode)); }
    const TArray<FCombatTrainingPoint>& GetPoints() const { return Points; }
    AEnemyTarget* GetCurrentTarget() const { return Bot; }
    const FTrainerHitFeedback& GetHitFeedback() const { return HitFeedback; }
    float GetPlayerHurtAlpha() const { return PlayerHurtAlpha; }
    float GetHoldReactionRemainingMs() const;
    bool IsHoldReactionStarted() const { return HoldExposureAt>=0; }
    float GetHoldReactionElapsedMs() const;
    FVector GetCurrentPeekFloor() const { return Selection.IsValidIndex(SelectedPoint)?Points[Selection[SelectedPoint]].PeekFloor:FVector::ZeroVector; }
    FVector GetCurrentPlayerFloor() const { return Selection.IsValidIndex(SelectedPoint)?Points[Selection[SelectedPoint]].PlayerFloor:FVector::ZeroVector; }
    bool HasFlashFired() const { return bFlashFired; }
    static FString ModeName(int32 Index);
    static float ReactionBudget(int32 Difficulty);
    static float ReactionBudgetForMode(int32 ModeIndex, int32 Difficulty);
    static bool IsHoldDeadlineExpired(double VisibleSeconds);
    static float FlashStrength(float FacingDot, float Distance, bool Visible);
    static bool IsRoundComplete(int32 Kills, int32 Required);
    static bool IsShotScorable(int32 ModeIndex, bool FlashFired);
    static bool RaySphere(const FVector& Origin, const FVector& Direction, const FVector& Center, float Radius, float& Distance);
    UPROPERTY(BlueprintReadOnly) ECombatPracticeMode Mode = ECombatPracticeMode::Explore;
    UPROPERTY(BlueprintReadOnly) int32 Difficulty = 1;
    UPROPERTY(BlueprintReadOnly) int32 Shots = 0;
    UPROPERTY(BlueprintReadOnly) int32 Hits = 0;
    UPROPERTY(BlueprintReadOnly) int32 Headshots = 0;
    UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(BlueprintReadOnly) int32 RequiredKills = 1;
    UPROPERTY(BlueprintReadOnly) int32 Health = 100;
    UPROPERTY(BlueprintReadOnly) float LastReactionMs = 0;
    UPROPERTY(BlueprintReadOnly) float BestReactionMs = 0;
    UPROPERTY(BlueprintReadOnly) float LastShotSpeed = 0;
    UPROPERTY(BlueprintReadOnly) float AverageAimErrorDegrees = 0;
    UPROPERTY(BlueprintReadOnly) int32 StoppedShots = 0;
    UPROPERTY(BlueprintReadOnly) FString SelectedRegion = TEXT("All");
    UPROPERTY(BlueprintReadOnly) int32 SelectedPoint = 0;
    UPROPERTY(BlueprintReadOnly) bool bWon = false;
private:
    UPROPERTY() TArray<FCombatTrainingPoint> Points;
    TArray<int32> Selection;
    UPROPERTY() AEnemyTarget* Bot = nullptr;
    UPROPERTY() AAscCharacter* Player = nullptr;
    UPROPERTY() UAscWeaponComponent* Weapon = nullptr;
    UPROPERTY() UTrainingSaveGame* History = nullptr;
    UPROPERTY() AStaticMeshActor* FlashActor = nullptr;
    FDelegateHandle ShotHandle;
    bool bRunning = false, bPaused = false, bFinished = false, bFlashFired = false;
    bool bReactionRecorded = false;
    int32 AimSamples = 0;
    int32 LastDefenseApproach = -1;
    float Elapsed = 0, WaveElapsed = 0, BotRevealDelay = 1, LosSince = -1, BotLastShot = -1;
    float FlashAlpha = 0, FlashRemaining = 0, BotHealth = 100;
    FVector BotHide, BotPeek, FlashSource;
    FVector CapturedPlayer, CapturedEnemy;
    bool bHasPlayerCapture = false, bHasEnemyCapture = false;
    TArray<float> Reactions;
    FString Status;
    FTrainerHitFeedback HitFeedback;
    float PlayerHurtAlpha = 0;
    double HoldExposureAt = -1, PauseWorldAt = 0;
    void LoadPoints();
    bool PlacePlayer(const FVector& Floor, const FVector& Facing);
    bool Grounded(const FVector& Floor) const;
    bool CaptureFloor(FVector& Floor) const;
    bool ClearSight(const FVector& From, const FVector& To) const;
    void SpawnWave();
    void Finish(bool Won, const FString& Reason);
    void StoreRecord(bool Won);
    void Shot(const FVector& Start, const FVector& End, const FVector& Impact, bool HitWorld);
    bool CanSeeHoldTarget() const;
    void EnemyShot(bool Lethal);
};
