#include "TrainingSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	float ReadNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Default)
	{
		if (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field))
		{
			return static_cast<float>(Obj->GetNumberField(Field));
		}
		return Default;
	}

	bool ReadBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Default)
	{
		if (Obj.IsValid() && Obj->HasTypedField<EJson::Boolean>(Field))
		{
			return Obj->GetBoolField(Field);
		}
		return Default;
	}

	FString ReadString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Default)
	{
		if (Obj.IsValid() && Obj->HasTypedField<EJson::String>(Field))
		{
			return Obj->GetStringField(Field);
		}
		return Default;
	}

	void ReadSection(const TSharedPtr<FJsonObject>& Root, const TCHAR* Section,
		TFunctionRef<void(const TSharedPtr<FJsonObject>&)> Apply)
	{
		if (!Root.IsValid())
		{
			return;
		}
		const TSharedPtr<FJsonObject>* SectionObj = nullptr;
		if (Root->TryGetObjectField(Section, SectionObj) && SectionObj->IsValid())
		{
			Apply(*SectionObj);
		}
	}

	/** Clamps InOut into [Min, Max] and counts the change. */
	int32 ClampField(float& InOut, float Min, float Max, const TCHAR* Field, int32& Changed)
	{
		const float Clamped = FMath::Clamp(InOut, Min, Max);
		if (!FMath::IsNearlyEqual(Clamped, InOut))
		{
			UE_LOG(LogTemp, Warning, TEXT("TrainingSettings: %s=%.3f out of range [%.2f, %.2f], clamped to %.3f"),
				Field, InOut, Min, Max, Clamped);
			InOut = Clamped;
			++Changed;
		}
		return Changed;
	}
}

float FTrainerSettings::ComputeCmPer360(float Sensitivity, float Dpi)
{
	if (Sensitivity <= 0.0f || Dpi <= 0.0f)
	{
		return 0.0f;
	}
	const float CountsPer360 = 360.0f / (ValorantDegreesPerCount * Sensitivity);
	return CountsPer360 / Dpi * 2.54f;
}

FVector FTrainerSettings::FloorToActorLocation(const FVector& FloorPoint, bool bCrouched) const
{
	const float HalfHeight = bCrouched ? Hit.CapsuleHalfHeightCm * 0.66f : Hit.CapsuleHalfHeightCm;
	return FVector(FloorPoint.X, FloorPoint.Y, FloorPoint.Z + HalfHeight);
}

float FTrainerSettings::ResolveMeshScale(float MeshMaxExtent, float ConfiguredScale)
{
	if (ConfiguredScale > 0.0f)
	{
		return ConfiguredScale;
	}
	// A ~200 m playable map is ~200 units in meters and ~20000 units in cm.
	return (MeshMaxExtent > 3000.0f) ? 1.0f : 100.0f;
}

int32 FTrainerSettings::Validate()
{
	int32 Changed = 0;
	ClampField(Movement.RunSpeed, 50.0f, 2000.0f, TEXT("movement.runSpeed"), Changed);
	ClampField(Movement.WalkMultiplier, 0.05f, 1.5f, TEXT("movement.walkMultiplier"), Changed);
	ClampField(Movement.CrouchMultiplier, 0.05f, 1.5f, TEXT("movement.crouchMultiplier"), Changed);
	ClampField(Movement.JumpZVelocity, 0.0f, 1200.0f, TEXT("movement.jumpZVelocity"), Changed);

	ClampField(Hit.HeadRadiusCm, 1.0f, 40.0f, TEXT("hit.headRadiusCm"), Changed);
	ClampField(Hit.StandEyeHeightCm, 90.0f, 240.0f, TEXT("hit.standEyeHeightCm"), Changed);
	ClampField(Hit.TorsoHeightCm, 60.0f, 200.0f, TEXT("hit.torsoHeightCm"), Changed);
	ClampField(Hit.CapsuleHalfHeightCm, 40.0f, 140.0f, TEXT("hit.capsuleHalfHeightCm"), Changed);

	ClampField(Bot.DelayMs, 0.0f, 2000.0f, TEXT("bot.delayMs"), Changed);
	ClampField(Bot.WindupMs, 0.0f, 1000.0f, TEXT("bot.windupMs"), Changed);

	ClampField(Duel.TimeoutMs, 500.0f, 600000.0f, TEXT("duel.timeoutMs"), Changed);
	ClampField(Duel.ResetDelayMs, 0.0f, 10000.0f, TEXT("duel.resetDelayMs"), Changed);

	ClampField(Map.MeshScale, 0.0f, 1000.0f, TEXT("map.meshScale"), Changed);
	ClampField(Map.GroundProbeHeightCm, 50.0f, 5000.0f, TEXT("map.groundProbeHeightCm"), Changed);

	ClampField(Input.ValorantSensitivity, 0.01f, 10.0f, TEXT("input.valorantSensitivity"), Changed);
	ClampField(Input.MouseDpi, 100.0f, 6400.0f, TEXT("input.mouseDpi"), Changed);

	return Changed;
}

