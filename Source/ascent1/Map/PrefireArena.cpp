#include "PrefireArena.h"
#include "SpotLoader.h"
#include "../Core/AscGameState.h"
#include "../Player/AscCharacter.h"
#include "../Player/AscWeaponComponent.h"
#include "../Target/EnemyTarget.h"
#include "../Target/EnemySpawner.h"
#include "../Training/TrainerDuel.h"
#include "../Training/TrainingHubBuilder.h"
#include "../Training/TrainingSettings.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

APrefireArena::APrefireArena()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentSpotIndex = -1;
}

void APrefireArena::BeginPlay()
{
	Super::BeginPlay();
}

bool APrefireArena::EnsurePlayableMap()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Already present in the level?
	if (AActor* Existing = UGameplayStatics::GetActorOfClass(World, AStaticMeshActor::StaticClass()))
	{
		if (AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Existing))
		{
			if (SMA->GetStaticMeshComponent() &&
				SMA->GetStaticMeshComponent()->GetStaticMesh() &&
				SMA->GetStaticMeshComponent()->GetStaticMesh()->GetName().Contains(TEXT("AscentPlayable")))
			{
				return true;
			}
		}
	}

	const FTrainerSettings Settings = UTrainingSettings::Load();
	const FString MeshPath = Settings.Map.MeshPath;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("PrefireArena: missing map mesh %s"), *MeshPath);
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* MapActor = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!MapActor)
	{
		return false;
	}

#if WITH_EDITOR
	MapActor->SetActorLabel(TEXT("AscentPlayable"));
#endif
	UStaticMeshComponent* Comp = MapActor->GetStaticMeshComponent();
	Comp->SetStaticMesh(Mesh);
	Comp->SetMobility(EComponentMobility::Static);
	// QueryAndPhysics block-all so ground snap / walk / LOS all hit the map.
	Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Comp->SetCollisionObjectType(ECC_WorldStatic);
	Comp->SetCollisionResponseToAllChannels(ECR_Block);

	// FBX was authored in Blender meters; UE world/cm spots need *100.
	const FBoxSphereBounds UnscaledBounds = Comp->Bounds;
	const float MeshScale = FTrainerSettings::ResolveMeshScale(UnscaledBounds.BoxExtent.GetMax(), Settings.Map.MeshScale);
	MapActor->SetActorScale3D(FVector(MeshScale));

	const FBoxSphereBounds Bounds = Comp->Bounds;
	UE_LOG(LogTemp, Log, TEXT("PrefireArena: spawned map %s scale=%.1f origin=%s unscaledExtent=%s worldExtent=%s"),
		*MeshPath, MeshScale, *UnscaledBounds.Origin.ToString(),
		*UnscaledBounds.BoxExtent.ToString(), *Bounds.BoxExtent.ToString());

	// Lighting: Direct sun + captured-scene skylight (no baked lighting).
	// Shell meshes are single-sided; fill indirect so backfaces/black voids read.
	if (!UGameplayStatics::GetActorOfClass(World, ADirectionalLight::StaticClass()))
	{
		if (ADirectionalLight* NewSun = World->SpawnActor<ADirectionalLight>(
			FVector(0, 0, 3000), FRotator(-35.f, 40.f, 0.f)))
		{
#if WITH_EDITOR
			NewSun->SetActorLabel(TEXT("TrainerSun"));
#endif
		}
	}

	if (!UGameplayStatics::GetActorOfClass(World, ASkyLight::StaticClass()))
	{
		if (ASkyLight* NewSky = World->SpawnActor<ASkyLight>(FVector(0, 0, 2000), FRotator::ZeroRotator))
		{
#if WITH_EDITOR
			NewSky->SetActorLabel(TEXT("TrainerSky"));
#endif
			if (USkyLightComponent* SkyComp = NewSky->GetLightComponent())
			{
				SkyComp->SourceType = ESkyLightSourceType::SLS_CapturedScene;
				SkyComp->SetIntensity(1.0f);
				SkyComp->SetLightColor(FLinearColor(0.72f, 0.78f, 0.92f));
				SkyComp->SetCastShadows(false);
				SkyComp->MarkRenderStateDirty();
			}
		}
	}

	// Two-sided material is unavailable as an engine default; use the World Grid
	// so Shell geometry reads clearly even without textures.
	if (Comp)
	{
		if (UMaterialInterface* GridMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")))
		{
			Comp->SetMaterial(0, GridMat);
		}
		Comp->bCastDynamicShadow = true;
		Comp->bCastStaticShadow = true;
	}

	return true;
}

