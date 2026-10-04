#include "TrainingHUD.h"
#include "../Core/AscGameState.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"

void ATrainingHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const FLinearColor CrosshairColor = FLinearColor::White;
	const float Gap = 6.0f;
	const float Length = 10.0f;

	FCanvasLineItem Left(FVector2D(Center.X - Gap - Length, Center.Y), FVector2D(Center.X - Gap, Center.Y));
	FCanvasLineItem Right(FVector2D(Center.X + Gap, Center.Y), FVector2D(Center.X + Gap + Length, Center.Y));
	FCanvasLineItem Top(FVector2D(Center.X, Center.Y - Gap - Length), FVector2D(Center.X, Center.Y - Gap));
	FCanvasLineItem Bottom(FVector2D(Center.X, Center.Y + Gap), FVector2D(Center.X, Center.Y + Gap + Length));
	for (FCanvasLineItem* Line : { &Left, &Right, &Top, &Bottom })
	{
		Line->SetColor(CrosshairColor);
		Line->LineThickness = 2.0f;
		Canvas->DrawItem(*Line);
	}

	AAscGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAscGameState>() : nullptr;
	const FString StatsLine = GS
		? FString::Printf(TEXT("Shots %d  Hits %d  HS %d  Hit %.1f%%  HS %.1f%%  Best %.0fms"),
			GS->TotalShots,
			GS->TotalHits,
			GS->Headshots,
			GS->GetHitRate(),
			GS->GetHeadshotRate(),
			GS->BestReactionTimeMs < TNumericLimits<float>::Max() ? GS->BestReactionTimeMs : 0.0f)
		: TEXT("Shots 0  Hits 0  HS 0");

	DrawText(StatsLine, FLinearColor::White, 24.0f, 24.0f, nullptr, 1.1f);
	DrawText(TEXT("1 A Site   2 B Site   3 Mid   4 A Main   5 B Main   6 Market/Door"),
		FLinearColor::Yellow, 24.0f, 54.0f, nullptr, 0.95f);
}
