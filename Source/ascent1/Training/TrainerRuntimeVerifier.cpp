#include "TrainerRuntimeVerifier.h"
#include "TrainerSession.h"
#include "TrainingPreferences.h"
#include "TrainingSaveGame.h"
#include "Core/AscPlayerController.h"
#include "Player/AscCharacter.h"
#include "Player/AscWeaponComponent.h"
#include "Player/TrainerShotTracer.h"
#include "Target/EnemyTarget.h"
#include "Target/TrainerImpactEffect.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

ATrainerRuntimeVerifier::ATrainerRuntimeVerifier()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ATrainerRuntimeVerifier::BeginPlay()
{
    Super::BeginPlay();
    StartedAt = StageAt = FPlatformTime::Seconds();
#if UE_BUILD_SHIPPING
    SetActorTickEnabled(false);
#else
    if (!FParse::Param(FCommandLine::Get(), TEXT("TrainerVerify")))
    {
        SetActorTickEnabled(false);
        return;
    }
    bSnapshotReady = SnapshotFiles();
    if (!bSnapshotReady) Finish(TEXT("Could not safely snapshot existing user save files."));
#endif
}

void ATrainerRuntimeVerifier::Enter(EStage Next)
{
    Stage = Next;
    StageAt = FPlatformTime::Seconds();
    StageFrame = Frame;
    AimedFrame = -1;
    UE_LOG(LogTemp, Display, TEXT("[TrainerVerify] stage=%d"), static_cast<int32>(Next));
}

void ATrainerRuntimeVerifier::Record(const FString& Name, bool Passed, const FString& Evidence)
{
    auto Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("Name"), Name);
    Item->SetBoolField(TEXT("Passed"), Passed);
    Item->SetStringField(TEXT("Evidence"), Evidence);
    Item->SetNumberField(TEXT("WallSeconds"), FPlatformTime::Seconds() - StartedAt);
    Cases.Add(MakeShared<FJsonValueObject>(Item));
    UE_LOG(LogTemp, Display, TEXT("[TrainerVerify] %s=%s %s"), *Name, Passed ? TEXT("PASS") : TEXT("FAIL"), *Evidence);
}

bool ATrainerRuntimeVerifier::SnapshotFiles()
{
    for (const FString& Path : {
        UTrainingPreferences::GetPreferencesFilePath(),
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Trainer/personal_points.json")),
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames/TrainingData.sav"))})
    {
        FFileSnapshot Snapshot;
        Snapshot.Path = Path;
        Snapshot.Existed = IFileManager::Get().FileExists(*Path);
        if (Snapshot.Existed && !FFileHelper::LoadFileToArray(Snapshot.Bytes, *Path)) return false;
        Snapshots.Add(MoveTemp(Snapshot));
    }
    HistoryTotalBefore = UTrainingSaveGame::Load()->TotalSessionsCompleted;
    return true;
}

void ATrainerRuntimeVerifier::RestoreFiles()
{
    if (bRestored || !bSnapshotReady) return;
    if (Controller && Controller->IsMenuOpen() && Session && Session->IsRunning()) Controller->ToggleMenu();
    if (Session) Session->Stop();
    if (Controller) Controller->SetPause(false);
    bool Restored = true;
    for (const auto& Snapshot : Snapshots)
    {
        if (Snapshot.Existed)
        {
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Snapshot.Path), true);
            Restored &= FFileHelper::SaveArrayToFile(Snapshot.Bytes, *Snapshot.Path);
        }
        else if (IFileManager::Get().FileExists(*Snapshot.Path))
            Restored &= IFileManager::Get().Delete(*Snapshot.Path, false, true);
    }
    bRestored = true;
    Record(TEXT("UserSaveRestoration"), Restored, TEXT("Original preferences, personal points and TrainingData.sav bytes restored."));
}

bool ATrainerRuntimeVerifier::TraceGround(const FVector& Floor, FString& Evidence) const
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainerVerifyGround), true);
    Query.AddIgnoredActor(Player);
    if (Session) Query.AddIgnoredActor(Session->GetCurrentTarget());
    const bool HitGround = GetWorld()->LineTraceSingleByChannel(Hit, Floor + FVector(0,0,35), Floor - FVector(0,0,60), ECC_Visibility, Query);
    const bool Good = HitGround && FMath::Abs(Hit.ImpactPoint.Z - Floor.Z) < 4;
    Evidence = FString::Printf(TEXT("floor=%s hit=%d impact=%s actor=%s"), *Floor.ToString(), HitGround, *Hit.ImpactPoint.ToString(), *GetNameSafe(Hit.GetActor()));
    return Good;
}

bool ATrainerRuntimeVerifier::HasSight(const FVector& From, const FVector& To, FString* Blocker) const
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainerVerifyLOS), true);
    Query.AddIgnoredActor(Player);
    if (Session) Query.AddIgnoredActor(Session->GetCurrentTarget());
    const bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Query);
    if (Blocker) *Blocker = FString::Printf(TEXT("blocker=%s impact=%s"), *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString());
    return !Blocked;
}

bool ATrainerRuntimeVerifier::GroundPlayer() const
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainerVerifyPawnGround), true);
    Query.AddIgnoredActor(Player);
    if (Session) Query.AddIgnoredActor(Session->GetCurrentTarget());
    const FVector Center = Player->GetActorLocation();
    const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return GetWorld()->LineTraceSingleByChannel(Hit, Center, Center-FVector(0,0,HalfHeight+12), ECC_Visibility, Query)
        && FMath::Abs(Center.Z - HalfHeight - Hit.ImpactPoint.Z) < 12;
}

bool ATrainerRuntimeVerifier::StartMode(int32 Mode, const FString& Region, int32 Point)
{
    AccountEndingSession();
    Player->GetCharacterMovement()->MaxWalkSpeed = SavedMaxWalkSpeed;
    Controller->StartPractice(Mode, Region, 1, Point);
    AimedFrame = -1;
    if (!Session->IsRunning() || Session->IsFinished() || Controller->IsMenuOpen())
    {
        Record(FString::Printf(TEXT("Mode%dStart"), Mode), false, Session->GetStatus());
        return false;
    }
    Record(FString::Printf(TEXT("Mode%dStart"), Mode), true,
        FString::Printf(TEXT("region=%s point=%s player=%s target=%s"), *Region, *Session->GetPointName(), *Player->GetActorLocation().ToString(), *Session->GetTargetFloor().ToString()));
    return true;
}

void ATrainerRuntimeVerifier::AccountEndingSession()
{
    // Predict the save contract from runtime activity before a lifecycle call resets counters.
    if (Session && Session->IsRunning() && (Session->IsFinished() || Session->Shots>0 || Session->Mode==ECombatPracticeMode::Explore))
        ++ExpectedHistoryRecords;
}

bool ATrainerRuntimeVerifier::TeleportFixture(const FVector& Floor)
{
    FString Evidence;
    if (!TraceGround(Floor, Evidence))
    {
        Record(TEXT("AutomationFixtureGround"), false, Evidence);
        return false;
    }
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->SetActorLocation(Floor+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+1), false, nullptr, ETeleportType::TeleportPhysics);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    AimedFrame = -1;
    UE_LOG(LogTemp, Display, TEXT("[TrainerVerify] Automation fixture teleport=%s (not route-walking evidence)"), *Floor.ToString());
    return true;
}

bool ATrainerRuntimeVerifier::WalkToward(const FVector& Floor, float DeltaSeconds, bool StopOnSight)
{
    const FVector Difference = Floor - Player->GetActorLocation();
    const float Distance = Difference.Size2D();
    bSawMovement |= FVector::DistSquared2D(WalkStart, Player->GetActorLocation()) > 25;
    const auto Bot = Session->GetCurrentTarget();
    if (Distance <= 5 || (StopOnSight && Bot && HasSight(Player->GetCameraLocation(),Bot->GetHeadCenter())))
    {
        Player->GetCharacterMovement()->StopMovementImmediately();
        return true;
    }
    // Slow only near the endpoint, so a frame cannot overshoot the five-centimeter tolerance.
    const float InputScale = FMath::Clamp(Distance/(SavedMaxWalkSpeed*FMath::Max(DeltaSeconds, .005f))*.45f, .025f, 1.f);
    Player->AddMovementInput(Difference.GetSafeNormal2D(), InputScale, true);
    return false;
}