bool APrefireArena::AlignMapToSpots(const TArray<FPrefireSpot>& InSpots)
{
	if (InSpots.Num() == 0)
	{
		return false;
	}

	AStaticMeshActor* MapActor = nullptr;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		if (It->GetStaticMeshComponent() && It->GetStaticMeshComponent()->GetStaticMesh() &&
			It->GetStaticMeshComponent()->GetStaticMesh()->GetName().Contains(TEXT("AscentPlayable")))
		{
			MapActor = *It;
			break;
		}
	}
	if (!MapActor || !MapActor->GetStaticMeshComponent())
	{
		return false;
	}

	// Drop legacy TrainingHub gallery meshes / lights so only AscentPlayable remains.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const FString Name = It->GetName();
		FString Label = Name;
#if WITH_EDITOR
		Label = It->GetActorLabel();
#endif
		if (Name.Contains(TEXT("TrainingHub")) || Label.Contains(TEXT("Region")) ||
			Label.Contains(TEXT("FallbackFloor")) || Label.Contains(TEXT("Target_")) ||
			Label.Contains(TEXT("TrainingRegion")) || It->IsA<ATrainingHubBuilder>())
		{
			It->Destroy();
		}
	}

	const FPrefireSpot& Ref = InSpots[0];
	const FVector DesiredFloor(Ref.Player.GamePosition.X, Ref.Player.GamePosition.Y, Ref.Player.GamePosition.Z);
	const FBox WorldBounds = MapActor->GetComponentsBoundingBox(true);

	const bool bInside =
		DesiredFloor.X >= WorldBounds.Min.X - 100.0f && DesiredFloor.X <= WorldBounds.Max.X + 100.0f &&
		DesiredFloor.Y >= WorldBounds.Min.Y - 100.0f && DesiredFloor.Y <= WorldBounds.Max.Y + 100.0f &&
		DesiredFloor.Z >= WorldBounds.Min.Z - 500.0f && DesiredFloor.Z <= WorldBounds.Max.Z + 500.0f;

	// Mesh is already in Blender absolute cm after *100 scale. If the spot sits
	// inside the bounds, DO NOT shift — a missed probe is just missing collision.
	if (bInside)
	{
		UE_LOG(LogTemp, Log, TEXT("PrefireArena: map already covers ref spot, no shift. bounds=%s..%s"),
			*WorldBounds.Min.ToString(), *WorldBounds.Max.ToString());
		return true;
	}

	// Spot is outside: recentre XY onto the spot, keep relative layout of other spots.
	const FVector BoundsCenter = WorldBounds.GetCenter();
	MapActor->AddActorWorldOffset(FVector(
		DesiredFloor.X - BoundsCenter.X,
		DesiredFloor.Y - BoundsCenter.Y,
		0.0f));
	const FBox Shifted = MapActor->GetComponentsBoundingBox(true);
	const float DeltaZ = DesiredFloor.Z - FMath::Clamp(DesiredFloor.Z, Shifted.Min.Z, Shifted.Max.Z);
	if (!FMath::IsNearlyZero(DeltaZ, 1.0f))
	{
		MapActor->AddActorWorldOffset(FVector(0.0f, 0.0f, DeltaZ));
	}

	const FBox FinalBounds = MapActor->GetComponentsBoundingBox(true);
	UE_LOG(LogTemp, Log, TEXT("PrefireArena: map shifted to cover spot bounds=%s..%s"),
		*FinalBounds.Min.ToString(), *FinalBounds.Max.ToString());
	return true;
}

void APrefireArena::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ActiveDuel && ActiveDuel->GetState() == ETrainerDuelState::Ready)
	{
		if (AAscCharacter* PC = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
		{
			// Ignore residual teleport velocity: only a real walk starts the peek.
			if (PC->GetVelocity().Size2D() > 50.0f)
			{
				ActiveDuel->NotifyPlayerMovedOrFired();
			}
		}
	}
}

ETrainerDuelState APrefireArena::GetDuelState() const
{
	return ActiveDuel ? ActiveDuel->GetState() : ETrainerDuelState::Ready;
}

