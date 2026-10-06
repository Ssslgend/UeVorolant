#include "SpotData.h"

FVector FPrefireSpot::GetPlayerUEPos() const
{
	return FVector(
		static_cast<float>(Player.GamePosition.X),
		static_cast<float>(Player.GamePosition.Y),
		static_cast<float>(Player.GamePosition.Z));
}

FVector FPrefireSpot::GetEnemyUEPos() const
{
	return FVector(
		static_cast<float>(Enemy.GamePosition.X),
		static_cast<float>(Enemy.GamePosition.Y),
		static_cast<float>(Enemy.GamePosition.Z));
}
