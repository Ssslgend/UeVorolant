#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TrainingPreferences.generated.h"

class APlayerController;

/** Native user preferences; hardware DPI only informs the cm/360 display. */
USTRUCT(BlueprintType)
struct ASCENT1_API FTrainerPreferences
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    float Sensitivity = 0.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    int32 Dpi = 800;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    FLinearColor CrosshairColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    float CrosshairLength = 8.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    float CrosshairGap = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    float CrosshairThickness = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    float Volume = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    int32 QualityLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    int32 LastMode = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    FString LastRegion = TEXT("All");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    int32 LastDifficulty = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trainer")
    int32 LastPoint = 0;
};

UCLASS()
class ASCENT1_API UTrainingPreferences : public UObject
{
    GENERATED_BODY()

public:
    static FTrainerPreferences Load();
    static bool Save(const FTrainerPreferences& Preferences, FString& Error);
    static bool Export(const FTrainerPreferences& Preferences, FString& Error);
    /** Validates and saves before replacing InOut. Any failure preserves InOut. */
    static bool Import(FTrainerPreferences& InOut, FString& Error);
    static void Apply(APlayerController* Controller, const FTrainerPreferences& Preferences);
    static bool Validate(const FTrainerPreferences& Preferences, FString& Error);
    /** Requires every field; wrong types, missing fields and out-of-range values fail atomically. */
    static bool FromJson(const FString& Json, FTrainerPreferences& InOut, FString& Error);
    static FString ToJson(const FTrainerPreferences& Preferences);
    static float CentimetersPer360(float Sensitivity, int32 Dpi);
    static FString GetPreferencesFilePath();
    static FString GetExportFilePath();
    static FString GetImportFilePath();
};