bool APrefireArena::ResolveSpotTransforms(UWorld* World, const FPrefireSpot& Spot, FSpotResolution& Out)
{
	const FVector RawPlayer = Spot.GetPlayerUEPos();
	const FVector RawEnemy = Spot.GetEnemyUEPos();
	const FTrainerSettings Settings = UTrainingSettings::Load();

	// JSON pos.z is the floor height (from the playable map report). A zero z
	// asks for a downward ray snap; non-zero is a forced floor height.
	FVector PlayerFloor = RawPlayer;
	FVector EnemyFloor = RawEnemy;
	Out.bPlayerGrounded = USpotLoader::ResolveGroundedPosition(World, RawPlayer, PlayerFloor);
	Out.bEnemyGrounded = USpotLoader::ResolveGroundedPosition(World, RawEnemy, EnemyFloor);

	if (!Out.bPlayerGrounded)
	{
		PlayerFloor = RawPlayer;
	}
	if (!Out.bEnemyGrounded)
	{
		EnemyFloor = RawEnemy;
	}

	Out.PlayerPos = Settings.FloorToActorLocation(PlayerFloor, false);
	Out.EnemyPos = Settings.FloorToActorLocation(EnemyFloor, false);

	// Prefer explicit yaw from JSON; else face each other.
	if (!FMath::IsNearlyZero(Spot.Player.Yaw) || !FMath::IsNearlyZero(Spot.Enemy.Yaw))
	{
		Out.PlayerRot = FRotator(0.0f, Spot.Player.Yaw, 0.0f);
		Out.EnemyRot = FRotator(0.0f, Spot.Enemy.Yaw, 0.0f);
	}
	else
	{
		Out.PlayerRot = (Out.EnemyPos - Out.PlayerPos).Rotation();
		Out.EnemyRot = (Out.PlayerPos - Out.EnemyPos).Rotation();
	}
	// Keep yaw-only facing for duel readability
	Out.PlayerRot.Pitch = 0.0f;
	Out.PlayerRot.Roll = 0.0f;
	Out.EnemyRot.Pitch = 0.0f;
	Out.EnemyRot.Roll = 0.0f;

	return Out.bPlayerGrounded && Out.bEnemyGrounded;
}

void APrefireArena::LoadAndBuild(const FString& JsonPath)
{
	const FString Path = JsonPath.IsEmpty() ? DefaultSpotFile : JsonPath;
	const FSpotLoadResult Result = USpotLoader::LoadSpotsFromFile(Path);

	// Bring in the user-exported Ascent geometry, then align it to spot space.
	EnsurePlayableMap();
	AlignMapToSpots(Result.Spots);

	Spots = Result.Spots;
	LoadWarnings = Result.Warnings;

	for (const FString& Warning : LoadWarnings)
	{
		UE_LOG(LogTemp, Warning, TEXT("PrefireArena: %s"), *Warning);
	}

	if (Spots.Num() > 0)
	{
		StartSpot(0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PrefireArena: No spots loaded from %s"), *Path);
	}
}

void APrefireArena::StartSpot(int32 Index)
{
	if (Index < 0 || Index >= Spots.Num()) return;

	ClearCurrentSpot();
	CurrentSpotIndex = Index;

	const FPrefireSpot& Spot = Spots[Index];
	FSpotResolution Res;
	const bool bGrounded = ResolveSpotTransforms(GetWorld(), Spot, Res);

	if (!bGrounded)
	{
		UE_LOG(LogTemp, Error, TEXT("PrefireArena: spot '%s' failed ground snap — skipped"), *Spot.Id);
		LoadWarnings.Add(FString::Printf(TEXT("spot '%s' failed ground snap"), *Spot.Id));
		// Skip to next valid spot
		int32 Next = Index + 1;
		while (Next < Spots.Num())
		{
			FSpotResolution Probe;
			if (ResolveSpotTransforms(GetWorld(), Spots[Next], Probe))
			{
				StartSpot(Next);
				return;
			}
			++Next;
		}
		CurrentSpotIndex = -1;
		return;
	}

	TeleportPlayer(Res.PlayerPos, Res.PlayerRot);
	SpawnCoverWall(Res.PlayerPos, Res.EnemyPos);
	BindWeaponShot();

	UE_LOG(LogTemp, Log, TEXT("PrefireArena: StartSpot[%d] '%s' player=(%.0f,%.0f,%.0f) enemy=(%.0f,%.0f,%.0f) grounded=%d/%d"),
		Index, *Spot.Id,
		Res.PlayerPos.X, Res.PlayerPos.Y, Res.PlayerPos.Z,
		Res.EnemyPos.X, Res.EnemyPos.Y, Res.EnemyPos.Z,
		Res.bPlayerGrounded ? 1 : 0, Res.bEnemyGrounded ? 1 : 0);

	AEnemySpawner* Spawner = Cast<AEnemySpawner>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AEnemySpawner::StaticClass()));
	if (!Spawner)
	{
		Spawner = GetWorld()->SpawnActor<AEnemySpawner>();
	}
	ActiveBot = Spawner->SpawnBot(Res.EnemyPos, Res.EnemyRot);

	if (!ActiveDuel)
	{
		ActiveDuel = GetWorld()->SpawnActor<ATrainerDuel>();
	}

	if (ActiveBot && ActiveDuel)
	{
		FTrainerSettings Settings = UTrainingSettings::Load();
		if (Spot.Bot.DelayMs >= 0.0f)
		{
			Settings.Bot.DelayMs = Spot.Bot.DelayMs;
		}
		ActiveBot->ConfigureHitVolumes(Settings.Hit.HeadRadiusCm, Settings.Hit.StandEyeHeightCm);
		if (AAscCharacter* PC = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
		{
			PC->ApplyTrainerSettings(Settings);
			if (UAscWeaponComponent* Weapon = PC->FindComponentByClass<UAscWeaponComponent>())
			{
				Weapon->SetFrozen(false);
				Weapon->ApplySettings(Settings);
			}
		}
		ActiveDuel->BeginDuel(
			Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)),
			ActiveBot,
			Settings,
			Spot.Id,
			Spot.Name);
		ActiveDuel->OnDuelResult.AddDynamic(this, &APrefireArena::HandleDuelResult);
	}

	SpotStartTime = GetWorld()->GetTimeSeconds();
}