FString UTrainingSettings::GetSettingsFilePath()
{
	return FPaths::ProjectContentDir() / TEXT("Data") / TEXT("trainer_settings.json");
}

void UTrainingSettings::ApplyTo(FTrainerSettings& Out, const FString& JsonString)
{
	Out = FTrainerSettings();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("TrainingSettings: failed to parse settings JSON, using defaults"));
		return;
	}

	ReadSection(Root, TEXT("movement"), [&](const TSharedPtr<FJsonObject>& Sec)
	{
		Out.Movement.RunSpeed = ReadNumber(Sec, TEXT("runSpeed"), Out.Movement.RunSpeed);
		Out.Movement.WalkMultiplier = ReadNumber(Sec, TEXT("walkMultiplier"), Out.Movement.WalkMultiplier);
		Out.Movement.CrouchMultiplier = ReadNumber(Sec, TEXT("crouchMultiplier"), Out.Movement.CrouchMultiplier);
		Out.Movement.JumpZVelocity = ReadNumber(Sec, TEXT("jumpZVelocity"), Out.Movement.JumpZVelocity);
		Out.Movement.bInstantStop = ReadBool(Sec, TEXT("instantStop"), Out.Movement.bInstantStop);
	});

	ReadSection(Root, TEXT("hit"), [&](const TSharedPtr<FJsonObject>& Sec)
	{
		Out.Hit.HeadRadiusCm = ReadNumber(Sec, TEXT("headRadiusCm"), Out.Hit.HeadRadiusCm);
		Out.Hit.StandEyeHeightCm = ReadNumber(Sec, TEXT("standEyeHeightCm"), Out.Hit.StandEyeHeightCm);
		Out.Hit.TorsoHeightCm = ReadNumber(Sec, TEXT("torsoHeightCm"), Out.Hit.TorsoHeightCm);
		Out.Hit.CapsuleHalfHeightCm = ReadNumber(Sec, TEXT("capsuleHalfHeightCm"), Out.Hit.CapsuleHalfHeightCm);
	});

	ReadSection(Root, TEXT("bot"), [&](const TSharedPtr<FJsonObject>& Sec)
	{
		Out.Bot.DelayMs = ReadNumber(Sec, TEXT("delayMs"), Out.Bot.DelayMs);
		Out.Bot.WindupMs = ReadNumber(Sec, TEXT("windupMs"), Out.Bot.WindupMs);
		Out.Bot.bRandomizeStance = ReadBool(Sec, TEXT("randomizeStance"), Out.Bot.bRandomizeStance);
	});

	ReadSection(Root, TEXT("duel"), [&](const TSharedPtr<FJsonObject>& Sec)
	{
		Out.Duel.TimeoutMs = ReadNumber(Sec, TEXT("timeoutMs"), Out.Duel.TimeoutMs);
		Out.Duel.ResetDelayMs = ReadNumber(Sec, TEXT("resetDelayMs"), Out.Duel.ResetDelayMs);
		Out.Duel.bAutoReset = ReadBool(Sec, TEXT("autoReset"), Out.Duel.bAutoReset);
	});

	ReadSection(Root, TEXT("map"), [&](const TSharedPtr<FJsonObject>& Sec)
	{
		Out.Map.MeshPath = ReadString(Sec, TEXT("meshPath"), Out.Map.MeshPath);
		Out.Map.MeshScale = ReadNumber(Sec, TEXT("meshScale"), Out.Map.MeshScale);
		Out.Map.GroundProbeHeightCm = ReadNumber(Sec, TEXT("groundProbeHeightCm"), Out.Map.GroundProbeHeightCm);
	});

	ReadSection(Root, TEXT("input"), [&](const TSharedPtr<FJsonObject>& Sec)
	{
		Out.Input.bDisableSmoothing = ReadBool(Sec, TEXT("disableSmoothing"), Out.Input.bDisableSmoothing);
		Out.Input.ValorantSensitivity = ReadNumber(Sec, TEXT("valorantSensitivity"), Out.Input.ValorantSensitivity);
		Out.Input.MouseDpi = ReadNumber(Sec, TEXT("mouseDpi"), Out.Input.MouseDpi);
	});

	Out.Validate();
}

FTrainerSettings UTrainingSettings::Load()
{
	FTrainerSettings Settings;
	const FString Path = GetSettingsFilePath();
	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *Path))
	{
		ApplyTo(Settings, JsonString);
		UE_LOG(LogTemp, Log, TEXT("TrainingSettings: loaded %s (botDelay=%.0fms headRadius=%.1fcm)"),
			*Path, Settings.Bot.DelayMs, Settings.Hit.HeadRadiusCm);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("TrainingSettings: %s not found, using defaults"), *Path);
		Settings.Validate();
	}
	return Settings;
}
