#include "AscGameState.h"
#include "Net/UnrealNetwork.h"
#include <limits>

void AAscGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAscGameState, TotalSpots);
	DOREPLIFETIME(AAscGameState, CompletedSpots);
	DOREPLIFETIME(AAscGameState, TotalHits);
	DOREPLIFETIME(AAscGameState, TotalShots);
	DOREPLIFETIME(AAscGameState, Headshots);
	DOREPLIFETIME(AAscGameState, BestReactionTimeMs);
}

void AAscGameState::RecordShot(bool bHit, bool bHeadshot, float ReactionTimeMs)
{
	TotalShots++;
	if (bHit)
	{
		TotalHits++;
		if (bHeadshot)
		{
			Headshots++;
		}
		if (ReactionTimeMs < BestReactionTimeMs)
		{
			BestReactionTimeMs = ReactionTimeMs;
		}
	}
}

float AAscGameState::GetHeadshotRate() const
{
	if (TotalHits == 0) return 0.0f;
	return static_cast<float>(Headshots) / TotalHits * 100.0f;
}

float AAscGameState::GetHitRate() const
{
	if (TotalShots == 0) return 0.0f;
	return static_cast<float>(TotalHits) / TotalShots * 100.0f;
}
