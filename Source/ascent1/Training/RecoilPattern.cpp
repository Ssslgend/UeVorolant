#include "RecoilPattern.h"

FVector2D URecoilPattern::GetNextRecoilOffset()
{
	if (RecoilSteps.Num() == 0) return FVector2D::ZeroVector;

	const FRecoilStep& Step = RecoilSteps[CurrentStep];
	FVector2D Offset(Step.PitchOffset, Step.YawOffset);

	CurrentStep++;
	if (CurrentStep >= RecoilSteps.Num())
	{
		CurrentStep = FMath::Max(0, RecoilSteps.Num() - 5);
	}

	CurrentRecoil += Offset;
	return Offset;
}

void URecoilPattern::Reset()
{
	CurrentStep = 0;
	CurrentRecoil = FVector2D::ZeroVector;
}