bool ATrainerRuntimeVerifier::FireAtTarget(bool Body,int32 Limb)
{
    auto Bot = Session->GetCurrentTarget();
    if (!Bot || Bot->IsHidden() || Session->IsFinished())return false;
    const auto LimbMesh=Limb==INDEX_NONE?nullptr:FindObject<UStaticMeshComponent>(Bot,*FString::Printf(TEXT("Limb%d"),Limb));
    const FVector Target=LimbMesh?LimbMesh->GetComponentLocation():Body?Bot->GetTorsoTarget():Bot->GetHeadCenter();
    if (!HasSight(Player->GetCameraLocation(),Target))
    {
        AimedFrame = -1;
        return false;
    }
    Player->GetCharacterMovement()->StopMovementImmediately();
    Controller->SetControlRotation((Target-Player->GetCameraLocation()).Rotation());
    if (AimedFrame < 0)
    {
        AimedFrame = Frame;
        AimAt = FPlatformTime::Seconds();
        return false;
    }
    if (Frame <= AimedFrame || Player->GetVelocity().Size2D() > 1.f) return false;
    // PlayerCameraManager caches the rendered viewpoint. Check it has caught up before shooting.
    FVector Eye; FRotator Rotation;
    Controller->GetPlayerViewPoint(Eye, Rotation);
    const float Dot = FVector::DotProduct(Rotation.Vector(), (Target-Eye).GetSafeNormal());
    const float Tolerance = Body ? .9998f : .999999f;
    if (Dot < Tolerance && FPlatformTime::Seconds()-AimAt < .20) return false;
    const bool Fired = Weapon->FireOnceNow();
    FVector Start, End;
    Weapon->GetLastShotRay(Start, End);
    UE_LOG(LogTemp, Display, TEXT("[TrainerVerify] shot body=%d fired=%d speed=%.2f dot=%.9f ray=%s -> %s hits=%d kills=%d"),
        Body, Fired, Player->GetVelocity().Size2D(), Dot, *Start.ToString(), *End.ToString(), Session->Hits, Session->Kills);
    AimedFrame = -1;
    return Fired;
}

FVector ATrainerRuntimeVerifier::FindFlashSource() const
{
    const FVector Expected = Session->GetCurrentPeekFloor()+FVector(0,0,180);
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        if (FVector::DistSquared(It->GetActorLocation(),Expected) < 10000) return It->GetActorLocation();
    return Expected;
}

void ATrainerRuntimeVerifier::BeginFlash(bool Back)
{
    if (!StartMode(3,TEXT("All"),PreAimPoint)) {Finish(TEXT("Flash failed to start."));return;}
    bPreFlashChecked = bFlashTurning = false;
    TeleportFixture(Session->GetCurrentPeekFloor());
    FlashSource = FindFlashSource();
    Enter(Back ? EStage::FlashBack : EStage::FlashFace);
}

void ATrainerRuntimeVerifier::DriveFlash(bool Back)
{
    // Leave one rendered frame for the facing flash screenshot before changing sessions.
    if (!Back && bFlashTurning)
    {
        if(Frame==FacingFlashFrame+3)
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/flash_facing.png")),true,false);
        if(Frame>FacingFlashFrame+5)BeginFlash(true);
        return;
    }
    const FVector Direction = (FlashSource - Player->GetCameraLocation()).GetSafeNormal();
    if (!bPreFlashChecked)
    {
        if (!Session->HasFlashFired() && Session->GetCurrentTarget())
        {
            Controller->SetControlRotation((Session->GetCurrentTarget()->GetHeadCenter()-Player->GetCameraLocation()).Rotation());
            if (AimedFrame < 0) {AimedFrame=Frame; return;}
            if (Frame>AimedFrame)
            {
                const int32 Before = Session->Kills;
                const bool Fired = Weapon->FireOnceNow();
                Record(Back?TEXT("FlashBackBeforeDetonation"):TEXT("FlashFaceBeforeDetonation"),
                    Fired && Session->Kills==Before && !Session->bWon && Session->Hits==0,
                    FString::Printf(TEXT("fired=%d flashFired=%d shots=%d hits=%d kills=%d"), Fired, Session->HasFlashFired(), Session->Shots,Session->Hits,Session->Kills));
                bPreFlashChecked=true; AimedFrame=-1;
            }
        }
        else
        {
            Record(TEXT("FlashPreDetonationWindow"),false,TEXT("Detonation occurred before the automation shot."));
            bPreFlashChecked=true;
        }
    }
    if (!Session->HasFlashFired())
    {
        Controller->SetControlRotation((Back ? -Direction : Direction).Rotation());
        return;
    }
    if (!bFlashTurning)
    {
        const float Alpha=Session->GetFlashAlpha();
        if (Back) BackFlashAlpha=Alpha; else FacingFlashAlpha=Alpha;
        Record(Back?TEXT("FlashBackStrength"):TEXT("FlashFacingStrength"),Back ? Alpha<.3f : Alpha>.3f,
            FString::Printf(TEXT("actualAlpha=%.4f source=%s eye=%s facingDot=%.4f"),Alpha,*FlashSource.ToString(),*Player->GetCameraLocation().ToString(),
                FVector::DotProduct(Controller->GetControlRotation().Vector(),Direction)));
        bFlashTurning=true;
        if (!Back)
        {
            FacingFlashFrame=Frame;
            return;
        }
    }
    FireAtTarget();
    if (Session->IsFinished())
    {
        Record(TEXT("FlashBackHeadshotWin"),Session->bWon && Session->Headshots==1,
            FString::Printf(TEXT("won=%d kills=%d shots=%d headshots=%d faceAlpha=%.4f backAlpha=%.4f"),Session->bWon,Session->Kills,Session->Shots,Session->Headshots,FacingFlashAlpha,BackFlashAlpha));
        Record(TEXT("FlashFacingVsBack"),FacingFlashAlpha>BackFlashAlpha && BackFlashAlpha<.3f,
            FString::Printf(TEXT("face=%.4f back=%.4f (two actual detonations)"),FacingFlashAlpha,BackFlashAlpha));
        if (StartMode(4))
        {
            LastKills=0; DefensePositions.Reset(); bHoldMoved=false;
            if (Session->GetCurrentTarget()) WaveBotStart=Session->GetCurrentTarget()->GetActorLocation();
            Enter(EStage::Defense);
        }
        else Finish(TEXT("Defense failed to start."));
    }
}

