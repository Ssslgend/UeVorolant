#include "TrainingHubBuilder.h"
#include "../Target/EnemyTarget.h"
#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ATrainingHubBuilder::ATrainingHubBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	TargetClass = AEnemyTarget::StaticClass();
}

FRotator ATrainingHubBuilder::GetTrainingRegionFacingRotation()
{
	return FRotator(0.0f, 90.0f, 0.0f);
}

FVector ATrainingHubBuilder::GetGroundedRegionActorLocation(
	const FVector& RegionLocation,
	const FBoxSphereBounds& MeshBounds,
	float MeshScale,
	float SourceFloorZ)
{
	const FVector ScaledCenterOffset = MeshBounds.Origin * MeshScale;
	return FVector(
		RegionLocation.X - ScaledCenterOffset.X,
		RegionLocation.Y - ScaledCenterOffset.Y,
		RegionLocation.Z - SourceFloorZ * MeshScale);
}

FVector ATrainingHubBuilder::GetTrainingPlayerOffset()
{
	return FVector(0.0f, -2500.0f, 250.0f);
}

FVector ATrainingHubBuilder::GetFallbackFloorExtent(const FBoxSphereBounds& MeshBounds, float MeshScale)
{
	constexpr float Padding = 600.0f;
	constexpr float FloorHalfThickness = 100.0f;
	return FVector(
		MeshBounds.BoxExtent.X * MeshScale + Padding,
		MeshBounds.BoxExtent.Y * MeshScale + Padding,
		FloorHalfThickness);
}

FVector ATrainingHubBuilder::GetFallbackFloorLocation(const FVector& RegionLocation)
{
	return RegionLocation - FVector(0.0f, 0.0f, GetFallbackFloorExtent(FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f), 1.0f).Z);
}

float ATrainingHubBuilder::GetTrainingSunIntensity()
{
	return 8.0f;
}

float ATrainingHubBuilder::GetTrainingSkyLightIntensity()
{
	return 0.6f;
}

const TCHAR* ATrainingHubBuilder::GetTrainingSkyLightCubemapPath()
{
	return TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap");
}

bool ATrainingHubBuilder::ShouldHideFallbackFloorInGame()
{
	return true;
}

void ATrainingHubBuilder::BeginPlay()
{
	Super::BeginPlay();

	const TArray<FTrainingRegionDefinition> Regions = BuildRegionDefinitions();
	SpawnLighting();

	for (const FTrainingRegionDefinition& Region : Regions)
	{
		SpawnRegion(Region);
	}

	MovePlayerToFirstRegion(Regions);
	UE_LOG(LogTemp, Log, TEXT("TrainingHubBuilder: spawned %d training regions"), Regions.Num());
}

TArray<FTrainingRegionDefinition> ATrainingHubBuilder::BuildRegionDefinitions() const
{
	constexpr float Spacing = 18000.0f;
	const FVector BaseLocations[] = {
		FVector(-2.5f * Spacing, 0.0f, 0.0f),
		FVector(-1.5f * Spacing, 0.0f, 0.0f),
		FVector(-0.5f * Spacing, 0.0f, 0.0f),
		FVector(0.5f * Spacing, 0.0f, 0.0f),
		FVector(1.5f * Spacing, 0.0f, 0.0f),
		FVector(2.5f * Spacing, 0.0f, 0.0f),
	};

	const TCHAR* RegionNames[] = {
		TEXT("A_Site_Duel"),
		TEXT("B_Site_Duel"),
		TEXT("Mid_Duel"),
		TEXT("A_Main_Entry"),
		TEXT("B_Main_Entry"),
		TEXT("Market_Door_Duel"),
	};

	const TCHAR* DisplayNames[] = {
		TEXT("A Site"),
		TEXT("B Site"),
		TEXT("Mid"),
		TEXT("A Main"),
		TEXT("B Main"),
		TEXT("Market Door"),
	};

	const float SourceFloorZValues[] = {
		69.6f,
		65.4f,
		68.0f,
		70.0f,
		68.0f,
		68.0f,
	};

	TArray<FTrainingRegionDefinition> Regions;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RegionNames); ++Index)
	{
		FTrainingRegionDefinition Region;
		Region.RegionName = RegionNames[Index];
		Region.DisplayName = DisplayNames[Index];
		Region.Location = BaseLocations[Index];
		Region.MeshPath = FSoftObjectPath(FString::Printf(
			TEXT("/Game/AscentDuelRegions/%s/%s.%s"),
			RegionNames[Index],
			RegionNames[Index],
			RegionNames[Index]));
		Region.SourceFloorZ = SourceFloorZValues[Index];
		Regions.Add(Region);
	}
	return Regions;
}

void ATrainingHubBuilder::SpawnRegion(const FTrainingRegionDefinition& Region)
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(Region.MeshPath.TryLoad());
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("TrainingHubBuilder: missing region mesh %s"), *Region.MeshPath.ToString());
		return;
	}

	const FVector MeshActorLocation = GetGroundedRegionActorLocation(Region.Location, Mesh->GetBounds(), RegionScale, Region.SourceFloorZ);
	AStaticMeshActor* MeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(MeshActorLocation, FRotator::ZeroRotator);
	if (!MeshActor)
	{
		return;
	}

