#include "StatsPanelWidget.h"

void UStatsPanelWidget::Refresh(const FTrainingSummary& Stats)
{
	HeadshotRate = Stats.HeadshotRate;
	HitRate = Stats.HitRate;
	AverageReactionTimeMs = Stats.AverageReactionTimeMs;
	BestReactionTimeMs = Stats.BestReactionTimeMs;
	TotalHits = Stats.TotalHits;
	TotalShots = Stats.TotalShots;
	Headshots = Stats.Headshots;
}

void UStatsPanelWidget::ToggleVisibility()
{
	ESlateVisibility Current = GetVisibility();
	SetVisibility(Current == ESlateVisibility::Visible
		? ESlateVisibility::Collapsed
		: ESlateVisibility::Visible);
}
