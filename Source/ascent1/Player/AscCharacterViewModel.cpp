#include "AscCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/AscPlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const FVector RiflePosition(52,17,-14);
}

void AAscCharacter::BuildViewModel()
{
    RifleRoot=CreateDefaultSubobject<USceneComponent>(TEXT("HeldRifleRoot"));
    RifleRoot->SetupAttachment(Camera);RifleRoot->SetRelativeLocation(RiflePosition);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Tint(TEXT("/Game/CombatTraining/Weapons/M_ViewModelTint.M_ViewModelTint"));
    const FLinearColor Metal(.055f,.07f,.085f),Steel(.16f,.19f,.22f),Rubber(.015f,.022f,.027f),Glove(.028f,.045f,.05f),Sleeve(.075f,.10f,.12f);
    auto Part=[&](const TCHAR* Name,UStaticMesh* ShapeMesh,FVector Location,FVector Dimensions,FLinearColor Color,FRotator Rotation=FRotator(0,0,0))
    {
        auto Component=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Component->SetupAttachment(RifleRoot);Component->SetStaticMesh(ShapeMesh);
        Component->SetRelativeLocation(Location);Component->SetRelativeRotation(Rotation);Component->SetRelativeScale3D(Dimensions/100.f);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);Component->SetOnlyOwnerSee(true);Component->SetCastShadow(false);
        Component->SetMaterial(0,Tint.Object);Component->ComponentTags.Add(TEXT("TrainerViewModel"));
        // Tints are initialized in BeginPlay/tick, never create transient MIDs in a CDO.
        Component->ComponentTags.Add(FName(*FString::Printf(TEXT("Tint_%.3f_%.3f_%.3f"),Color.R,Color.G,Color.B)));
        ViewModelParts.Add(Component);return Component;
    };
    // Anatomy follows a 77cm rifle: connected stock, receiver, guard, barrel and muzzle.
    Part(TEXT("RifleReceiver"),Cube.Object,FVector(4,0,3),FVector(25,6,9),Metal);
    Part(TEXT("RifleUpper"),Cube.Object,FVector(5,0,8),FVector(27,5.4f,3),Steel);
    Part(TEXT("RifleStock"),Cube.Object,FVector(-17,0,2),FVector(18,5,7),Rubber);
    Part(TEXT("RifleButtPlate"),Cube.Object,FVector(-26,0,2),FVector(2.2f,6.8f,11),Metal);
    Part(TEXT("RifleStockJoin"),Cylinder.Object,FVector(-9,0,4),FVector(3.8f,3.8f,5),Steel,FRotator(90,0,0));
    Part(TEXT("RifleHandguard"),Cube.Object,FVector(25,0,4),FVector(20,5.5f,7),Rubber);
    Part(TEXT("RifleHandguardTop"),Cube.Object,FVector(25,0,8),FVector(20,4,1.6f),Metal);
    Part(TEXT("RifleBarrel"),Cylinder.Object,FVector(40,0,4),FVector(1.8f,1.8f,14),Steel,FRotator(90,0,0));
    Part(TEXT("RifleMuzzleBrake"),Cylinder.Object,FVector(48,0,4),FVector(3,3,4),Metal,FRotator(90,0,0));
    Part(TEXT("RifleBore"),Cylinder.Object,FVector(50.1f,0,4),FVector(1.8f,1.8f,.2f),Rubber,FRotator(90,0,0));
    Part(TEXT("RifleMagazine"),Cube.Object,FVector(9,0,-9),FVector(7,4.5f,16),Metal,FRotator(12,0,0));
    Part(TEXT("RifleMagazineBase"),Cube.Object,FVector(7.5f,0,-17),FVector(8,5,2),Rubber);
    Part(TEXT("RifleGrip"),Cube.Object,FVector(-3,0,-7),FVector(5,4,12),Rubber,FRotator(-15,0,0));
    Part(TEXT("RifleTriggerGuardBottom"),Cube.Object,FVector(1,0,-5),FVector(7,1.1f,1.2f),Steel);
    Part(TEXT("RifleTriggerGuardFront"),Cube.Object,FVector(4,0,-2.5f),FVector(1.2f,1.1f,5),Steel);
    Part(TEXT("RifleTrigger"),Cube.Object,FVector(0,0,-2),FVector(1,1.2f,3),Metal,FRotator(-15,0,0));
    Part(TEXT("RifleEjectionPort"),Cube.Object,FVector(5,3.08f,5),FVector(8,.3f,2),Rubber);
    Part(TEXT("RifleChargingHandle"),Cube.Object,FVector(0,4,6),FVector(3,2.5f,1.2f),Steel);
    Part(TEXT("RifleRearSight"),Cube.Object,FVector(-5,0,11),FVector(3,3,3),Rubber);
    Part(TEXT("RifleFrontSight"),Cube.Object,FVector(30,0,10.5f),FVector(2,2,3),Metal);
    for(int32 I=0;I<5;++I)
    {
        Part(*FString::Printf(TEXT("RifleVent%d"),I),Cube.Object,FVector(18+I*3.f,2.82f,5),FVector(1.3f,.25f,2.6f),Metal);
        Part(*FString::Printf(TEXT("RifleRail%d"),I),Cube.Object,FVector(14+I*4.f,0,9.2f),FVector(1.6f,4.6f,.8f),Steel);
    }
    // Right hand seats around the grip; support hand wraps the fore-end from below.
    Part(TEXT("RightGlove"),Sphere.Object,FVector(-4,3,-5),FVector(9,7,10),Glove);
    Part(TEXT("RightThumb"),Cylinder.Object,FVector(-4,-1,-2),FVector(2.3f,2.3f,6),Glove,FRotator(25,0,20));
    Part(TEXT("RightTriggerFinger"),Cylinder.Object,FVector(1,2.1f,-1),FVector(1.5f,1.5f,6),Glove,FRotator(90,0,0));
    Part(TEXT("LeftGlove"),Sphere.Object,FVector(21,-1,-.8f),FVector(10,8,8),Glove);
    Part(TEXT("LeftThumb"),Cylinder.Object,FVector(22,3,2),FVector(2,2,7),Glove,FRotator(0,0,65));
    for(int32 I=0;I<3;++I)
    {
        Part(*FString::Printf(TEXT("RightFinger%d"),I),Cylinder.Object,FVector(-3-I*.8f,-1,-5-I*2.f),FVector(1.7f,1.7f,5),Glove,FRotator(90,0,0));
        Part(*FString::Printf(TEXT("LeftFinger%d"),I),Cylinder.Object,FVector(18+I*2.f,-3,1.5f),FVector(1.7f,1.7f,5.5f),Glove,FRotator(20,0,10));
    }
    auto Arm=[&](const TCHAR* Name,FVector Elbow,FVector Wrist,float Diameter)
    {
        const FVector Delta=Wrist-Elbow;
        Part(Name,Cylinder.Object,(Elbow+Wrist)*.5f,FVector(Diameter,Diameter,Delta.Size()+1.f),Sleeve,FRotationMatrix::MakeFromZ(Delta).Rotator());
    };
    Arm(TEXT("RightForearm"),FVector(-28,17,-27),FVector(-4,3,-9),8.5f);
    Arm(TEXT("LeftForearm"),FVector(-28,-20,-28),FVector(21,-1,-2),8);
    RifleMuzzle=CreateDefaultSubobject<USceneComponent>(TEXT("RifleMuzzle"));
    RifleMuzzle->SetupAttachment(RifleRoot);RifleMuzzle->SetRelativeLocation(FVector(50.5f,0,4));
    RifleMuzzleFlash=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RifleMuzzleFlash"));
    RifleMuzzleFlash->SetupAttachment(RifleMuzzle);RifleMuzzleFlash->SetStaticMesh(Sphere.Object);
    RifleMuzzleFlash->SetRelativeScale3D(FVector(.07f,.045f,.045f));
    RifleMuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);RifleMuzzleFlash->SetOnlyOwnerSee(true);RifleMuzzleFlash->SetCastShadow(false);RifleMuzzleFlash->SetVisibility(false);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Flash(TEXT("/Game/CombatTraining/Targets/M_HitSpark.M_HitSpark"));
    RifleMuzzleFlash->SetMaterial(0,Flash.Object);
}

