#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Training/TrainerSession.h"
#include "Target/EnemyTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/AscCharacter.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionRulesTest, "ascent1.Trainer.Session.Rules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSessionRulesTest::RunTest(const FString&)
{
    TestEqual(TEXT("easy reaction budget"), ATrainerSession::ReactionBudget(0), 650.f);
    TestEqual(TEXT("hard reaction budget"), ATrainerSession::ReactionBudget(2), 250.f);
    TestEqual(TEXT("blocked flash"), ATrainerSession::FlashStrength(1.f, 500.f, false), 0.f);
    TestEqual(TEXT("behind is reduced"), ATrainerSession::FlashStrength(-1.f, 500.f, true), 0.12f);
    TestEqual(TEXT("front flash is full"), ATrainerSession::FlashStrength(1.f, 500.f, true), 1.f);
    TestEqual(TEXT("out of range"), ATrainerSession::FlashStrength(1.f, 5000.f, true), 0.f);
    TestTrue(TEXT("head shot round completes"), ATrainerSession::IsRoundComplete(1, 1));
    TestFalse(TEXT("defense needs all waves"), ATrainerSession::IsRoundComplete(2, 5));
    TestFalse(TEXT("free roam never completes itself"), ATrainerSession::IsRoundComplete(0, 0));
    TestFalse(TEXT("flash cannot score before detonation"), ATrainerSession::IsShotScorable(3, false));
    TestTrue(TEXT("flash scores after detonation"), ATrainerSession::IsShotScorable(3, true));
    TestFalse(TEXT("explore has no scored enemies"), ATrainerSession::IsShotScorable(0, true));
    float Distance = 0.f;
    TestTrue(TEXT("ray intersects head"), ATrainerSession::RaySphere(FVector::ZeroVector, FVector(1,0,0), FVector(100,0,0), 4.f, Distance));
    TestEqual(TEXT("first surface"), Distance, 96.f);
    TestFalse(TEXT("ray pointing away"), ATrainerSession::RaySphere(FVector::ZeroVector, FVector(-1,0,0), FVector(100,0,0), 4.f, Distance));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyHeadModelTest, "ascent1.Trainer.Enemy.VisibleHead", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyHeadModelTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("Head inspection world"),World)) return false;
    auto Enemy=World->SpawnActor<AEnemyTarget>();
    if(!Enemy){World->DestroyWorld(false);return false;}
    auto Head=FindObject<UStaticMeshComponent>(Enemy,TEXT("HeadVisual"));
    auto Body=FindObject<UStaticMeshComponent>(Enemy,TEXT("BodyVisual"));
    auto Neck=FindObject<UStaticMeshComponent>(Enemy,TEXT("NeckVisual"));
    TestNotNull(TEXT("Visible head"),Head);TestNotNull(TEXT("Torso"),Body);TestNotNull(TEXT("Neck joins head and torso"),Neck);
    for(const float EyeHeight:{160.f,112.f})
    {
        Enemy->ConfigureHitVolumes(Enemy->GetHeadRadius(),EyeHeight);
        if(Head && Body)
        {
            const float Radius=Head->GetRelativeScale3D().X*50;
            TestTrue(TEXT("Readable head diameter is at least 18cm"),Radius>=9.f);
            TestTrue(TEXT("Whole visible head matches the headshot volume"),FMath::IsNearlyEqual(Radius,Enemy->GetHeadRadius(),.001f));
            TestTrue(TEXT("Head model is centered on headshot target"),Head->GetComponentLocation().Equals(Enemy->GetHeadCenter(),.001f));
            if(Neck)
            {
                const float TorsoTop=Body->GetComponentLocation().Z+Body->GetRelativeScale3D().Z*50;
                const float HeadBottom=Head->GetComponentLocation().Z-Radius;
                const float NeckBottom=Neck->GetComponentLocation().Z-Neck->GetRelativeScale3D().Z*50;
                const float NeckTop=Neck->GetComponentLocation().Z+Neck->GetRelativeScale3D().Z*50;
                TestTrue(TEXT("Neck seats into torso"),NeckBottom<=TorsoTop);
                TestTrue(TEXT("Neck seats into head"),NeckTop>=HeadBottom);
            }
        }
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyVisibleBodyHitsTest, "ascent1.Trainer.Enemy.VisibleBodyHits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyVisibleBodyHitsTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Enemy=World->SpawnActor<AEnemyTarget>();
    if(!Enemy){World->DestroyWorld(false);return false;}
    Enemy->SetActorRotation(FRotator(0,37,0));
    for(const float EyeHeight:{160.f,112.f})
    {
        Enemy->ConfigureHitVolumes(9.f,EyeHeight);
        for(const TCHAR* Name:{TEXT("BodyVisual"),TEXT("NeckVisual"),TEXT("Limb0"),TEXT("Limb1"),TEXT("Limb2"),TEXT("Limb3"),TEXT("HeadVisual")})
        {
            auto Mesh=FindObject<UStaticMeshComponent>(Enemy,Name);
            if(!TestNotNull(Name,Mesh))continue;
            const FVector Aim=Mesh->GetComponentLocation();
            const FVector Direction=Enemy->GetActorForwardVector();
            float Distance=0;EHitRegion Region=EHitRegion::Miss;
            TestTrue(FString::Printf(TEXT("Visible %s takes damage at eye height %.0f"),Name,EyeHeight),
                Enemy->IntersectShotRay(Aim-Direction*400,Direction,Distance,Region));
            const EHitRegion Want=Mesh==FindObject<UStaticMeshComponent>(Enemy,TEXT("HeadVisual"))?EHitRegion::Head:
                (FString(Name)==TEXT("Limb0")||FString(Name)==TEXT("Limb1"))?EHitRegion::Legs:EHitRegion::Body;
            TestTrue(TEXT("Correct visible hit region"),Region==Want);
            TestTrue(TEXT("Visible hit is in front of the ray"),Distance>0&&Distance<400);
        }
        const FVector Gap=Enemy->GetActorLocation()+Enemy->GetActorRightVector()*11;
        float Distance=0;EHitRegion Region=EHitRegion::Miss;
        TestFalse(TEXT("Empty space below feet is not a hit"),Enemy->IntersectShotRay(Gap-FVector(400,0,110),FVector(1,0,0),Distance,Region));
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBodyHitFlashTest, "ascent1.Trainer.Enemy.BodyHitFlash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyBodyHitFlashTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Enemy=World->SpawnActor<AEnemyTarget>();
    if(!Enemy){World->DestroyWorld(false);return false;}
    Enemy->OnShotByPlayer(Enemy->GetTorsoTarget());
    auto Body=FindObject<UStaticMeshComponent>(Enemy,TEXT("BodyVisual"));
    auto Material=Body?Cast<UMaterialInstanceDynamic>(Body->GetMaterial(0)):nullptr;
    if(TestNotNull(TEXT("A real body hit creates a local flash material"),Material))
    {
        TestTrue(TEXT("Body hit flashes immediately"),Material->K2_GetScalarParameterValue(TEXT("ImpactFlash"))>.9f);
        Enemy->Tick(.2f);
        TestEqual(TEXT("Body flash decays rather than staying white"),Material->K2_GetScalarParameterValue(TEXT("ImpactFlash")),0.f);
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHoldDeadlineRulesTest, "ascent1.Trainer.Session.HoldDeadline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHoldDeadlineRulesTest::RunTest(const FString&)
{
    for(int32 Difficulty=0;Difficulty<3;++Difficulty)
        TestEqual(TEXT("Hold always gives exactly 200 milliseconds"),ATrainerSession::ReactionBudgetForMode(2,Difficulty),200.f);
    TestEqual(TEXT("Preaim retains its difficulty budget"),ATrainerSession::ReactionBudgetForMode(1,0),650.f);
    TestFalse(TEXT("A shot at 199ms is still in time"),ATrainerSession::IsHoldDeadlineExpired(.199));
    TestTrue(TEXT("At 200ms the kill deadline has expired"),ATrainerSession::IsHoldDeadlineExpired(.200));
    TestTrue(TEXT("A later shot must not override an enemy kill"),ATrainerSession::IsHoldDeadlineExpired(.205));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstPersonWeaponModelTest, "ascent1.Trainer.Player.HeldRifle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFirstPersonWeaponModelTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Player=World->SpawnActor<AAscCharacter>();
    if(!Player){World->DestroyWorld(false);return false;}
    for(const TCHAR* Name:{TEXT("RifleReceiver"),TEXT("RifleBarrel"),TEXT("RifleMagazine"),TEXT("RightGlove"),TEXT("LeftGlove")})
    {
        auto Part=FindObject<UStaticMeshComponent>(Player,Name);
        if(TestNotNull(FString::Printf(TEXT("Player visibly holds %s"),Name),Part))
        {
            TestTrue(TEXT("Part has real mesh geometry"),Part->GetStaticMesh()!=nullptr);
            TestTrue(TEXT("Weapon and hands never block player movement or bullet traces"),Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
            TestTrue(TEXT("Only the owning player's camera sees the view model"),Part->bOnlyOwnerSee);
        }
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponTracerTest, "ascent1.Trainer.Player.ShotTracer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWeaponTracerTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Player=World->SpawnActor<AAscCharacter>();
    if(!Player){World->DestroyWorld(false);return false;}
    auto Weapon=Player->FindComponentByClass<UAscWeaponComponent>();
    TestTrue(TEXT("An actual weapon shot fires"),Weapon&&Weapon->FireOnceNow());
    int32 Tracers=0;
    for(TActorIterator<AActor> It(World);It;++It)
        if(It->ActorHasTag(TEXT("PlayerShotTracer")))
        {
            ++Tracers;
            auto Beam=FindObject<UStaticMeshComponent>(*It,TEXT("Beam"));
            if(TestNotNull(TEXT("Shot has a real visible beam mesh"),Beam))
                TestTrue(TEXT("Tracer cannot obstruct characters or later bullets"),Beam->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
        }
    TestEqual(TEXT("Every fired player bullet creates one tracer"),Tracers,1);
    World->DestroyWorld(false);
    return true;
}
#endif