void ATrainerRuntimeVerifier::DriveWaves(bool Combat)
{
    if (Session->Kills != LastKills)
    {
        LastKills=Session->Kills;
        if (Combat) CombatAfterKill=Player->GetActorLocation();
    }
    auto Bot=Session->GetCurrentTarget();
    if (Bot)
    {
        if (Combat)
        {
            // Assert before fixture positioning: SpawnWave must not relocate the player.
            if (LastKills>0 && !CombatAfterKill.IsZero())
            {
                const float Delta=FVector::Dist(Player->GetActorLocation(),CombatAfterKill);
                bCombatContinuous &= Delta<5;
                Record(FString::Printf(TEXT("CombatEncounter%dPlayerContinuity"),LastKills+1),Delta<5,
                    FString::Printf(TEXT("before=%s afterSpawn=%s deltaCm=%.3f; next positioning is an explicit automation teleport"),
                        *CombatAfterKill.ToString(),*Player->GetActorLocation().ToString(),Delta));
                CombatAfterKill=FVector::ZeroVector;
                TeleportFixture(Session->GetCurrentPeekFloor());
            }
            const int32 BeforeHits=Session->Hits,BeforeKills=Session->Kills;
            const bool Body=Session->Kills==0;
            if(FireAtTarget(Body) && Session->Hits>BeforeHits)
            {
                if(Body)++CombatBodyShots;
                const auto& Feedback=Session->GetHitFeedback();
                Record(FString::Printf(TEXT("CombatConfirmedHit%d"),Session->Hits),Feedback.HitRemaining>0&&Feedback.Damage==(Body?40:160)&&Session->Shots==Session->Hits,
                    FString::Printf(TEXT("shots=%d hits=%d kills=%d damage=%.0f remainingHealth=%.0f confirmed=%d"),Session->Shots,Session->Hits,Session->Kills,Feedback.Damage,Feedback.RemainingHealth,Feedback.HitRemaining>0));
                if(Session->Kills>BeforeKills)
                    Record(FString::Printf(TEXT("CombatKillFeedback%d"),Session->Kills),Feedback.KillRemaining>0&&Feedback.bKilled&&Feedback.bHeadshotKill==!Body,
                        FString::Printf(TEXT("kills=%d bodyShots=%d headshot=%d killRemaining=%.3f"),Session->Kills,CombatBodyShots,Feedback.bHeadshotKill,Feedback.KillRemaining));
            }
        }
        else
        {
            bHoldMoved |= FVector::DistSquared(WaveBotStart,Bot->GetActorLocation())>25;
            if(!Bot->IsHidden() && Session->Kills==0 && TargetPreviewFrame<0)
            {
                // Capture the live model after it has cleared the cover, rather
                // than the first visible frame of its spawn animation.
                if(!HasSight(Player->GetCameraLocation(),Bot->GetHeadCenter()) ||
                   !HasSight(Player->GetCameraLocation(),Bot->GetActorLocation())) return;
                Record(TEXT("LiveEnemyHeadSize"),Bot->GetHeadRadius()>=9.f,FString::Printf(TEXT("visible/headshot radiusCm=%.2f eye=%s"),Bot->GetHeadRadius(),*Bot->GetHeadCenter().ToString()));
                Controller->SetControlRotation((Bot->GetHeadCenter()-Player->GetCameraLocation()).Rotation());
                TargetPreviewFrame=Frame;
                return;
            }
            if(TargetPreviewFrame>=0 && Frame==TargetPreviewFrame+1)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/enemy_head_preview.png")),false,false);
                return;
            }
            if (!Bot->IsHidden() && HasSight(Player->GetCameraLocation(),Bot->GetHeadCenter()))
            {
                const FVector Position=Bot->GetActorLocation();
                if (FireAtTarget() && Session->Kills>LastKills)
                {
                    DefensePositions.Add(Position);
                    WaveBotStart=FVector::ZeroVector;
                }
            }
        }
    }
    if (!Session->IsFinished()) return;
    if (Combat)
    {
        Record(TEXT("CombatThreeEncounters"),Session->bWon && Session->Kills==3 && bCombatContinuous,
            FString::Printf(TEXT("won=%d kills=%d headshots=%d health=%d continuous=%d; fixture teleports used between encounters, no route-walk claim"),
                Session->bWon,Session->Kills,Session->Headshots,Session->Health,bCombatContinuous));
        Enter(FParse::Param(FCommandLine::Get(),TEXT("TrainerVerifyRoutes"))?EStage::MapWalk:EStage::Persistence);
    }
    else
    {
        Record(TEXT("DefenseFiveWaves"),Session->bWon && Session->Kills==5 && bHoldMoved,
            FString::Printf(TEXT("won=%d kills=%d actualMoved=%d sampledKillPositions=%d"),Session->bWon,Session->Kills,bHoldMoved,DefensePositions.Num()));
        bool Different=false;
        for (int32 I=1;I<DefensePositions.Num();++I) Different |= FVector::Dist(DefensePositions[I],DefensePositions[0])>15;
        FString Positions;
        for (const FVector& Position:DefensePositions) Positions+=Position.ToString()+TEXT("; ");
        Record(TEXT("DefenseActualAngleVariation"),Different,Positions.IsEmpty()?Session->GetStatus():Positions);
        if (StartMode(5))
        {
            LastKills=0;CombatBodyShots=0;bCombatContinuous=true;CombatAfterKill=FVector::ZeroVector;
            TeleportFixture(Session->GetCurrentPeekFloor());
            Enter(EStage::CombatHurt);
        }
        else Finish(TEXT("Combat failed to start."));
    }
}

