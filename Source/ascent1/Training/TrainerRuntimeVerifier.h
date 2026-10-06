#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainerRuntimeVerifier.generated.h"

class AAscPlayerController;
class AAscCharacter;
class UAscWeaponComponent;
class ATrainerSession;
class FJsonObject;
class FJsonValue;

/** Opt-in, real-world integration driver. Spawn only for -TrainerVerify. */
UCLASS()
class ASCENT1_API ATrainerRuntimeVerifier : public AActor
{
    GENERATED_BODY()
public:
    ATrainerRuntimeVerifier();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    enum class EStage : uint8
    {
        AwaitWorld, GroundAndExplore, ExploreWalk, PreAimOcclusion, PreAimWalk,
        LimbShots, BodyShots, HeadWin, PostRoundWalk, FeedbackFade, FailedRound, PauseHold, HoldWin, FlashFace, FlashBack,
        Defense, CombatHurt, Combat, MapWalk, Persistence, Done
    };
    struct FFileSnapshot
    {
        FString Path;
        TArray<uint8> Bytes;
        bool Existed = false;
    };
    UPROPERTY() AAscPlayerController* Controller = nullptr;
    UPROPERTY() AAscCharacter* Player = nullptr;
    UPROPERTY() UAscWeaponComponent* Weapon = nullptr;
    UPROPERTY() ATrainerSession* Session = nullptr;
    EStage Stage = EStage::AwaitWorld;
    double StartedAt = 0, StageAt = 0, AimAt = 0;
    int32 Frame = 0, StageFrame = 0, AimedFrame = -1, PreAimPoint = 0, BodyFired = 0;
    int32 LastKills = 0, HistoryTotalBefore = 0, PauseShots = 0, PauseHealth = 0;
    int32 ExpectedHistoryRecords = 0;
    int32 FacingFlashFrame = -1;
    int32 TargetPreviewFrame = -1;
    int32 BodyPreviewFrame = -1, CombatBodyShots = 0;
    bool bDone = false, bRestored = false, bSnapshotReady = false;
    bool bSawMovement = false, bHoldMoved = false, bCombatContinuous = true;
    bool bPreFlashChecked = false, bFlashTurning = false;
    bool bAllGrounded = true;
    float FacingFlashAlpha = -1, BackFlashAlpha = -1;
    float SavedMaxWalkSpeed = 540;
    FVector WalkStart, PauseBot, WaveBotStart, FlashSource, CombatAfterKill;
    TArray<FVector> DefensePositions;
    TArray<FVector> WalkWaypoints;
    TArray<int32> WalkRouteEnds;
    TArray<FString> WalkRouteNames;
    int32 WalkWaypointIndex = 0, WalkRouteIndex = 0;
    double WalkWaypointAt = 0;
    TArray<TSharedPtr<FJsonValue>> Cases;
    TArray<FFileSnapshot> Snapshots;
    void Enter(EStage Next);
    void Record(const FString& Name, bool Passed, const FString& Evidence);
    bool StartMode(int32 Mode, const FString& Region = TEXT("All"), int32 Point = 0);
    bool TraceGround(const FVector& Floor, FString& Evidence) const;
    bool HasSight(const FVector& From, const FVector& To, FString* Blocker = nullptr) const;
    bool GroundPlayer() const;
    bool WalkToward(const FVector& Floor, float DeltaSeconds, bool StopOnSight);
    bool FireAtTarget(bool Body = false, int32 Limb = INDEX_NONE);
    bool TeleportFixture(const FVector& Floor);
    FVector FindFlashSource() const;
    void BeginFlash(bool Back);
    void DriveFlash(bool Back);
    void DriveWaves(bool Combat);
    bool SnapshotFiles();
    void AccountEndingSession();
    void RestoreFiles();
    void Finish(const FString& Reason);
};