#if WITH_EDITOR
	MeshActor->SetActorLabel(FString::Printf(TEXT("Region_%s"), *Region.DisplayName));
#endif
	MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	MeshActor->SetActorScale3D(FVector(RegionScale));
	MeshActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshActor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

	SpawnFallbackFloor(Region, Mesh->GetBounds());

	const FVector LeftTarget = Region.Location + FVector(-900.0f, -TargetNearDistance, 250.0f);
	const FVector RightTarget = Region.Location + FVector(900.0f, -TargetNearDistance, 250.0f);
	const FVector DeepTarget = Region.Location + FVector(0.0f, TargetDeepDistance, 250.0f);
	SpawnTarget(LeftTarget, FString::Printf(TEXT("Bot_%s_Left"), *Region.DisplayName));
	SpawnTarget(RightTarget, FString::Printf(TEXT("Bot_%s_Right"), *Region.DisplayName));
	SpawnTarget(DeepTarget, FString::Printf(TEXT("Bot_%s_Deep"), *Region.DisplayName));
}

void ATrainingHubBuilder::SpawnTarget(const FVector& Location, const FString& Label)
{
	TSubclassOf<AEnemyTarget> SpawnClass = TargetClass;
	if (!SpawnClass)
	{
		SpawnClass = AEnemyTarget::StaticClass();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnemyTarget* Target = GetWorld()->SpawnActor<AEnemyTarget>(SpawnClass, Location, FRotator(0.0f, 180.0f, 0.0f), Params);
	if (Target)
	{
#if WITH_EDITOR
		Target->SetActorLabel(Label);
#endif
		Target->SetPeekPositions(Location, Location);
		Target->StartPeek();
	}
}

void ATrainingHubBuilder::SpawnFallbackFloor(const FTrainingRegionDefinition& Region, const FBoxSphereBounds& MeshBounds)
{
	AStaticMeshActor* FloorActor = GetWorld()->SpawnActor<AStaticMeshActor>(GetFallbackFloorLocation(Region.Location), FRotator::ZeroRotator);
	if (!FloorActor)
	{
		return;
	}

#if WITH_EDITOR
	FloorActor->SetActorLabel(FString::Printf(TEXT("FallbackFloor_%s"), *Region.DisplayName));
#endif

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* Floor = FloorActor->GetStaticMeshComponent();
	if (!CubeMesh || !Floor)
	{
		FloorActor->Destroy();
		return;
	}

	const FVector FloorExtent = GetFallbackFloorExtent(MeshBounds, RegionScale);
	Floor->SetStaticMesh(CubeMesh);
	Floor->SetWorldScale3D(FloorExtent / 50.0f);
	Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->SetCollisionObjectType(ECC_WorldStatic);
	Floor->SetCollisionResponseToAllChannels(ECR_Block);
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->SetVisibility(!ShouldHideFallbackFloorInGame());
	Floor->SetHiddenInGame(ShouldHideFallbackFloorInGame());
}

void ATrainingHubBuilder::SpawnLighting()
{
	ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0.0f, -12000.0f, 12000.0f), FRotator(-45.0f, 0.0f, 0.0f));
	if (Sun && Sun->GetLightComponent())
	{
		Sun->GetLightComponent()->SetIntensity(GetTrainingSunIntensity());
	}
	ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(FVector(0.0f, 0.0f, 4000.0f), FRotator::ZeroRotator);
	if (Sky && Sky->GetLightComponent())
	{
		USkyLightComponent* SkyLight = Sky->GetLightComponent();
		SkyLight->SetMobility(EComponentMobility::Movable);
		SkyLight->SetIntensity(GetTrainingSkyLightIntensity());
		SkyLight->bLowerHemisphereIsBlack = false;
		if (UTextureCube* AmbientCubemap = LoadObject<UTextureCube>(nullptr, GetTrainingSkyLightCubemapPath()))
		{
			SkyLight->SourceType = SLS_SpecifiedCubemap;
			SkyLight->SetCubemap(AmbientCubemap);
		}
	}

	APostProcessVolume* Exposure = GetWorld()->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Exposure)
	{
		Exposure->bUnbound = true;
		Exposure->Settings.bOverride_AutoExposureMinBrightness = true;
		Exposure->Settings.bOverride_AutoExposureMaxBrightness = true;
		Exposure->Settings.bOverride_AutoExposureBias = true;
		Exposure->Settings.bOverride_BloomIntensity = true;
		Exposure->Settings.AutoExposureMinBrightness = 1.0f;
		Exposure->Settings.AutoExposureMaxBrightness = 1.0f;
		Exposure->Settings.AutoExposureBias = 0.0f;
		Exposure->Settings.BloomIntensity = 0.0f;
	}
}

void ATrainingHubBuilder::MovePlayerToFirstRegion(const TArray<FTrainingRegionDefinition>& Regions)
{
	if (Regions.Num() == 0)
	{
		return;
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!Pawn || !PC)
	{
		return;
	}

	const FVector SpawnLocation = Regions[0].Location + GetTrainingPlayerOffset();
	Pawn->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(GetTrainingRegionFacingRotation());
}