void ATrainerRuntimeVerifier::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bDone) return;
    ++Frame;
    const double Now=FPlatformTime::Seconds(), Age=Now-StageAt;
    if (Frame==StageFrame+2 && Stage!=EStage::AwaitWorld)
    {
        const FString Screenshot=FPaths::Combine(FPaths::ProjectSavedDir(),
            FString::Printf(TEXT("TrainerVerification/stage_%02d.png"),static_cast<int32>(Stage)));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Screenshot),true);
        FScreenshotRequest::RequestScreenshot(Screenshot,true,false);
    }
    const double Deadline=FParse::Param(FCommandLine::Get(),TEXT("TrainerVerifyRoutes"))?360:110;
    if (Now-StartedAt>Deadline)
    {
        Record(TEXT("OverallDeadline"),false,FString::Printf(TEXT("stage=%d exceeded 110 seconds"),static_cast<int32>(Stage)));
        Finish(TEXT("Overall runtime deadline reached."));return;
    }
    if (Stage==EStage::AwaitWorld)
    {
        Controller=Cast<AAscPlayerController>(UGameplayStatics::GetPlayerController(this,0));
        Player=Controller?Cast<AAscCharacter>(Controller->GetPawn()):nullptr;
        Session=Controller?Controller->GetSession():nullptr;
        Weapon=Player?Player->FindComponentByClass<UAscWeaponComponent>():nullptr;
        if (!Controller || !Player || !Session || !Weapon)
        {
            if(Age>15)Finish(TEXT("Player/controller/session/weapon unavailable after 15 seconds."));
            return;
        }
        SavedMaxWalkSpeed=Player->GetCharacterMovement()->MaxWalkSpeed;
        Record(TEXT("RenderedRuntimeWorld"),GetWorld()->GetGameViewport()!=nullptr && GetWorld()->IsGameWorld() && GetWorld()->GetMapName().Contains(TEXT("TrainerAscent")),
            FString::Printf(TEXT("map=%s viewport=%d gameWorld=%d"),*GetWorld()->GetMapName(),GetWorld()->GetGameViewport()!=nullptr,GetWorld()->IsGameWorld()));
        bool AudioReady=true;FString AudioEvidence;
        for(const TCHAR* Name:{TEXT("TrainingBodyHit"),TEXT("TrainingHeadshot"),TEXT("TrainingKill"),TEXT("TrainingHurt")})
        {
            const FString Path=FString::Printf(TEXT("/Game/CombatTraining/Audio/%s.%s"),Name,Name);
            const auto Sound=LoadObject<USoundBase>(nullptr,*Path);
            AudioReady &= Sound&&Sound->GetDuration()>.05f;
            AudioEvidence+=FString::Printf(TEXT("%s=%.3fs "),Name,Sound?Sound->GetDuration():0.f);
        }
        Record(TEXT("FeedbackAudioAssets"),AudioReady,AudioEvidence);
        Enter(EStage::GroundAndExplore);
    }
    else if(Stage==EStage::GroundAndExplore)
    {
        FString GeometryJson;TSharedPtr<FJsonObject> GeometryRoot;
        const TArray<TSharedPtr<FJsonValue>>* GeometryChecks=nullptr;
        const bool Loaded=FFileHelper::LoadFileToString(GeometryJson,*FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Data/trainer_map_geometry_checks.json")))
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GeometryJson),GeometryRoot) && GeometryRoot
            && GeometryRoot->TryGetArrayField(TEXT("checks"),GeometryChecks) && GeometryChecks->Num()==14;
        Record(TEXT("CombatGeometryChecksLoaded"),Loaded,TEXT("14 source wall/cover collision probes"));
        if(!Loaded){Finish(TEXT("Missing source wall/cover collision probes"));return;}
        for(const auto& Value:*GeometryChecks)
        {
            const auto Check=Value->AsObject();
            const auto VectorAt=[&](const TCHAR* Key)
            {const auto V=Check->GetArrayField(Key);return FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());};
            const FVector Surface=VectorAt(TEXT("surface"));FHitResult Hit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(VerifyCombatStructure),true);Query.AddIgnoredActor(Player);Query.AddIgnoredActor(Session->GetCurrentTarget());
            const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,VectorAt(TEXT("start")),VectorAt(TEXT("end")),ECC_Visibility,Query);
            Record(TEXT("CombatGeometryCollision_")+Check->GetStringField(TEXT("name")),Blocked&&FVector::Dist(Hit.ImpactPoint,Surface)<2.f,
                FString::Printf(TEXT("blocked=%d surface=%s impact=%s errorCm=%.4f actor=%s"),Blocked,*Surface.ToString(),*Hit.ImpactPoint.ToString(),FVector::Dist(Hit.ImpactPoint,Surface),*GetNameSafe(Hit.GetActor())));
        }
        const auto Points=Session->GetPoints();
        Record(TEXT("SourceValidatedPointCount"),Points.Num()>=7,FString::Printf(TEXT("Loaded %d actual point records"),Points.Num()));
        for (const auto& Point:Points)
        {
            FString P,E,K;
            const bool PG=TraceGround(Point.PlayerFloor,P),EG=TraceGround(Point.EnemyFloor,E),KG=TraceGround(Point.PeekFloor,K);
            Record(TEXT("Ground_")+Point.Id,PG&&EG&&KG,P+TEXT(" | ")+E+TEXT(" | ")+K);
            bAllGrounded &= PG&&EG&&KG;
            if(!Point.Id.StartsWith(TEXT("personal_")))
            {
                int32 ValidRoutes=0;
                FString Indices;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(VerifyDefenseCapsule),false);Query.AddIgnoredActor(Player);Query.AddIgnoredActor(Session->GetCurrentTarget());
                for(int32 I=0;I<Point.DefenseApproaches.Num();++I)
                {
                    const auto& Route=Point.DefenseApproaches[I];FHitResult Sweep;FString H,R;
                    const bool Blocked=GetWorld()->SweepSingleByChannel(Sweep,Route.Hide+FVector(0,0,94.5),Route.Peek+FVector(0,0,94.5),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(25,91.5),Query);
                    if(!Blocked && TraceGround(Route.Hide,H) && TraceGround(Route.Peek,R)
                        && !HasSight(Point.EnemyFloor+FVector(0,0,162.15),Route.Hide+FVector(0,0,160))
                        && HasSight(Point.EnemyFloor+FVector(0,0,162.15),Route.Peek+FVector(0,0,160)))
                    {++ValidRoutes;Indices+=FString::Printf(TEXT("%d "),I);}
                }
                Record(TEXT("DefenseCapsuleRoutes_")+Point.Id,ValidRoutes>=2,FString::Printf(TEXT("sourceRoutes=%d actualUEValid=%d indices=%s"),Point.DefenseApproaches.Num(),ValidRoutes,*Indices));
            }
        }
        for(const FString Region:{TEXT("A"),TEXT("Mid"),TEXT("B")})
        {
            const bool Started=StartMode(0,Region);
            Record(TEXT("RegionPlayerGround_")+Region,Started&&GroundPlayer(),Player->GetActorLocation().ToString());
        }
        if(!StartMode(0)){Finish(TEXT("Explore failed to start."));return;}
        WalkStart=Player->GetActorLocation();bSawMovement=false;
        Enter(EStage::ExploreWalk);
    }
    else if(Stage==EStage::ExploreWalk)
    {
        if(WalkToward(Session->GetCurrentPeekFloor(),DeltaSeconds,false) || Age>6)
        {
            const float Distance=FVector::Dist2D(Player->GetActorLocation(),Session->GetCurrentPeekFloor());
            Record(TEXT("ExploreRealMovement"),bSawMovement && Distance<=5 && GroundPlayer() && Session->IsRunning() && !Session->GetCurrentTarget(),
                FString::Printf(TEXT("start=%s end=%s distanceToPeek=%.3f grounded=%d velocity=%.2f; AddMovementInput used, no teleport"),
                    *WalkStart.ToString(),*Player->GetActorLocation().ToString(),Distance,GroundPlayer(),Player->GetVelocity().Size2D()));
            if(!StartMode(1)){Finish(TEXT("PreAim failed to start."));return;}
            Enter(EStage::PreAimOcclusion);
        }
    }
    else if(Stage==EStage::PreAimOcclusion)
    {
        auto Bot=Session->GetCurrentTarget();
        if(!Bot){Finish(TEXT("PreAim target missing."));return;}
        FString Blocker;
        const bool Occluded=!HasSight(Player->GetCameraLocation(),Bot->GetHeadCenter(),&Blocker);
        if(!Occluded)
        {
            ++PreAimPoint;
            if(PreAimPoint<Session->GetPoints().Num() && Age<6){StartMode(1,TEXT("All"),PreAimPoint);return;}
            Record(TEXT("PreAimWorldOcclusion"),false,TEXT("No selected source point began behind head-height world cover."));
        }
        else
        {
            if (AimedFrame<0)
            {
                const FVector Original=Player->GetActorLocation();
                // Sweep toward the hidden player position from the validated defender floor.
                const FVector From=Session->GetTargetFloor()+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3);
                Player->GetCapsuleComponent()->IgnoreActorWhenMoving(Bot,true);
                Player->SetActorLocation(From,false,nullptr,ETeleportType::TeleportPhysics);
                FHitResult SweepHit;
                const FVector HiddenFloor=Session->GetCurrentPlayerFloor();
                const FVector Desired(HiddenFloor.X,HiddenFloor.Y,From.Z);
                Player->SetActorLocation(Desired,true,&SweepHit);
                const FVector Actual=Player->GetActorLocation();
                const bool Blocked=SweepHit.bBlockingHit&&!SweepHit.bStartPenetrating&&SweepHit.GetActor()!=Bot&&FVector::Dist(Actual,Desired)>10&&FMath::Abs(SweepHit.Normal.Z)<.3f;
                Record(TEXT("PlayerCapsuleWorldCollision"),Blocked,
                    FString::Printf(TEXT("explicit sweep fixture from=%s desired=%s actual=%s blocked=%d penetrating=%d actor=%s"),
                        *From.ToString(),*Desired.ToString(),*Actual.ToString(),SweepHit.bBlockingHit,SweepHit.bStartPenetrating,*GetNameSafe(SweepHit.GetActor())));
                Player->SetActorLocation(Original,false,nullptr,ETeleportType::TeleportPhysics);
                Player->GetCapsuleComponent()->IgnoreActorWhenMoving(Bot,false);
                Player->GetCharacterMovement()->StopMovementImmediately();
            }
            Controller->SetControlRotation((Bot->GetHeadCenter()-Player->GetCameraLocation()).Rotation());
            if(AimedFrame<0){AimedFrame=Frame;return;}
            if(Frame<=AimedFrame)return;
            const bool Fired=Weapon->FireOnceNow();
            FVector RayStart,RayEnd;Weapon->GetLastShotRay(RayStart,RayEnd);
            const bool EndBeforeTarget=FVector::Dist(RayStart,RayEnd)<FVector::Dist(RayStart,Bot->GetHeadCenter())-5;
            Record(TEXT("NoFeedbackForOccludedShot"),Session->GetHitFeedback().HitRemaining==0&&Session->GetHitFeedback().KillRemaining==0,TEXT("Wall-blocked shot creates no confirmed hit or kill cue."));
            bool TracerStopped=false;
            for(TActorIterator<ATrainerShotTracer> It(GetWorld());It;++It)
                if(It->ActorHasTag(TEXT("PlayerShotTracer"))&&FVector::Dist(It->GetTraceEnd(),RayEnd)<1)TracerStopped=true;
            Record(TEXT("TracerStopsAtWall"),TracerStopped&&EndBeforeTarget,TEXT("Visible beam ends at the same first wall as the hitscan ray."));
            Record(TEXT("PreAimWorldOcclusion"),Fired&&Session->Kills==0&&Session->Hits==0&&EndBeforeTarget,
                FString::Printf(TEXT("fired=%d shots=%d hits=%d kills=%d stoppedRay=%d %s"),Fired,Session->Shots,Session->Hits,Session->Kills,EndBeforeTarget,*Blocker));
        }
        WalkStart=Player->GetActorLocation();bSawMovement=false;
        Enter(EStage::PreAimWalk);
    }
    else if(Stage==EStage::PreAimWalk)
    {
        if(WalkToward(Session->GetCurrentPeekFloor(),DeltaSeconds,false))
        {
            const auto Bot=Session->GetCurrentTarget();
            Record(TEXT("PreAimActualPeek"),bSawMovement&&GroundPlayer()&&Bot&&HasSight(Player->GetCameraLocation(),Bot->GetHeadCenter()),
                FString::Printf(TEXT("start=%s end=%s grounded=%d movementInputOnly=1"),*WalkStart.ToString(),*Player->GetActorLocation().ToString(),GroundPlayer()));
            // Body damage is a separate fixture: the walking case has already spent
            // part of the enemy reaction budget before reaching its final waypoint.
            AccountEndingSession();Session->Restart();TeleportFixture(Session->GetCurrentPeekFloor());
            BodyFired=0;Enter(EStage::LimbShots);
        }
        else if(Age>6 || Session->IsFinished())
        {
            Record(TEXT("PreAimActualPeek"),false,Session->GetStatus()+TEXT(" movement did not reach clear head line of sight"));
            AccountEndingSession();Session->Restart();TeleportFixture(Session->GetCurrentPeekFloor());BodyFired=0;Enter(EStage::LimbShots);
        }
    }
    else if(Stage==EStage::LimbShots)
    {
        if(FireAtTarget(false,BodyFired==0?3:0))++BodyFired;
        if(BodyFired>=3 || Age>3 || Session->IsFinished())
        {
            Record(TEXT("VisibleLimbsKillThreeShots"),BodyFired==3&&Session->Hits==3&&Session->Headshots==0&&Session->Kills==1&&Session->bWon,
                FString::Printf(TEXT("arm/legShots=%d hits=%d kills=%d won=%d status=%s"),BodyFired,Session->Hits,Session->Kills,Session->bWon,*Session->GetStatus()));
            AccountEndingSession();Session->Restart();TeleportFixture(Session->GetCurrentPeekFloor());BodyFired=0;Enter(EStage::BodyShots);
        }
    }
    else if(Stage==EStage::BodyShots)
    {
        if(BodyPreviewFrame>=0 && Frame<=BodyPreviewFrame+2)return;
        if(FireAtTarget(true))
        {
            ++BodyFired;
            if(BodyFired==1 && Session->GetCurrentTarget())
            {
                const auto& Feedback=Session->GetHitFeedback();
                auto Mesh=FindObject<UStaticMeshComponent>(Session->GetCurrentTarget(),TEXT("BodyVisual"));
                auto Flash=Mesh?Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)):nullptr;
                int32 Effects=0;for(TActorIterator<ATrainerImpactEffect> It(GetWorld());It;++It)++Effects;
                Record(TEXT("BodyHitFeedback"),Session->Hits==1&&Feedback.Region==EHitRegion::Body&&Feedback.Damage==40&&Feedback.RemainingHealth==60&&Feedback.HitRemaining>0&&Flash&&Flash->K2_GetScalarParameterValue(TEXT("ImpactFlash"))>0&&Effects>0,
                    FString::Printf(TEXT("hits=%d damage=%.0f remainingHealth=%.0f flash=%.2f worldEffects=%d"),Session->Hits,Feedback.Damage,Feedback.RemainingHealth,Flash?Flash->K2_GetScalarParameterValue(TEXT("ImpactFlash")):0,Effects));
                auto Receiver=FindObject<UStaticMeshComponent>(Player,TEXT("RifleReceiver"));
                auto RightHand=FindObject<UStaticMeshComponent>(Player,TEXT("RightGlove"));
                auto LeftHand=FindObject<UStaticMeshComponent>(Player,TEXT("LeftGlove"));
                auto MuzzleFlash=FindObject<UStaticMeshComponent>(Player,TEXT("RifleMuzzleFlash"));
                Record(TEXT("VisibleRifleAndHands"),Receiver&&RightHand&&LeftHand&&Receiver->IsVisible()&&RightHand->IsVisible()&&LeftHand->IsVisible()&&Receiver->bOnlyOwnerSee&&Receiver->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("Camera-attached receiver and both hands are visible and do not collide."));
                Record(TEXT("ShotMuzzleFlash"),MuzzleFlash&&MuzzleFlash->IsVisible(),TEXT("Real weapon shot enables the rifle muzzle flash."));
                bool TracerHit=false;
                for(TActorIterator<ATrainerShotTracer> It(GetWorld());It;++It)
                    if(It->ActorHasTag(TEXT("PlayerShotTracer"))&&FVector::Dist(It->GetTraceEnd(),Feedback.WorldImpact)<1&&FVector::Dist(It->GetTraceStart(),Player->GetGunMuzzleLocation())<1)TracerHit=true;
                Record(TEXT("TracerMuzzleToBodyImpact"),TracerHit,TEXT("Real body shot beam starts at rifle muzzle and ends at resolved body impact."));
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/body_hit_feedback.png")),false,false);
                BodyPreviewFrame=Frame;
            }
        }
        if(BodyFired>=3 || Age>3 || Session->IsFinished())
        {
            const auto& Feedback=Session->GetHitFeedback();
            Record(TEXT("BodyKillFeedback"),Feedback.Region==EHitRegion::Body&&Feedback.Damage==40&&Feedback.bKilled&&Feedback.KillRemaining>0&&Feedback.HitRemaining>0,
                FString::Printf(TEXT("region=%d damage=%.0f killed=%d hitRemaining=%.3f killRemaining=%.3f"),static_cast<int32>(Feedback.Region),Feedback.Damage,Feedback.bKilled,Feedback.HitRemaining,Feedback.KillRemaining));
            Record(TEXT("BodyDamageThreeShots"),BodyFired==3&&Session->Hits==3&&Session->Headshots==0&&Session->Kills==1&&Session->bWon,
                FString::Printf(TEXT("bodyShots=%d hits=%d headshots=%d kills=%d won=%d status=%s"),BodyFired,Session->Hits,Session->Headshots,Session->Kills,Session->bWon,*Session->GetStatus()));
            AccountEndingSession();Session->Restart();
            Record(TEXT("RestartResetsRound"),Session->IsRunning()&&!Session->IsFinished()&&Session->Shots==0&&Session->Kills==0&&Session->GetCurrentTarget()!=nullptr,Session->GetStatus());
            Record(TEXT("RestartClearsFeedback"),Session->GetHitFeedback().HitRemaining==0&&Session->GetHitFeedback().KillRemaining==0&&Session->GetPlayerHurtAlpha()==0,TEXT("New round clears hit, kill and player injury cues."));
            TeleportFixture(Session->GetCurrentPeekFloor());Enter(EStage::HeadWin);
        }
    }
    else if(Stage==EStage::HeadWin)
    {
        FireAtTarget();
        if(Session->IsFinished() || Age>3)
        {
            const auto& Feedback=Session->GetHitFeedback();
            Record(TEXT("HeadshotKillFeedback"),Feedback.Region==EHitRegion::Head&&Feedback.Damage==160&&Feedback.bHeadshotKill&&Feedback.KillRemaining>0,
                FString::Printf(TEXT("region=%d damage=%.0f headshotKill=%d killRemaining=%.3f"),static_cast<int32>(Feedback.Region),Feedback.Damage,Feedback.bHeadshotKill,Feedback.KillRemaining));
            Record(TEXT("PerfectHeadshotWin"),Session->bWon&&Session->Shots==1&&Session->Headshots==1&&Session->Kills==1,
                FString::Printf(TEXT("shots=%d headshots=%d kills=%d won=%d speed=%.3f"),Session->Shots,Session->Headshots,Session->Kills,Session->bWon,Session->LastShotSpeed));
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/headshot_kill_feedback.png")),false,false);
            WalkStart=Player->GetActorLocation();PauseShots=Session->Shots;
            Enter(EStage::PostRoundWalk);
        }
    }
    else if(Stage==EStage::PostRoundWalk)
    {
        Weapon->FireOnceNow();
        WalkToward(Session->GetCurrentPlayerFloor(),DeltaSeconds,false);
        const float Distance=FVector::Dist2D(WalkStart,Player->GetActorLocation());
        if(Distance>60 || Age>1.5)
        {
            Record(TEXT("CompletedRoundStillWalks"),Distance>40 && GroundPlayer() && Session->IsFinished() && Weapon->IsFrozen() && Session->Shots==PauseShots,
                FString::Printf(TEXT("actualWalkDistanceCm=%.2f movementMode=%d finished=%d shotsBefore=%d after=%d weaponFrozen=%d"),Distance,
                    static_cast<int32>(Player->GetCharacterMovement()->MovementMode),Session->IsFinished(),PauseShots,Session->Shots,Weapon->IsFrozen()));
            Player->GetCharacterMovement()->StopMovementImmediately();
            Enter(EStage::FeedbackFade);
        }
    }
    else if(Stage==EStage::FeedbackFade)
    {
        if(Age>1.3)
        {
            const auto& Feedback=Session->GetHitFeedback();
            Record(TEXT("FinishedFeedbackFades"),Session->IsFinished()&&Feedback.HitRemaining==0&&Feedback.KillRemaining==0,TEXT("Hit marker and kill badge expire after the round finishes."));
            AccountEndingSession();Session->Restart();TeleportFixture(Session->GetCurrentPeekFloor());Enter(EStage::FailedRound);
        }
    }
    else if(Stage==EStage::CombatHurt)
    {
        if(auto Bot=Session->GetCurrentTarget())Controller->SetControlRotation((Bot->GetHeadCenter()-Player->GetCameraLocation()).Rotation());
        if(Session->Health<100 || Age>3)
        {
            Record(TEXT("CombatPlayerHurtFeedback"),Session->Health==60&&Session->GetPlayerHurtAlpha()>0&&!Session->IsFinished(),
                FString::Printf(TEXT("health=%d hurtAlpha=%.3f shots=%d hits=%d"),Session->Health,Session->GetPlayerHurtAlpha(),Session->Shots,Session->Hits));
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/player_hurt_feedback.png")),false,false);
            Enter(EStage::Combat);
        }
    }
    else if(Stage==EStage::FailedRound)
    {
        if(Session->IsFinished() || Age>2)
        {
            if(AimedFrame<0)
            {
            Record(TEXT("FailedRoundClearsEnemy"),Session->IsFinished()&&!Session->bWon&&Session->GetCurrentTarget()==nullptr&&Weapon->IsFrozen(),
                FString::Printf(TEXT("finished=%d won=%d target=%s weaponFrozen=%d status=%s"),Session->IsFinished(),Session->bWon,*GetNameSafe(Session->GetCurrentTarget()),Weapon->IsFrozen(),*Session->GetStatus()));
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/failed_round_preview.png")),false,false);
                AimedFrame=Frame;return;
            }
            if(Frame<=AimedFrame+1)return;
            const FString Before=Session->GetPointName();AccountEndingSession();Session->Next();
            Record(TEXT("NextChangesPoint"),Before!=Session->GetPointName()&&Session->IsRunning()&&Session->Shots==0,
                Before+TEXT(" -> ")+Session->GetPointName());
            if(!StartMode(2)){Finish(TEXT("Hold failed to start."));return;}
            PauseBot=Session->GetCurrentTarget()?Session->GetCurrentTarget()->GetActorLocation():FVector::ZeroVector;
            WaveBotStart=PauseBot;PauseShots=Session->Shots;PauseHealth=Session->Health;
            Controller->ToggleMenu();Weapon->FireOnceNow();Enter(EStage::PauseHold);
        }
    }
    else if(Stage==EStage::PauseHold)
    {
        Weapon->FireOnceNow();
        if(Age>=1.2)
        {
            const auto Bot=Session->GetCurrentTarget();
            const bool Frozen=Bot&&FVector::DistSquared(PauseBot,Bot->GetActorLocation())<.01&&Session->Shots==PauseShots&&Session->Health==PauseHealth;
            Record(TEXT("PausedWorldFreeze"),Frozen&&Controller->IsMenuOpen()&&Session->IsPaused()&&UGameplayStatics::IsGamePaused(this)&&Weapon->IsFrozen(),
                FString::Printf(TEXT("wallSeconds=%.3f botDelta=%.4f shotsBefore=%d after=%d healthBefore=%d after=%d worldPaused=%d"),
                    Age,Bot?FVector::Dist(PauseBot,Bot->GetActorLocation()):-1,PauseShots,Session->Shots,PauseHealth,Session->Health,UGameplayStatics::IsGamePaused(this)));
            Controller->ToggleMenu();bHoldMoved=false;Enter(EStage::HoldWin);
        }
    }
    else if(Stage==EStage::HoldWin)
    {
        if(auto Bot=Session->GetCurrentTarget())
        {
            bHoldMoved |= FVector::DistSquared(WaveBotStart,Bot->GetActorLocation())>25;
            if(!Bot->IsHidden())FireAtTarget();
        }
        if(Session->IsFinished() || Age>10)
        {
            Record(TEXT("HoldWaitRevealAndHeadshot"),bHoldMoved&&Session->bWon&&Session->Headshots==1,
                FString::Printf(TEXT("actualBotMoved=%d waitedWallSeconds=%.3f won=%d headshots=%d status=%s"),bHoldMoved,Age,Session->bWon,Session->Headshots,*Session->GetStatus()));
            Record(TEXT("HoldHeadshotWithin200ms"),Session->bWon&&Session->LastReactionMs<200,FString::Printf(TEXT("reactionMs=%.3f"),Session->LastReactionMs));
            if(!StartMode(2)){Finish(TEXT("Hold deadline fixture could not start"));return;}
            bHoldHiddenChecked=false;Enter(EStage::HoldExposure);
        }
    }
    else if(Stage==EStage::HoldExposure)
    {
        if(auto Bot=Session->GetCurrentTarget())Controller->SetControlRotation((Bot->GetHeadCenter()-Player->GetCameraLocation()).Rotation());
        if(!bHoldHiddenChecked)
        {
            Record(TEXT("HoldHiddenNoCountdown"),!Session->IsHoldReactionStarted()&&Session->GetHoldReactionRemainingMs()==200,TEXT("Countdown waits for exposed shootable geometry."));
            bHoldHiddenChecked=true;
        }
        if(Session->IsHoldReactionStarted()&&Session->GetHoldReactionElapsedMs()>=40)
        {
            if(AimedFrame<0)
            {
                Record(TEXT("HoldCountdownStartsOnExposure"),!Session->IsFinished()&&Session->GetHoldReactionRemainingMs()>0&&Session->GetHoldReactionRemainingMs()<200,FString::Printf(TEXT("remainingMs=%.3f"),Session->GetHoldReactionRemainingMs()));
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/hold_countdown.png")),false,false);
                AimedFrame=Frame;return;
            }
            if(Frame<=AimedFrame+1)return;
            Controller->ToggleMenu();PausedHoldRemaining=Session->GetHoldReactionRemainingMs();Enter(EStage::HoldVisiblePause);
        }
        else if(Age>10){Record(TEXT("HoldCountdownStartsOnExposure"),false,TEXT("No exposure within fixture timeout"));BeginFlash(false);}
    }
    else if(Stage==EStage::HoldVisiblePause)
    {
        if(Age>=1.2)
        {
            Record(TEXT("HoldVisiblePausePreservesDeadline"),FMath::Abs(Session->GetHoldReactionRemainingMs()-PausedHoldRemaining)<.1f&&!Session->IsFinished(),FString::Printf(TEXT("pauseWallSeconds=%.3f beforeMs=%.3f afterMs=%.3f"),Age,PausedHoldRemaining,Session->GetHoldReactionRemainingMs()));
            Controller->ToggleMenu();Enter(EStage::HoldTimeout);
        }
    }
    else if(Stage==EStage::HoldTimeout)
    {
        if(Session->IsFinished()||Age>2)
        {
            if(AimedFrame<0)
            {
                Record(TEXT("Hold200msEnemyKillsPlayer"),Session->IsFinished()&&!Session->bWon&&Session->Health==0&&Session->Shots==0&&Session->GetHoldReactionElapsedMs()>=199.9f&&Session->GetHoldReactionElapsedMs()<250,FString::Printf(TEXT("elapsedMs=%.3f health=%d won=%d"),Session->GetHoldReactionElapsedMs(),Session->Health,Session->bWon));
                bool EnemyTracer=false;for(TActorIterator<ATrainerShotTracer> It(GetWorld());It;++It)EnemyTracer|=It->ActorHasTag(TEXT("EnemyShotTracer"));
                Record(TEXT("HoldEnemyTracerAndHurt"),EnemyTracer&&Session->GetPlayerHurtAlpha()>0,TEXT("Lethal enemy shot has a visible tracer and red player hurt cue."));
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/hold_deadline.png")),false,false);
                AimedFrame=Frame;return;
            }
            if(Frame<=AimedFrame+1)return;
            if(!StartMode(2)){Finish(TEXT("Late hold shot fixture could not start"));return;}
            Enter(EStage::HoldLateShot);
        }
    }
    else if(Stage==EStage::HoldLateShot)
    {
        if(auto Bot=Session->GetCurrentTarget())Controller->SetControlRotation((Bot->GetHeadCenter()-Player->GetCameraLocation()).Rotation());
        if(Session->IsHoldReactionStarted())
        {
            // Explicit frame-order fixture: advance the gameplay clock without session tick.
            Session->SetActorTickEnabled(false);
            if(Session->GetHoldReactionElapsedMs()>=215)
            {
                const bool Fired=Weapon->FireOnceNow();Session->SetActorTickEnabled(true);
                Record(TEXT("HoldLateShotCannotBeatTickOrder"),Fired&&Session->IsFinished()&&!Session->bWon&&Session->Health==0&&Session->Hits==0&&Session->Kills==0,TEXT("Shot after 215ms loses even when session deadline tick has not run."));
                BeginFlash(false);
            }
        }
        if(Age>10){Session->SetActorTickEnabled(true);Record(TEXT("HoldLateShotCannotBeatTickOrder"),false,TEXT("Late shot fixture timeout"));BeginFlash(false);}
    }
    else if(Stage==EStage::FlashFace || Stage==EStage::FlashBack)
    {
        if(Age>5)
        {
            Record(Stage==EStage::FlashFace?TEXT("FlashFacingTimeout"):TEXT("FlashBackTimeout"),false,
                FString::Printf(TEXT("flashFired=%d status=%s"),Session->HasFlashFired(),*Session->GetStatus()));
            if(Stage==EStage::FlashFace)BeginFlash(true);
            else
            {
                if(!StartMode(4)){Finish(TEXT("Defense failed to start."));return;}
                LastKills=0;DefensePositions.Reset();bHoldMoved=false;
                WaveBotStart=Session->GetCurrentTarget()?Session->GetCurrentTarget()->GetActorLocation():FVector::ZeroVector;
                Enter(EStage::Defense);
            }
        }
        else DriveFlash(Stage==EStage::FlashBack);
    }
    else if(Stage==EStage::Defense || Stage==EStage::Combat)
    {
        if(Age>35)
        {
            const bool Combat=Stage==EStage::Combat;
            Record(Combat?TEXT("CombatThreeEncounters"):TEXT("DefenseFiveWaves"),false,
                FString::Printf(TEXT("timed out kills=%d health=%d status=%s"),Session->Kills,Session->Health,*Session->GetStatus()));
            if(Combat)Enter(EStage::Persistence);
            else
            {
                if(!StartMode(5)){Finish(TEXT("Combat failed to start."));return;}
                LastKills=0;bCombatContinuous=true;CombatAfterKill=FVector::ZeroVector;
                TeleportFixture(Session->GetCurrentPeekFloor());Enter(EStage::Combat);
            }
        }
        else DriveWaves(Stage==EStage::Combat);
    }
    else if(Stage==EStage::MapWalk)
    {
        if(AimedFrame<0)
        {
            FString Json;TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Routes=nullptr;
            const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/combat_walk_routes.json"));
            if(!FFileHelper::LoadFileToString(Json,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root||!Root->TryGetArrayField(TEXT("routes"),Routes))
            {Record(TEXT("NativeWalkRoutesLoaded"),false,TEXT("Missing verification route data"));Finish(TEXT("Walk route data unavailable"));return;}
            for(const auto& Value:*Routes)
            {
                const auto Route=Value->AsObject();const TArray<TSharedPtr<FJsonValue>>* Waypoints=nullptr;FString Name;
                if(!Route || !Route->GetBoolField(TEXT("found")) || !Route->TryGetArrayField(TEXT("waypoints"),Waypoints)||!Route->TryGetStringField(TEXT("to"),Name))
                {Record(TEXT("NativeWalkRoutesLoaded"),false,TEXT("Source route is incomplete"));Finish(TEXT("Source route incomplete"));return;}
                for(const auto& Item:*Waypoints)
                {
                    const auto& XYZ=Item->AsArray();if(XYZ.Num()!=3){Finish(TEXT("Invalid waypoint"));return;}
                    WalkWaypoints.Add(FVector(XYZ[0]->AsNumber(),XYZ[1]->AsNumber(),XYZ[2]->AsNumber()));
                }
                WalkRouteEnds.Add(WalkWaypoints.Num()-1);WalkRouteNames.Add(Name);
            }
            if(!StartMode(0)){Finish(TEXT("Could not start walking verification"));return;}
            AimedFrame=Frame;WalkWaypointAt=Now;WalkWaypointIndex=WalkRouteIndex=0;
            Record(TEXT("NativeWalkRoutesLoaded"),WalkRouteEnds.Num()==7,FString::Printf(TEXT("routes=%d waypoints=%d"),WalkRouteEnds.Num(),WalkWaypoints.Num()));
            return;
        }
        if(!WalkWaypoints.IsValidIndex(WalkWaypointIndex)){Enter(EStage::Persistence);return;}
        const FVector Destination=WalkWaypoints[WalkWaypointIndex];
        FRotator Facing=(Destination-Player->GetActorLocation()).Rotation();Facing.Pitch=Facing.Roll=0;Controller->SetControlRotation(Facing);
        const FVector Difference=Destination-Player->GetActorLocation();
        const bool AtRouteEnd=WalkRouteEnds.IsValidIndex(WalkRouteIndex)&&WalkWaypointIndex==WalkRouteEnds[WalkRouteIndex];
        const double FootZ=Player->GetActorLocation().Z-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const bool Arrived=Difference.Size2D()<8 && (!AtRouteEnd||(GroundPlayer()&&FMath::Abs(FootZ-Destination.Z)<12));
        if(Arrived)
        {
            if(WalkRouteEnds.IsValidIndex(WalkRouteIndex)&&WalkWaypointIndex==WalkRouteEnds[WalkRouteIndex])
            {
                const FVector ActualFloor=Player->GetActorLocation()-FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
                Record(TEXT("NativeRouteWalk_")+WalkRouteNames[WalkRouteIndex],GroundPlayer()&&FMath::Abs(ActualFloor.Z-Destination.Z)<12,
                    FString::Printf(TEXT("actualFloor=%s destination=%s waypoint=%d; CharacterMovement input only after initial map spawn"),*ActualFloor.ToString(),*Destination.ToString(),WalkWaypointIndex));
                ++WalkRouteIndex;
            }
            ++WalkWaypointIndex;WalkWaypointAt=Now;
        }
        else if(Now-WalkWaypointAt>4)
        {
            Record(TEXT("NativeRouteWalkBlocked"),false,FString::Printf(TEXT("waypoint=%d actor=%s destination=%s grounded=%d"),WalkWaypointIndex,*Player->GetActorLocation().ToString(),*Destination.ToString(),GroundPlayer()));
            Finish(TEXT("Actual character movement could not reach route waypoint"));
        }
        else
        {
            const float Scale=FMath::Clamp(Difference.Size2D()/(SavedMaxWalkSpeed*FMath::Max(DeltaSeconds,.005f))*.45f,.025f,1.f);
            Player->AddMovementInput(Difference.GetSafeNormal2D(),Scale,true);
        }
    }
    else if(Stage==EStage::Persistence)
    {
        if (AimedFrame<0)
        {
            const auto Pref=UTrainingPreferences::Load();FString Error;
            const bool ValidPref=UTrainingPreferences::Validate(Pref,Error)&&Pref.LastMode==static_cast<int32>(Session->Mode)&&Pref.LastRegion==Session->SelectedRegion;
            Controller->ToggleMenu();
            Record(TEXT("PreferencesMenuReopen"),ValidPref&&Controller->IsMenuOpen(),
                FString::Printf(TEXT("menuOpen=%d diskLastMode=%d diskRegion=%s runtimeMode=%d parseError=%s"),Controller->IsMenuOpen(),Pref.LastMode,*Pref.LastRegion,static_cast<int32>(Session->Mode),*Error));
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/menu_reopened.png")),true,false);
            AimedFrame=Frame;
            return;
        }
        if (Frame<=AimedFrame+2)return;
        if (Session->IsRunning() && Controller->IsMenuOpen()) Controller->ToggleMenu();
        AccountEndingSession();Session->Stop();
        const auto Save=UTrainingSaveGame::Load();
        const int32 Added=Save->TotalSessionsCompleted-HistoryTotalBefore;
        bool Valid=false;
        if(!Save->Records.IsEmpty())
        {
            const auto& Last=Save->Records.Last();
            Valid=Last.TotalShots==Session->Shots&&Last.TotalHits==Session->Hits&&Last.Headshots==Session->Headshots&&Last.bWon==Session->bWon;
        }
        Record(TEXT("HistoryDiskReload"),Added==ExpectedHistoryRecords&&Added>0&&Valid,
            FString::Printf(TEXT("baselineTotal=%d reloadedTotal=%d expectedAdded=%d actualAdded=%d retainedRows=%d lastRecordMatchesRuntime=%d"),
                HistoryTotalBefore,Save->TotalSessionsCompleted,ExpectedHistoryRecords,Added,Save->Records.Num(),Valid));
        // Existing personal points are inspected without creating or replacing the user's file.
        const FString PersonalPath=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Trainer/personal_points.json"));
        FString Personal;TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* PersonalList=nullptr;
        const bool Exists=IFileManager::Get().FileExists(*PersonalPath);
        const bool Readable=!Exists||(FFileHelper::LoadFileToString(Personal,*PersonalPath)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Personal),Root)&&Root&&Root->TryGetArrayField(TEXT("points"),PersonalList));
        Record(TEXT("PersonalPointsReadOnly"),Readable,
            FString::Printf(TEXT("exists=%d parsedRows=%d file never written; capture/save workflow is not claimed by this case"),Exists,PersonalList?PersonalList->Num():0));
        const int32 BeforePoints=Session->GetPoints().Num();
        const FVector NativeEnemy=Session->GetPoints()[0].EnemyFloor;
        if(StartMode(0))
        {
            const bool RejectIncomplete=!Session->SavePersonalPoint();
            Session->CapturePlayer();
            const FVector PlayerBase=Player->GetActorLocation()-FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            FHitResult PlayerGround; FCollisionQueryParams CaptureQuery(SCENE_QUERY_STAT(VerifyPersonalGround),true);CaptureQuery.AddIgnoredActor(Player);
            const bool HasGround=GetWorld()->LineTraceSingleByChannel(PlayerGround,PlayerBase+FVector(0,0,35),PlayerBase-FVector(0,0,60),ECC_Visibility,CaptureQuery);
            const FVector CapturedPlayer=PlayerGround.ImpactPoint;
            const bool Positioned=TeleportFixture(NativeEnemy);
            Session->CaptureEnemy();
            const FVector CapturedEnemy=NativeEnemy;
            const bool SavedPoint=Session->SavePersonalPoint();
            // A fresh actor must load the new point from disk, independent of in-memory append.
            auto Reloaded=GetWorld()->SpawnActor<ATrainerSession>();
            bool Exact=false;
            if(Reloaded && Reloaded->GetPoints().Num()==BeforePoints+1)
            {
                const auto& Point=Reloaded->GetPoints().Last();
                Exact=Point.Id.StartsWith(TEXT("personal_")) && Point.Region==TEXT("A")
                    && FVector::Dist(Point.PlayerFloor,CapturedPlayer)<.01 && FVector::Dist(Point.EnemyFloor,CapturedEnemy)<.01;
            }
            Record(TEXT("PersonalCaptureSaveFreshReload"),RejectIncomplete&&HasGround&&Positioned&&SavedPoint&&Exact,
                FString::Printf(TEXT("missingCaptureRejected=%d saved=%d oldCount=%d newCount=%d exactCoordinatesAndRegion=%d; enemy capture uses explicit ground-position fixture"),
                    RejectIncomplete,SavedPoint,BeforePoints,Reloaded?Reloaded->GetPoints().Num():0,Exact));
            if(Reloaded)Reloaded->Destroy();
        }
        else Record(TEXT("PersonalCaptureSaveFreshReload"),false,TEXT("Explore could not start for point capture."));
        // The stage remains persistence; all fixture-created saves are restored by Finish.
        Finish(TEXT("Runtime stages completed."));
    }
}

