#include "PrefireArena.h"
#include "SpotLoader.h"
#include "../Player/AscCharacter.h"
#include "../Target/EnemyTarget.h"
#include "../Target/EnemySpawner.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

APrefireArena::APrefireArena()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentSpotIndex = -1;
}

void APrefireArena::BeginPlay()
{
	Super::BeginPlay();
}

void APrefireArena::LoadAndBuild(const FString& JsonPath)
{
	Spots = USpotLoader::LoadSpotsFromFile(JsonPath);
	if (Spots.Num() > 0)
	{
		StartSpot(0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PrefireArena: No spots loaded from %s"), *JsonPath);
	}
}

void APrefireArena::StartSpot(int32 Index)
{
	if (Index < 0 || Index >= Spots.Num()) return;

	ClearCurrentSpot();
	CurrentSpotIndex = Index;

	const FPrefireSpot& Spot = Spots[Index];
	FVector PlayerPos = USpotLoader::ValToUE(Spot.Player.GamePosition);
	FVector EnemyPos = USpotLoader::ValToUE(Spot.Enemy.GamePosition);
	FRotator PlayerRot = (EnemyPos - PlayerPos).Rotation();
	FRotator EnemyRot = (PlayerPos - EnemyPos).Rotation();

	TeleportPlayer(PlayerPos, PlayerRot);

	SpawnCoverWall(PlayerPos, EnemyPos);

	AEnemySpawner* Spawner = Cast<AEnemySpawner>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AEnemySpawner::StaticClass()));
	if (!Spawner)
	{
		Spawner = GetWorld()->SpawnActor<AEnemySpawner>();
	}
	ActiveBot = Spawner->SpawnBot(EnemyPos, EnemyRot);

	if (ActiveBot)
	{
		ActiveBot->SetPeekPositions(EnemyPos, EnemyPos); // static bot for now
		ActiveBot->StartPeek();
	}

	GetWorldTimerManager().SetTimer(BotFireTimerHandle, this, &APrefireArena::BotFire, BotFireDelay, false);
	SpotStartTime = GetWorld()->GetTimeSeconds();
}

void APrefireArena::BotFire()
{
	float ReactionTime = (GetWorld()->GetTimeSeconds() - SpotStartTime) * 1000.0f;
	OnPlayerKilled.Broadcast(ReactionTime);
	RestartSpot();
}

void APrefireArena::OnPlayerHitBot(AAscCharacter* Player, const FVector& HitPoint)
{
	if (!ActiveBot) return;

	float ReactionTime = (GetWorld()->GetTimeSeconds() - SpotStartTime) * 1000.0f;

	BotFireTimerHandle.Invalidate();
	ActiveBot->OnShotByPlayer(HitPoint);

	if (!IsValid(ActiveBot)) // headshot = destroy
	{
		ActiveBot = nullptr;
		OnSpotCompleted.Broadcast(CurrentSpotIndex);
	}
	else
	{
		RestartSpot();
	}
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
}

void APrefireArena::SpawnCoverWall(const FVector& PlayerPos, const FVector& EnemyPos)
{
	if (!CoverWallClass) return;

	FVector MidPoint = PlayerPos + (EnemyPos - PlayerPos).GetSafeNormal() * CoverDistance;
	FRotator Rotation = (EnemyPos - PlayerPos).Rotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveCoverWall = GetWorld()->SpawnActor<AActor>(CoverWallClass,
		MidPoint + FVector(0, 0, CoverHeight * 0.5f), Rotation, Params);
}

void APrefireArena::TeleportPlayer(const FVector& Location, const FRotator& Rotation)
{
	AAscCharacter* Player = Cast<AAscCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (Player)
	{
		Player->SetActorLocation(Location);
		if (Player->GetController())
		{
			Player->GetController()->SetControlRotation(Rotation);
		}
	}
}

void APrefireArena::ClearCurrentSpot()
{
	BotFireTimerHandle.Invalidate();
	if (ActiveCoverWall)
	{
		ActiveCoverWall->Destroy();
		ActiveCoverWall = nullptr;
	}
	if (ActiveBot)
	{
		ActiveBot->Destroy();
		ActiveBot = nullptr;
	}
}