void APrefireArena::BotFire()
{
	if (ActiveDuel)
	{
		// Duel owns kill timing; this remains as a manual/debug hook.
		ActiveDuel->NotifyPlayerMovedOrFired();
	}
}

void APrefireArena::BindWeaponShot()
{
	if (AAscCharacter* PC = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		if (UAscWeaponComponent* Weapon = PC->FindComponentByClass<UAscWeaponComponent>())
		{
			Weapon->OnWeaponShot.AddUObject(this, &APrefireArena::HandleWeaponShot);
		}
	}
}

void APrefireArena::UnbindWeaponShot()
{
	if (AAscCharacter* PC = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		if (UAscWeaponComponent* Weapon = PC->FindComponentByClass<UAscWeaponComponent>())
		{
			Weapon->OnWeaponShot.RemoveAll(this);
		}
	}
}

void APrefireArena::HandleWeaponShot(const FVector& RayStart, const FVector& RayEnd, const FVector& ImpactPoint, bool bHitWorld)
{
	if (!ActiveBot || !ActiveDuel)
	{
		return;
	}

	const FVector Dir = (RayEnd - RayStart).GetSafeNormal();
	const FVector Head = ActiveBot->GetHeadCenter();
	const float HeadRadius = ActiveBot->GetHeadRadius();

	// World depth must ignore the bot itself: the body capsule sits around the
	// head sphere and would otherwise act as its own occluder.
	float WorldDist = TNumericLimits<float>::Max();
	{
		FHitResult WorldHit;
		FCollisionQueryParams WorldParams(SCENE_QUERY_STAT(TrainerShotWorld), /*bTraceComplex*/ false);
		WorldParams.AddIgnoredActor(ActiveBot);
		if (const AActor* ShotOwner = GetOwner())
		{
			WorldParams.AddIgnoredActor(ShotOwner);
		}
		if (GetWorld()->LineTraceSingleByChannel(WorldHit, RayStart, RayEnd, ECC_Visibility, WorldParams))
		{
			WorldDist = FVector::Dist(RayStart, WorldHit.ImpactPoint);
		}
	}

	// Analytic ray-sphere on the FULL untruncated ray. The body capsule would
	// otherwise swallow a first-hit Visibility trace before the tiny head sphere.
	const FVector ToHead = Head - RayStart;
	const float Tca = FVector::DotProduct(ToHead, Dir);
	float MissDistanceCm = TNumericLimits<float>::Max();
	bool bHeadHit = false;
	if (Tca > 0.0f)
	{
		const float PerpSq = FMath::Max(0.0f, ToHead.SizeSquared() - Tca * Tca);
		MissDistanceCm = FMath::Sqrt(PerpSq);
		const float HeadRadiusSq = HeadRadius * HeadRadius;
		if (PerpSq <= HeadRadiusSq)
		{
			// Entry distance into the head sphere; must be at or before the world hit.
			const float Thc = FMath::Sqrt(HeadRadiusSq - PerpSq);
			const float HitDist = Tca - Thc;
			bHeadHit = HitDist <= WorldDist + 1.0f;
		}
	}

	// Body hit: the first Visibility impact landed on the bot (not the head).
	bool bBodyHit = false;
	if (!bHeadHit && bHitWorld)
	{
		if (ImpactPoint.Equals(ActiveBot->GetHeadCenter(), 1.0f) ||
			FVector::Dist(ImpactPoint, ActiveBot->GetActorLocation()) <= ActiveBot->GetCapsuleRadius() * 2.5f)
		{
			bBodyHit = true;
		}
	}

	if (bHeadHit || bBodyHit)
	{
		ActiveBot->OnShotByPlayer(bHeadHit ? Head : ImpactPoint);
	}

	ActiveDuel->NotifyPlayerShot(bHeadHit, bHeadHit ? 0.0f : MissDistanceCm);

	if (AAscGameState* GS = GetWorld()->GetGameState<AAscGameState>())
	{
		GS->RecordShot(bHeadHit || bBodyHit, bHeadHit, ActiveDuel->GetReactionTimeMs());
	}
}

