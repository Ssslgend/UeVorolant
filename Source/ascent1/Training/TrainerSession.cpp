#include "TrainerSession.h"
#include "TrainingPreferences.h"
#include "TrainingSaveGame.h"
#include "Player/AscCharacter.h"
#include "Player/AscWeaponComponent.h"
#include "Target/EnemyTarget.h"
#include "Target/TrainerImpactEffect.h"
#include "Player/TrainerShotTracer.h"
#include "Core/AscGameState.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Sound/SoundBase.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
    bool ReadVec(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, FVector& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Obj.IsValid() || !Obj->TryGetArrayField(Key, Values) || Values->Num() != 3) return false;
        double N[3];
        for (int32 I=0; I<3; ++I) if (!(*Values)[I]->TryGetNumber(N[I]) || !FMath::IsFinite(N[I])) return false;
        Out = FVector(N[0],N[1],N[2]); return true;
    }
    TArray<TSharedPtr<FJsonValue>> VecJson(const FVector& V)
    { return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
}

ATrainerSession::ATrainerSession() { PrimaryActorTick.bCanEverTick = true; }
void ATrainerSession::BeginPlay() { Super::BeginPlay(); History=UTrainingSaveGame::Load(); LoadPoints(); Status=TEXT("请选择训练模式"); }
void ATrainerSession::EndPlay(const EEndPlayReason::Type R) { Stop(); Super::EndPlay(R); }
FString ATrainerSession::ModeName(int32 I)
{
    static const TCHAR* Names[]={TEXT("自由探索"),TEXT("预瞄练习"),TEXT("架枪练习"),TEXT("背闪练习"),TEXT("防守波次"),TEXT("实战对枪")};
    return Names[FMath::Clamp(I,0,5)];
}
float ATrainerSession::ReactionBudget(int32 D) { return D==0 ? 650.f : D==2 ? 250.f : 400.f; }
float ATrainerSession::ReactionBudgetForMode(int32 M,int32 D) { return M==2?200.f:ReactionBudget(D); }
bool ATrainerSession::IsHoldDeadlineExpired(double VisibleSeconds) { return VisibleSeconds>=.2-1.e-6; }
float ATrainerSession::GetHoldReactionElapsedMs() const
{
    if(HoldExposureAt<0 || !GetWorld())return 0;
    const double Now=bPaused?PauseWorldAt:GetWorld()->GetTimeSeconds();
    return FMath::Max(0.f,static_cast<float>((Now-HoldExposureAt)*1000));
}
float ATrainerSession::GetHoldReactionRemainingMs() const { return FMath::Max(0.f,200.f-GetHoldReactionElapsedMs()); }
bool ATrainerSession::CanSeeHoldTarget() const
{
    if(!Bot || Bot->IsHidden() || !Player)return false;
    const FVector Eye=Player->GetCameraLocation();
    if(ClearSight(Eye,Bot->GetHeadCenter()) || ClearSight(Eye,Bot->GetTorsoTarget()))return true;
    for(const TCHAR* Name:{TEXT("Limb0"),TEXT("Limb1"),TEXT("Limb2"),TEXT("Limb3")})
        if(auto Part=FindObject<UStaticMeshComponent>(Bot,Name))if(ClearSight(Eye,Part->GetComponentLocation()))return true;
    return false;
}
void ATrainerSession::EnemyShot(bool Lethal)
{
    if(!Bot || !Player)return;
    if(auto Tracer=GetWorld()->SpawnActor<ATrainerShotTracer>())Tracer->Initialize(Bot->GetEyeLocation(),Player->GetCameraLocation(),true);
    if(auto Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/CombatTraining/Audio/TrainingShot.TrainingShot")))UGameplayStatics::PlaySound2D(this,Sound,.8f);
    if(auto Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/CombatTraining/Audio/TrainingHurt.TrainingHurt")))UGameplayStatics::PlaySound2D(this,Sound,.7f);
    PlayerHurtAlpha=1.f;
    if(Lethal)
    {
        Health=0;
        UE_LOG(LogTemp,Display,TEXT("[TrainerSession] HoldDeadline elapsedMs=%.3f budgetMs=200"),GetHoldReactionElapsedMs());
        Finish(false,TEXT("超过 200ms 未击杀，假人开枪！按 R 重练 / N 下一点"));
    }
    else Health=FMath::Max(0,Health-40);
}
float ATrainerSession::FlashStrength(float Dot, float Distance, bool Visible)
{
    if (!Visible || Distance>=3500.f) return 0;
    const float Range=FMath::Clamp((3500.f-Distance)/2000.f,0.f,1.f);
    return Range*(Dot>0.5f ? 1.f : Dot>0.f ? 0.55f : 0.12f);
}
bool ATrainerSession::IsRoundComplete(int32 K, int32 R) { return R>0 && K>=R; }
bool ATrainerSession::IsShotScorable(int32 M,bool F) {return M!=0 && (M!=3 || F);}
bool ATrainerSession::RaySphere(const FVector& O, const FVector& D, const FVector& C, float R, float& T)
{
    const FVector Offset=O-C; const double B=FVector::DotProduct(Offset,D), Disc=B*B-(Offset.SizeSquared()-R*R);
    if (Disc<0) return false;
    T=static_cast<float>(-B-FMath::Sqrt(Disc)); if(T<0) T=static_cast<float>(-B+FMath::Sqrt(Disc));
    return T>=0;
}
void ATrainerSession::LoadPoints()
{
    Points.Reset();
    for (const FString& File : {FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Data/trainer_combat_spots_v1.json")),FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Trainer/personal_points.json"))})
    {
        FString Text; TSharedPtr<FJsonObject> Root;
        if(!FFileHelper::LoadFileToString(Text,*File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root) continue;
        const TArray<TSharedPtr<FJsonValue>>* List=nullptr;
        if(!Root->TryGetArrayField(TEXT("points"),List)) continue;
        for(const auto& Value:*List)
        {
            if(!Value || Value->Type!=EJson::Object) continue;
            const auto Obj=Value->AsObject(); if(!Obj) continue;
            FCombatTrainingPoint P; const TSharedPtr<FJsonObject>* PlayerObj=nullptr,*EnemyObj=nullptr,*PeekObj=nullptr;
            if(!Obj->TryGetStringField(TEXT("id"),P.Id) || !Obj->TryGetStringField(TEXT("name"),P.Name) || !Obj->TryGetStringField(TEXT("region"),P.Region)
                || !Obj->TryGetObjectField(TEXT("player"),PlayerObj) || !Obj->TryGetObjectField(TEXT("enemy"),EnemyObj) || !Obj->TryGetObjectField(TEXT("peek"),PeekObj)
                || !ReadVec(*PlayerObj,TEXT("pos"),P.PlayerFloor) || !ReadVec(*EnemyObj,TEXT("pos"),P.EnemyFloor) || !ReadVec(*PeekObj,TEXT("pos"),P.PeekFloor)) continue;
            const TArray<TSharedPtr<FJsonValue>>* Routes=nullptr;
            if(Obj->TryGetArrayField(TEXT("defense_approaches"),Routes))
                for(const auto& Route:*Routes)
                {
                    if(!Route || Route->Type!=EJson::Object)continue;
                    FDefenseApproach Approach;
                    if(ReadVec(Route->AsObject(),TEXT("hide"),Approach.Hide) && ReadVec(Route->AsObject(),TEXT("peek"),Approach.Peek))
                        P.DefenseApproaches.Add(Approach);
                }
            Points.Add(P);
        }
    }
}
bool ATrainerSession::Grounded(const FVector& P) const
{
    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(PracticeGround),true); Q.AddIgnoredActor(Player); Q.AddIgnoredActor(Bot);
    return GetWorld()->LineTraceSingleByChannel(Hit,P+FVector(0,0,35),P-FVector(0,0,60),ECC_Visibility,Q) && FMath::Abs(Hit.ImpactPoint.Z-P.Z)<4;
}
bool ATrainerSession::ClearSight(const FVector& A,const FVector& B) const
{
    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(PracticeLOS),true); Q.AddIgnoredActor(Player); Q.AddIgnoredActor(Bot);
    return !GetWorld()->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Q);
}
bool ATrainerSession::PlacePlayer(const FVector& Floor,const FVector& Facing)
{
    if(!Player || !Grounded(Floor)) { Status=TEXT("点位缺少地面，已停止训练"); return false; }
    Player->UnCrouch(); Player->GetCharacterMovement()->StopMovementImmediately();
    FVector Center=Floor+FVector(0,0,Player->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()+3);
    const FVector RequestedCenter=Center;
    if(!GetWorld()->FindTeleportSpot(Player,Center,Player->GetActorRotation()) || FVector::Dist(Center,RequestedCenter)>30)
    {Status=TEXT("出生位置没有足够站立空间，请切换点位");return false;}
    Player->SetActorLocation(Center,false,nullptr,ETeleportType::TeleportPhysics);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if(auto PC=Cast<APlayerController>(Player->GetController())) { FRotator Rot=(Facing-Floor).Rotation(); Rot.Pitch=0; Rot.Roll=0; PC->SetControlRotation(Rot); }
    return true;
}
bool ATrainerSession::Start(int32 M,const FString& R,int32 D,int32 Index)
{
    Stop(); LoadPoints(); Selection.Reset();
    for(int32 I=0;I<Points.Num();++I) if(R==TEXT("All") || Points[I].Region==R) Selection.Add(I);
    if(Selection.IsEmpty()) { Status=TEXT("没有可用点位"); return false; }
    Player=Cast<AAscCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Player) return false;
    Weapon=Player->FindComponentByClass<UAscWeaponComponent>(); if(!Weapon) return false;
    Mode=static_cast<ECombatPracticeMode>(FMath::Clamp(M,0,5)); SelectedRegion=R; Difficulty=FMath::Clamp(D,0,2); SelectedPoint=FMath::Clamp(Index,0,Selection.Num()-1);
    Shots=Hits=Headshots=Kills=0; Health=100; Reactions.Reset(); LastReactionMs=BestReactionMs=0; Elapsed=FlashAlpha=FlashRemaining=0;
    LastShotSpeed=AverageAimErrorDegrees=0;StoppedShots=AimSamples=0;LastDefenseApproach=-1;
    HitFeedback=FTrainerHitFeedback();PlayerHurtAlpha=0;
    HoldExposureAt=-1;
    bRunning=true; bFinished=bPaused=bWon=false; RequiredKills=Mode==ECombatPracticeMode::Explore?0:Mode==ECombatPracticeMode::Defense?5:Mode==ECombatPracticeMode::Combat?3:1;
    const auto& P=Points[Selection[SelectedPoint]];
    const bool Hold=Mode==ECombatPracticeMode::Hold || Mode==ECombatPracticeMode::Defense;
    if(!PlacePlayer(Hold?P.EnemyFloor:P.PlayerFloor,Hold?P.PeekFloor:P.EnemyFloor)) {bRunning=false; return false;}
    FTrainerSettings Settings=UTrainingSettings::Load(); Player->ApplyTrainerSettings(Settings); Weapon->ApplySettings(Settings); Weapon->SetFrozen(false);
    UTrainingPreferences::Apply(Cast<APlayerController>(Player->GetController()),UTrainingPreferences::Load());
    ShotHandle=Weapon->OnWeaponShot.AddUObject(this,&ATrainerSession::Shot);
    if(auto GS=GetWorld()->GetGameState<AAscGameState>()) {GS->TotalShots=GS->TotalHits=GS->Headshots=0; GS->BestReactionTimeMs=TNumericLimits<float>::Max();}
    if(Mode!=ECombatPracticeMode::Explore) SpawnWave(); else Status=TEXT("自由探索：沿原地图路线移动，F5/F6/F7 记录个人点位");
    auto Pref=UTrainingPreferences::Load(); Pref.LastMode=M; Pref.LastRegion=R; Pref.LastDifficulty=D; Pref.LastPoint=SelectedPoint; FString Error; UTrainingPreferences::Save(Pref,Error);
    UE_LOG(LogTemp,Display,TEXT("[TrainerSession] Start mode=%d region=%s point=%s"),M,*R,*P.Id);
    return true;
}
void ATrainerSession::SpawnWave()
{
    if(Bot) { Bot->Destroy(); Bot=nullptr; }
    if(FlashActor){FlashActor->Destroy();FlashActor=nullptr;}
    if(Mode==ECombatPracticeMode::Combat && Kills>0 && Selection.Num()>1)
    {
        SelectedPoint=(SelectedPoint+FMath::RandRange(1,Selection.Num()-1))%Selection.Num();
        // Keep the player's physical position: the next encounter is reached along the map routes.
    }
    const auto& P=Points[Selection[SelectedPoint]];
    const bool Moving=Mode==ECombatPracticeMode::Hold || Mode==ECombatPracticeMode::Defense;
    BotHide=Moving?P.PlayerFloor:P.EnemyFloor; BotPeek=Moving?P.PeekFloor:P.EnemyFloor;
    if(Mode==ECombatPracticeMode::Combat)
    {
        const FVector Side=FVector::CrossProduct((P.PlayerFloor-P.EnemyFloor).GetSafeNormal2D(),FVector::UpVector)*FMath::FRandRange(-65.f,65.f);
        const FVector Candidate=P.EnemyFloor+Side;
        if(Grounded(Candidate))BotPeek=Candidate;
    }
    // These routes were measured against original source geometry for this fixed defender position.
    if(Mode==ECombatPracticeMode::Defense)
    {
        TArray<int32> Approaches;
        const FVector Eye=Player->GetCameraLocation();
        for(int32 I=0;I<P.DefenseApproaches.Num();++I)
        {
            const auto& Route=P.DefenseApproaches[I];
            FHitResult RouteHit;
            FCollisionQueryParams RouteQuery(SCENE_QUERY_STAT(DefenseCapsulePath),false);RouteQuery.AddIgnoredActor(Player);RouteQuery.AddIgnoredActor(Bot);
            const bool CapsuleBlocked=GetWorld()->SweepSingleByChannel(RouteHit,Route.Hide+FVector(0,0,94.5),Route.Peek+FVector(0,0,94.5),
                FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(25.f,91.5f),RouteQuery);
            if(I!=LastDefenseApproach && Grounded(Route.Hide) && Grounded(Route.Peek)
                && !CapsuleBlocked && !ClearSight(Eye,Route.Hide+FVector(0,0,160)) && ClearSight(Eye,Route.Peek+FVector(0,0,160)))Approaches.Add(I);
        }
        if(!Approaches.IsEmpty())
        {
            LastDefenseApproach=Approaches[FMath::RandRange(0,Approaches.Num()-1)];
            BotHide=P.DefenseApproaches[LastDefenseApproach].Hide;BotPeek=P.DefenseApproaches[LastDefenseApproach].Peek;
            UE_LOG(LogTemp,Display,TEXT("[TrainerSession] Defense wave=%d approach=%d hide=%s peek=%s"),Kills+1,LastDefenseApproach,*BotHide.ToString(),*BotPeek.ToString());
        }
        else if(P.DefenseApproaches.Num()>1){Finish(false,TEXT("当前位置看不到有效防守入口，按 R 返回守点"));return;}
    }
    if(!Grounded(BotHide) || !Grounded(BotPeek)) {Finish(false,TEXT("敌人路线没有有效地面")); return;}
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Bot=GetWorld()->SpawnActor<AEnemyTarget>(BotHide+FVector(0,0,94.5),FRotator::ZeroRotator,Params);
    if(!Bot) {Finish(false,TEXT("敌人生成失败")); return;}
    Bot->SetGroundClearance(3.f);
    Bot->ConfigureHitVolumes(Bot->GetHeadRadius(),Mode==ECombatPracticeMode::Combat && FMath::RandBool()?112.f:160.f);
    if(Mode==ECombatPracticeMode::Flash || Moving)Bot->SetActorHiddenInGame(true);
    WaveElapsed=0; LosSince=BotLastShot=-1;HoldExposureAt=-1; BotHealth=100; BotRevealDelay=FMath::FRandRange(0.8f,2.8f); bFlashFired=false;
    FlashSource=P.PeekFloor+FVector(0,0,180);
    bReactionRecorded=false;
    if(Mode==ECombatPracticeMode::Flash)
    {
        const FVector Candidate=FlashSource+FVector(FMath::FRandRange(-35.f,35.f),FMath::FRandRange(-35.f,35.f),FMath::FRandRange(0.f,40.f));
        if(ClearSight(P.PeekFloor+FVector(0,0,160),Candidate))FlashSource=Candidate;
        FlashActor=GetWorld()->SpawnActor<AStaticMeshActor>(FlashSource,FRotator::ZeroRotator);
        if(FlashActor){auto Mesh=FlashActor->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CombatTraining/Targets/M_TargetHead.M_TargetHead")));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);FlashActor->SetActorScale3D(FVector(.2));}
    }
    Status=Mode==ECombatPracticeMode::Flash?TEXT("先探身；闪光提示后转身背闪，再击杀敌人"):
        Mode==ECombatPracticeMode::Hold?TEXT("架住拐角：假人露出后须在 200ms 内击杀"):
        Moving?TEXT("架住拐角：敌人将随机探身，停稳后开枪"):TEXT("沿掩体探身，预瞄原墙面爆头高度，击杀敌人");
}
void ATrainerSession::Tick(float Dt)
{
    Super::Tick(Dt);
    if(bPaused)return;
    HitFeedback.HitRemaining=FMath::Max(0.f,HitFeedback.HitRemaining-Dt);
    HitFeedback.KillRemaining=FMath::Max(0.f,HitFeedback.KillRemaining-Dt);
    PlayerHurtAlpha=FMath::Max(0.f,PlayerHurtAlpha-Dt*3.f);
    if(!bRunning || bFinished || !Player) return;
    Elapsed+=Dt; WaveElapsed+=Dt; FlashRemaining=FMath::Max(0.f,FlashRemaining-Dt);
    FlashAlpha=FMath::Max(0.f,FlashAlpha-Dt*0.55f);
    if(Player->GetActorLocation().Z < -2000.f) {Finish(false,TEXT("离开可行走区域，按 R 返回点位"));return;}
    if(Mode==ECombatPracticeMode::Explore) return;
    if(!Bot) {if(WaveElapsed>1.1f) SpawnWave(); return;}
    const bool Moving=Mode==ECombatPracticeMode::Hold || Mode==ECombatPracticeMode::Defense || Mode==ECombatPracticeMode::Combat;
    if(Moving && WaveElapsed<=BotRevealDelay && Mode!=ECombatPracticeMode::Combat)return;
    if(Moving && WaveElapsed>BotRevealDelay)
    {
        Bot->SetActorHiddenInGame(false);
        const float Speed=Difficulty==0?170.f:Difficulty==2?420.f:280.f;
        FVector Desired=FMath::VInterpConstantTo(Bot->GetActorLocation(),BotPeek+FVector(0,0,94.5),Dt,Speed);
        FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(BotStepGround),true);GroundQuery.AddIgnoredActor(Bot);GroundQuery.AddIgnoredActor(Player);
        FHitResult Ground;
        if(GetWorld()->LineTraceSingleByChannel(Ground,Desired+FVector(0,0,50),Desired-FVector(0,0,150),ECC_Visibility,GroundQuery))Desired.Z=Ground.ImpactPoint.Z+94.5;
        else {Finish(false,TEXT("探身路线缺少地面，按 N 切换点位"));return;}
        FHitResult Hit; Bot->SetActorLocation(Desired,true,&Hit);
        if(Hit.bBlockingHit && !Hit.bStartPenetrating && FMath::Abs(Hit.Normal.Z)<.3f && Ground.bBlockingHit)
        {
            // A three-part capsule sweep permits ordinary stairs without crossing walls.
            const FVector Original=Bot->GetActorLocation();FHitResult Up,Across,Down;
            Bot->SetActorLocation(Original+FVector(0,0,22),true,&Up);
            if(!Up.bBlockingHit)
            {
                Bot->SetActorLocation(FVector(Desired.X,Desired.Y,Bot->GetActorLocation().Z),true,&Across);
                if(!Across.bBlockingHit){Bot->SetActorLocation(Desired,true,&Down);if(!Down.bStartPenetrating)Hit=FHitResult();}
            }
            if(Hit.bBlockingHit)Bot->SetActorLocation(Original,false);
        }
        if(Hit.bBlockingHit && Mode==ECombatPracticeMode::Combat)BotPeek=Bot->GetActorLocation()-FVector(0,0,94.5);
        else if(Hit.bBlockingHit && WaveElapsed>BotRevealDelay+3) {Finish(false,TEXT("探身路线受阻，按 N 切换点位"));return;}
    }
    const FVector Eye=Player->GetCameraLocation(); const bool Visible=ClearSight(Bot->GetEyeLocation(),Eye);
    if(Mode==ECombatPracticeMode::Hold)
    {
        const bool Shootable=CanSeeHoldTarget();
        if(Shootable && HoldExposureAt<0)
        {
            HoldExposureAt=GetWorld()->GetTimeSeconds();LosSince=Elapsed;
            UE_LOG(LogTemp,Display,TEXT("[TrainerSession] HoldExposureStarted budgetMs=200 point=%s"),*GetPointName());
        }
        if(Shootable && IsHoldDeadlineExpired(GetHoldReactionElapsedMs()*.001)) {EnemyShot(true);return;}
        if(Elapsed>60.f)Finish(false,TEXT("本轮超时，按 R 重练 / N 下一点"));
        return;
    }
    if(Mode==ECombatPracticeMode::Flash && !bFlashFired)
    {
        if(Visible && LosSince<0) {LosSince=Elapsed; Status=TEXT("闪光准备！转身避闪，再回头对枪");}
        if(LosSince>=0 && Elapsed-LosSince>1.1f)
        {
            bFlashFired=true; const auto PC=Cast<APlayerController>(Player->GetController());
            Bot->SetActorHiddenInGame(false);if(FlashActor){FlashActor->Destroy();FlashActor=nullptr;}
            const float Dot=PC?FVector::DotProduct(PC->GetControlRotation().Vector(),(FlashSource-Eye).GetSafeNormal()):1;
            FlashAlpha=FlashStrength(Dot,FVector::Dist(Eye,FlashSource),ClearSight(Eye,FlashSource)); FlashRemaining=FlashAlpha*2.8f;
            Status=FlashAlpha<.3f?TEXT("背闪成功！回头击杀敌人"):TEXT("受到闪光！等视野恢复后对枪"); LosSince=Elapsed;
        }
        return;
    }
    if(Visible && LosSince<0) LosSince=Elapsed;
    if(Visible && LosSince>=0 && Elapsed-LosSince>(ReactionBudget(Difficulty)*.001f+ (Mode==ECombatPracticeMode::Flash?.7f:0.f)))
    {
        if(Mode!=ECombatPracticeMode::Combat) {Health=0;Finish(false,TEXT("敌人抢先开枪，按 R 重练 / N 下一点"));return;}
        if(BotLastShot<0 || Elapsed-BotLastShot>.25f)
        {
            EnemyShot(false);BotLastShot=Elapsed;
            if(Health<=0) {Health=0;Finish(false,TEXT("生命值耗尽，按 R 重练"));return;}
        }
    }
    if(Elapsed>(Mode==ECombatPracticeMode::Combat?180.f:60.f)) Finish(false,TEXT("本轮超时，按 R 重练 / N 下一点"));
}
void ATrainerSession::Shot(const FVector& Start,const FVector& End,const FVector&,bool)
{
    if(!bRunning || bFinished || bPaused) return; ++Shots;
    if(Mode==ECombatPracticeMode::Hold && CanSeeHoldTarget())
    {
        if(HoldExposureAt<0){HoldExposureAt=GetWorld()->GetTimeSeconds();LosSince=Elapsed;}
        if(IsHoldDeadlineExpired(GetHoldReactionElapsedMs()*.001))
        {
            if(auto GS=GetWorld()->GetGameState<AAscGameState>())GS->RecordShot(false,false,0);
            EnemyShot(true);return;
        }
    }
    LastShotSpeed=Player?Player->GetVelocity().Size2D():0;if(LastShotSpeed<=50.f)++StoppedShots;
    bool Hit=false,Head=false; float Reaction=0;
    if(Bot && !Bot->IsHidden() && IsShotScorable(static_cast<int32>(Mode),bFlashFired))
    {
        const FVector Direction=(End-Start).GetSafeNormal();
        const float Error=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction,(Bot->GetHeadCenter()-Start).GetSafeNormal()),-1.0,1.0)));
        AverageAimErrorDegrees=(AverageAimErrorDegrees*AimSamples+Error)/(AimSamples+1);++AimSamples;
        if(!bReactionRecorded && ClearSight(Start,Bot->GetEyeLocation()))
        {
            if(LosSince<0)LosSince=Elapsed;
            Reaction=Mode==ECombatPracticeMode::Hold?GetHoldReactionElapsedMs():FMath::Max(0.f,(Elapsed-LosSince)*1000.f);LastReactionMs=Reaction;Reactions.Add(Reaction);
            BestReactionMs=Reactions.Num()==1?Reaction:FMath::Min(BestReactionMs,Reaction);bReactionRecorded=true;
        }
        float T=0; EHitRegion Region=EHitRegion::Miss;
        const bool Intersects=Bot->IntersectShotRay(Start,Direction,T,Region);
        const bool H=Region==EHitRegion::Head;
        if(Intersects && T<=(End-Start).Size() && ClearSight(Start,Start+Direction*T))
        {
            Hit=true;Head=H;++Hits; if(H) ++Headshots;
            const float Damage=H?Weapon->HeadDamage:Region==EHitRegion::Legs?Weapon->LegsDamage:Weapon->BodyDamage;
            BotHealth-=Damage;
            HitFeedback.Region=Region;HitFeedback.Damage=Damage;HitFeedback.RemainingHealth=FMath::Max(0.f,BotHealth);
            HitFeedback.WorldImpact=Start+Direction*T;HitFeedback.HitRemaining=.45f;HitFeedback.bKilled=BotHealth<=0;
            Weapon->SetResolvedShotImpact(HitFeedback.WorldImpact,Bot);
            Bot->ShowHitReaction(Region);
            if(auto Effect=GetWorld()->SpawnActor<ATrainerImpactEffect>(HitFeedback.WorldImpact,FRotator::ZeroRotator))
            {
                Effect->Initialize(Direction,H);
                if(BotHealth>0)Effect->AttachToActor(Bot,FAttachmentTransformRules::KeepWorldTransform);
            }
            const TCHAR* SoundPath=H?TEXT("/Game/CombatTraining/Audio/TrainingHeadshot.TrainingHeadshot"):TEXT("/Game/CombatTraining/Audio/TrainingBodyHit.TrainingBodyHit");
            if(auto Sound=LoadObject<USoundBase>(nullptr,SoundPath))UGameplayStatics::PlaySound2D(this,Sound,H?.9f:.8f);
            UE_LOG(LogTemp,Display,TEXT("[TrainerSession] ConfirmedHit mode=%d region=%d damage=%.0f enemyHealth=%.0f shots=%d hits=%d headshots=%d"),static_cast<int32>(Mode),static_cast<int32>(Region),Damage,HitFeedback.RemainingHealth,Shots,Hits,Headshots);
            if(BotHealth<=0)
            {
                ++Kills;
                HitFeedback.KillRemaining=1.2f;HitFeedback.bHeadshotKill=H;
                if(auto Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/CombatTraining/Audio/TrainingKill.TrainingKill")))UGameplayStatics::PlaySound2D(this,Sound,.7f);
                Bot->Destroy(); Bot=nullptr; WaveElapsed=0;
                if(IsRoundComplete(Kills,RequiredKills)) Finish(true,TEXT("本轮完成！按 R 重练 / N 下一点"));
                else Status=FString::Printf(TEXT("击杀 %d / %d，准备下一波"),Kills,RequiredKills);
            }
            else Status=FString::Printf(TEXT("命中%s -%.0f，敌人剩余 %.0f 生命"),Region==EHitRegion::Legs?TEXT("腿部"):TEXT("身体"),Damage,HitFeedback.RemainingHealth);
        }
    }
    if(auto GS=GetWorld()->GetGameState<AAscGameState>()) GS->RecordShot(Hit,Head,Reaction);
}
void ATrainerSession::Finish(bool Won,const FString& Reason)
{
    if(bFinished) return; bFinished=true; bWon=Won; Status=Reason;
    if(Bot){Bot->Destroy();Bot=nullptr;}
    if(FlashActor){FlashActor->Destroy();FlashActor=nullptr;}
    if(Weapon) {Weapon->StopFiring();Weapon->SetFrozen(true);} StoreRecord(Won);
    if(Player) Player->GetCharacterMovement()->StopMovementImmediately();
    UE_LOG(LogTemp,Display,TEXT("[TrainerSession] Result mode=%d win=%d shots=%d hits=%d headshots=%d reason=%s"),static_cast<int32>(Mode),Won,Shots,Hits,Headshots,*Reason);
}
void ATrainerSession::StoreRecord(bool Won)
{
    if(!History) History=UTrainingSaveGame::Load();
    FTrainingRecord R; R.Date=FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));R.MapName=TEXT("亚海悬城 / ")+GetPointName();R.ModeName=GetModeName();
    R.TotalShots=Shots;R.TotalHits=Hits;R.HeadshotRate=Hits?100.f*Headshots/Hits:0;R.HitRate=Shots?100.f*Hits/Shots:0;
    R.BestReactionTimeMs=BestReactionMs; for(float Reaction:Reactions) R.AverageReactionTimeMs+=Reaction;
    if(Reactions.Num()) R.AverageReactionTimeMs/=Reactions.Num(); R.bWon=Won;R.Headshots=Headshots;
    R.StoppedShotRate=Shots?100.f*StoppedShots/Shots:0;R.AverageAimErrorDegrees=AverageAimErrorDegrees;
    History->AddRecord(R);History->Save();
}
void ATrainerSession::Stop()
{
    if(bRunning && !bFinished && (Shots>0 || Mode==ECombatPracticeMode::Explore)) StoreRecord(false);
    bRunning=false;bPaused=false;
    HitFeedback=FTrainerHitFeedback();PlayerHurtAlpha=0;
    if(Weapon){Weapon->OnWeaponShot.Remove(ShotHandle);Weapon->StopFiring();Weapon->SetFrozen(true);}
    if(Bot){Bot->Destroy();Bot=nullptr;}
    if(FlashActor){FlashActor->Destroy();FlashActor=nullptr;}
}
void ATrainerSession::Pause(bool P)
{
    const double Now=GetWorld()?GetWorld()->GetTimeSeconds():0;
    if(P&&!bPaused)PauseWorldAt=Now;
    if(!P&&bPaused&&HoldExposureAt>=0)HoldExposureAt+=Now-PauseWorldAt;
    bPaused=P;if(Weapon){Weapon->StopFiring();Weapon->SetFrozen(P||bFinished);}
}
void ATrainerSession::Restart(){Start(static_cast<int32>(Mode),SelectedRegion,Difficulty,SelectedPoint);}
void ATrainerSession::Next(){Start(static_cast<int32>(Mode),SelectedRegion,Difficulty,Selection.IsEmpty()?0:(SelectedPoint+1)%Selection.Num());}
FString ATrainerSession::GetPointName() const {return Selection.IsValidIndex(SelectedPoint)&&Points.IsValidIndex(Selection[SelectedPoint])?Points[Selection[SelectedPoint]].Name:TEXT("");}
FVector ATrainerSession::GetTargetFloor() const {return Bot?Bot->GetActorLocation()-FVector(0,0,94.5):Selection.IsValidIndex(SelectedPoint)?Points[Selection[SelectedPoint]].EnemyFloor:FVector::ZeroVector;}
bool ATrainerSession::CaptureFloor(FVector& Floor) const
{
    if(!Player || Player->GetCharacterMovement()->IsFalling()) return false;
    const FVector Base=Player->GetActorLocation()-FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(PersonalPointGround),true);
    Q.AddIgnoredActor(Player); Q.AddIgnoredActor(Bot);
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Base+FVector(0,0,35),Base-FVector(0,0,60),ECC_Visibility,Q)
        || FMath::Abs(Hit.ImpactPoint.Z-Base.Z)>35 || Hit.ImpactNormal.Z<.65) return false;
    Floor=Hit.ImpactPoint;
    return true;
}
void ATrainerSession::CapturePlayer()
{
    bHasPlayerCapture=CaptureFloor(CapturedPlayer);
    Status=bHasPlayerCapture?TEXT("已记录玩家位置，走到敌人位置按 F6"):TEXT("请站在有效地面上再按 F5");
}
void ATrainerSession::CaptureEnemy()
{
    bHasEnemyCapture=CaptureFloor(CapturedEnemy);
    Status=bHasEnemyCapture?TEXT("已记录敌人位置，按 F7 保存个人点位"):TEXT("请站在有效地面上再按 F6");
}
bool ATrainerSession::SavePersonalPoint()
{
    if(!bHasPlayerCapture || !bHasEnemyCapture || !Grounded(CapturedPlayer) || !Grounded(CapturedEnemy)){Status=TEXT("请先用 F5/F6 记录两处有效地面位置");return false;}
    auto Root=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> List;
    const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Trainer/personal_points.json"));
    FString Existing;TSharedPtr<FJsonObject> Old;const TArray<TSharedPtr<FJsonValue>>* OldList=nullptr;
    if(FFileHelper::LoadFileToString(Existing,*Path)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Existing),Old)&&Old&&Old->TryGetArrayField(TEXT("points"),OldList)) List=*OldList;
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("id"),TEXT("personal_")+FGuid::NewGuid().ToString());P->SetStringField(TEXT("name"),FString::Printf(TEXT("个人点位 %d"),List.Num()+1));
    FString Region=SelectedRegion;
    if(Region==TEXT("All"))
    {
        double Nearest=TNumericLimits<double>::Max();Region=TEXT("Mid");
        for(const auto& Point:Points)if(!Point.Id.StartsWith(TEXT("personal_")))
        {
            const double Distance=FVector::DistSquared2D(CapturedPlayer,Point.PlayerFloor);
            if(Distance<Nearest){Nearest=Distance;Region=Point.Region;}
        }
    }
    P->SetStringField(TEXT("region"),Region);
    for(const auto& Pair:TArray<TPair<FString,FVector>>{{TEXT("player"),CapturedPlayer},{TEXT("enemy"),CapturedEnemy},{TEXT("peek"),CapturedPlayer}})
    {auto Pos=MakeShared<FJsonObject>();Pos->SetArrayField(TEXT("pos"),VecJson(Pair.Value));P->SetObjectField(Pair.Key,Pos);}
    List.Add(MakeShared<FJsonValueObject>(P));Root->SetArrayField(TEXT("points"),List);FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    const bool Saved=FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);Status=Saved?TEXT("个人点位已保存，可在点位菜单选择"):TEXT("点位保存失败");if(Saved) LoadPoints();return Saved;
}