FVector AAscCharacter::GetGunMuzzleLocation() const { return RifleMuzzle?RifleMuzzle->GetComponentLocation():GetCameraLocation(); }

void AAscCharacter::PlayViewModelShot()
{
    ViewModelKick=1.f;MuzzleFlashRemaining=.055f;
    if(RifleMuzzleFlash)RifleMuzzleFlash->SetVisibility(true);
}

void AAscCharacter::TickViewModel(float Dt)
{
    if(!RifleRoot)return;
    const auto PC=Cast<AAscPlayerController>(GetController());
    const bool Visible=!PC || (!PC->IsMenuOpen()&&CurrentWeapon==EWeaponType::Rifle);
    RifleRoot->SetVisibility(Visible,true);
    for(auto Part:ViewModelParts)
    {
        if(Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))continue;
        for(const FName Tag:Part->ComponentTags)
        {
            TArray<FString> Values;Tag.ToString().ParseIntoArray(Values,TEXT("_"));
            if(Values.Num()==4&&Values[0]==TEXT("Tint"))
                if(auto Material=Part->CreateAndSetMaterialInstanceDynamic(0))Material->SetVectorParameterValue(TEXT("GunColor"),FLinearColor(FCString::Atof(*Values[1]),FCString::Atof(*Values[2]),FCString::Atof(*Values[3])));
        }
    }
    ViewModelKick=FMath::Max(0.f,ViewModelKick-Dt*9.f);MuzzleFlashRemaining=FMath::Max(0.f,MuzzleFlashRemaining-Dt);
    if(RifleMuzzleFlash)RifleMuzzleFlash->SetVisibility(Visible&&MuzzleFlashRemaining>0);
    const float Moving=FMath::Clamp(GetVelocity().Size2D()/540.f,0.f,1.f);
    ViewModelBobTime+=Dt*9.f;
    RifleRoot->SetRelativeLocation(RiflePosition+FVector(-2*ViewModelKick,FMath::Sin(ViewModelBobTime)*Moving*.35f,FMath::Abs(FMath::Cos(ViewModelBobTime))*Moving*.5f));
    RifleRoot->SetRelativeRotation(FRotator(ViewModelKick*2.2f,0,0));
}