void ATrainerRuntimeVerifier::Finish(const FString& Reason)
{
    if(bDone)return;
    bDone=true;
    RestoreFiles();
    auto Report=MakeShared<FJsonObject>();
    bool Passed=true;
    for(const auto& Item:Cases)Passed &= Item->AsObject()->GetBoolField(TEXT("Passed"));
    const bool Complete=Stage==EStage::Persistence;
    Report->SetBoolField(TEXT("Passed"),Passed&&Complete);
    Report->SetBoolField(TEXT("Completed"),Complete);
    Report->SetStringField(TEXT("Reason"),Reason);
    Report->SetStringField(TEXT("Map"),GetWorld()?GetWorld()->GetMapName():TEXT("unavailable"));
    Report->SetStringField(TEXT("PositioningPolicy"),TEXT("Explore/preaim movement uses CharacterMovement. Flash/combat use explicitly logged automation teleports; no continuous route-walk claim."));
    Report->SetNumberField(TEXT("DurationSeconds"),FPlatformTime::Seconds()-StartedAt);
    Report->SetArrayField(TEXT("Cases"),Cases);
    FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
    const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("TrainerVerification/runtime_report.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    const bool Saved=FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("[TrainerVerify] complete=%d passed=%d reportSaved=%d report=%s reason=%s"),Complete,Passed&&Complete,Saved,*Path,*Reason);
    SetActorTickEnabled(false);
    if(Controller&&Player)Player->GetCharacterMovement()->MaxWalkSpeed=SavedMaxWalkSpeed;
    if(FParse::Param(FCommandLine::Get(),TEXT("TrainerVerifyQuit")))
        UKismetSystemLibrary::QuitGame(this,Controller,EQuitPreference::Quit,false);
}

void ATrainerRuntimeVerifier::EndPlay(const EEndPlayReason::Type Reason)
{
    if(!bDone&&bSnapshotReady)Finish(TEXT("World ended before runtime stages completed."));
    Super::EndPlay(Reason);
}
