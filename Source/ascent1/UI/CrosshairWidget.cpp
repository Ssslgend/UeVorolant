#include "CrosshairWidget.h"
#include "TimerManager.h"

void UCrosshairWidget::UpdateSpread(float Spread)
{
	CurrentSpread = Spread;
}

void UCrosshairWidget::ShowHitMarker(bool bHeadshot)
{
	bShowingHitMarker = true;
	GetWorld()->GetTimerManager().SetTimer(HitMarkerTimerHandle, this,
		&UCrosshairWidget::HideHitMarker, HitMarkerDuration, false);
}

void UCrosshairWidget::HideHitMarker()
{
	bShowingHitMarker = false;
}