void APrefireArena::OnPlayerHitBot(AAscCharacter* Player, const FVector& HitPoint, bool bHeadHit, float DistanceToHeadCm)
{
	if (!ActiveBot || !ActiveDuel) return;

	const EHitRegion Region = ActiveBot->OnShotByPlayer(HitPoint);
	const bool bHead = bHeadHit || Region == EHitRegion::Head;
	const float MissDistance = bHead ? 0.0f : FMath::Max(DistanceToHeadCm, ActiveBot->GetDistanceToHead(HitPoint));

	ActiveDuel->NotifyPlayerShot(bHead, MissDistance);
}

void APrefireArena::HandleDuelResult(FTrainerDuelResult Result)
{
	// Freeze the weapon for the RESULT window (S2.2).
	if (AAscCharacter* PC = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		if (UAscWeaponComponent* Weapon = PC->FindComponentByClass<UAscWeaponComponent>())
		{
			Weapon->SetFrozen(true);
		}
	}

	if (Result.bSuccess)
	{
		OnSpotCompleted.Broadcast(CurrentSpotIndex);
	}
	else
	{
		OnPlayerKilled.Broadcast(Result.ReactionTimeMs);
	}
	OnSpotResult.Broadcast(CurrentSpotIndex, Result.bSuccess, Result.ReactionTimeMs);
	UE_LOG(LogTemp, Log, TEXT("PrefireArena: %s"), *Result.ToLogLine());
}

void APrefireArena::NextSpot()
{
	ClearCurrentSpot();
	int32 Next = CurrentSpotIndex + 1;
	if (Next < Spots.Num())
	{
		StartSpot(Next);
	}
}

void APrefireArena::RestartSpot()
{
	ClearCurrentSpot();
	if (CurrentSpotIndex >= 0 && CurrentSpotIndex < Spots.Num())
	{
		StartSpot(CurrentSpotIndex);
	}
	if (AAscCharacter* PC = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		if (UAscWeaponComponent* Weapon = PC->FindComponentByClass<UAscWeaponComponent>())
		{
			Weapon->SetFrozen(false);
		}
	}
}

void APrefireArena::SpawnCoverWall(const FVector& PlayerPos, const FVector& EnemyPos)
{
	if (!CoverWallClass) return;

	FVector MidPoint = PlayerPos + (EnemyPos - PlayerPos).GetSafeNormal() * CoverDistance;
	FRotator Rotation = (EnemyPos - PlayerPos).Rotation();
	ActiveCoverWall = GetWorld()->SpawnActor<AActor>(CoverWallClass, MidPoint, Rotation);
}

void APrefireArena::TeleportPlayer(const FVector& Location, const FRotator& Rotation)
{
	AAscCharacter* PC = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!PC) return;

	PC->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = PC->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
	}
	PC->GetController()->SetControlRotation(Rotation);
}

void APrefireArena::ClearCurrentSpot()
{
	UnbindWeaponShot();

	if (ActiveBot)
	{
		if (AEnemySpawner* Spawner = Cast<AEnemySpawner>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AEnemySpawner::StaticClass())))
		{
			Spawner->DespawnAll();
		}
		ActiveBot = nullptr;
	}

	if (ActiveCoverWall)
	{
		ActiveCoverWall->Destroy();
		ActiveCoverWall = nullptr;
	}

	if (ActiveDuel)
	{
		ActiveDuel->Destroy();
		ActiveDuel = nullptr;
	}

	GetWorldTimerManager().ClearTimer(BotFireTimerHandle);
}
