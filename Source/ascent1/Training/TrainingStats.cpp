#include "TrainingStats.h"

void FTrainingSession::RecordShot(bool bHit, bool bHeadshot, float ReactionTimeMs)
{
	TotalShots++;
	if (bHit)
	{
		TotalHits++;
		if (bHeadshot)
		{
			Headshots++;
		}
		ReactionTimes.Add(ReactionTimeMs);
		if (ReactionTimeMs < BestReactionTimeMs)
		{
			BestReactionTimeMs = ReactionTimeMs;
		}
	}
}

FTrainingSummary FTrainingSession::GetSummary() const
{
	FTrainingSummary Summary;
	Summary.TotalHits = TotalHits;
	Summary.TotalShots = TotalShots;
	Summary.Headshots = Headshots;

	if (TotalHits > 0)
	{
		Summary.HeadshotRate = static_cast<float>(Headshots) / TotalHits * 100.0f;
	}
	else
	{
		Summary.HeadshotRate = 0.0f;
	}

	if (TotalShots > 0)
	{
		Summary.HitRate = static_cast<float>(TotalHits) / TotalShots * 100.0f;
	}
	else
	{
		Summary.HitRate = 0.0f;
	}

	float Sum = 0.0f;
	for (float T : ReactionTimes) { Sum += T; }
	Summary.AverageReactionTimeMs = ReactionTimes.Num() > 0 ? Sum / ReactionTimes.Num() : 0.0f;
	Summary.BestReactionTimeMs = BestReactionTimeMs < std::numeric_limits<float>::infinity() ? BestReactionTimeMs : 0.0f;

	return Summary;
}

void FTrainingSession::Reset()
{
	ReactionTimes.Empty();
	TotalHits = 0;
	TotalShots = 0;
	Headshots = 0;
	BestReactionTimeMs = std::numeric_limits<float>::infinity();
}
