#include "TrainingPreferences.h"
#include "Player/AscCharacter.h"
#include "AudioDevice.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
    /** Validate the complete grammar before UE's permissive reader constructs the object. */
    class FStrictJsonSyntax
    {
    public:
        FStrictJsonSyntax(const FString& InJson, FString& InError) : Json(InJson), Error(InError) {}

        bool Check()
        {
            if (!ReadValue(0)) return false;
            SkipWhitespace();
            return Position == Json.Len() || Fail(TEXT("Unexpected data after the JSON document"));
        }

    private:
        const FString& Json;
        FString& Error;
        int32 Position = 0;

        bool Fail(const TCHAR* Reason)
        {
            Error = FString::Printf(TEXT("%s at character %d."), Reason, Position + 1);
            return false;
        }

        static bool IsDigit(TCHAR Character) { return Character >= TEXT('0') && Character <= TEXT('9'); }
        static bool IsHex(TCHAR Character)
        {
            return IsDigit(Character) || (Character >= TEXT('a') && Character <= TEXT('f'))
                || (Character >= TEXT('A') && Character <= TEXT('F'));
        }

        void SkipWhitespace()
        {
            while (Position < Json.Len())
            {
                const TCHAR Character = Json[Position];
                if (Character != TEXT(' ') && Character != TEXT('\t') && Character != TEXT('\r') && Character != TEXT('\n')) break;
                ++Position;
            }
        }

        bool Consume(TCHAR Character)
        {
            if (Position >= Json.Len() || Json[Position] != Character) return false;
            ++Position;
            return true;
        }

        bool ReadValue(int32 Depth)
        {
            SkipWhitespace();
            if (Position >= Json.Len()) return Fail(TEXT("Expected a JSON value"));
            const TCHAR Character = Json[Position];
            if (Character == TEXT('{') || Character == TEXT('['))
            {
                if (Depth >= 64) return Fail(TEXT("JSON nesting exceeds 64 levels"));
                return Character == TEXT('{') ? ReadObject(Depth + 1) : ReadArray(Depth + 1);
            }
            if (Character == TEXT('"')) return ReadString();
            if (Character == TEXT('-') || IsDigit(Character)) return ReadNumber();
            for (const TCHAR* Literal : {TEXT("true"), TEXT("false"), TEXT("null")})
            {
                const int32 Length = FCString::Strlen(Literal);
                if (Json.Mid(Position, Length).Equals(Literal, ESearchCase::CaseSensitive))
                {
                    Position += Length;
                    return true;
                }
            }
            return Fail(TEXT("Invalid JSON value"));
        }

        bool ReadObject(int32 Depth)
        {
            ++Position; // Opening brace was checked by ReadValue.
            SkipWhitespace();
            if (Consume(TEXT('}'))) return true;
            while (true)
            {
                if (!ReadString()) return false;
                SkipWhitespace();
                if (!Consume(TEXT(':'))) return Fail(TEXT("Expected ':' after an object key"));
                if (!ReadValue(Depth)) return false;
                SkipWhitespace();
                if (Consume(TEXT('}'))) return true;
                if (!Consume(TEXT(','))) return Fail(TEXT("Expected ',' or '}' in an object"));
                SkipWhitespace();
                // ReadString requires another key; a closing brace here is a trailing comma.
            }
        }

        bool ReadArray(int32 Depth)
        {
            ++Position; // Opening bracket was checked by ReadValue.
            SkipWhitespace();
            if (Consume(TEXT(']'))) return true;
            while (true)
            {
                if (!ReadValue(Depth)) return false;
                SkipWhitespace();
                if (Consume(TEXT(']'))) return true;
                if (!Consume(TEXT(','))) return Fail(TEXT("Expected ',' or ']' in an array"));
                // ReadValue requires another element; a closing bracket here is a trailing comma.
            }
        }

        bool ReadString()
        {
            if (!Consume(TEXT('"'))) return Fail(TEXT("Expected a quoted JSON string"));
            while (Position < Json.Len())
            {
                const TCHAR Character = Json[Position++];
                if (Character == TEXT('"')) return true;
                if (Character < 0x20) return Fail(TEXT("JSON strings must escape control characters"));
                if (Character != TEXT('\\')) continue;
                if (Position >= Json.Len()) return Fail(TEXT("Unterminated JSON escape"));
                const TCHAR Escape = Json[Position++];
                if (Escape == TEXT('u'))
                {
                    for (int32 Digit = 0; Digit < 4; ++Digit)
                    {
                        if (Position >= Json.Len() || !IsHex(Json[Position])) return Fail(TEXT("Unicode escapes require four hexadecimal digits"));
                        ++Position;
                    }
                }
                else if (Escape != TEXT('"') && Escape != TEXT('\\') && Escape != TEXT('/') && Escape != TEXT('b')
                    && Escape != TEXT('f') && Escape != TEXT('n') && Escape != TEXT('r') && Escape != TEXT('t'))
                {
                    return Fail(TEXT("Invalid JSON escape"));
                }
            }
            return Fail(TEXT("Unterminated JSON string"));
        }

        bool ReadNumber()
        {
            Consume(TEXT('-'));
            if (!Consume(TEXT('0')))
            {
                if (Position >= Json.Len() || Json[Position] < TEXT('1') || Json[Position] > TEXT('9')) return Fail(TEXT("Invalid JSON integer"));
                while (Position < Json.Len() && IsDigit(Json[Position])) ++Position;
            }
            if (Consume(TEXT('.')))
            {
                if (Position >= Json.Len() || !IsDigit(Json[Position])) return Fail(TEXT("A JSON fraction requires digits"));
                while (Position < Json.Len() && IsDigit(Json[Position])) ++Position;
            }
            if (Consume(TEXT('e')) || Consume(TEXT('E')))
            {
                if (!Consume(TEXT('+'))) Consume(TEXT('-'));
                if (Position >= Json.Len() || !IsDigit(Json[Position])) return Fail(TEXT("A JSON exponent requires digits"));
                while (Position < Json.Len() && IsDigit(Json[Position])) ++Position;
            }
            return true;
        }
    };

    bool WritePreferences(const FString& Path, const FTrainerPreferences& Preferences, FString& Error)
    {
        if (!UTrainingPreferences::Validate(Preferences, Error)) return false;
        IFileManager& Files = IFileManager::Get();
        if (!Files.MakeDirectory(*FPaths::GetPath(Path), true))
        {
            Error = TEXT("Could not create Saved/Trainer.");
            return false;
        }
        const FString Temporary = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(UTrainingPreferences::ToJson(Preferences), *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            Files.Delete(*Temporary);
            Error = TEXT("Could not write the temporary settings file.");
            return false;
        }
        // Same-directory replacement on Windows is atomic, including existing preferences.
#if PLATFORM_WINDOWS
        const bool bMoved = !!::MoveFileExW(*Temporary, *Path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
        const bool bMoved = Files.Move(*Path, *Temporary, true, false, false, true);
#endif
        if (!bMoved)
        {
            Files.Delete(*Temporary);
            Error = TEXT("Could not replace the settings file. Current settings were preserved.");
            return false;
        }
        Error.Empty();
        return true;
    }

    bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, double& Out, FString& Error)
    {
        const TSharedPtr<FJsonValue>* Field = Object->Values.Find(Name);
        if (!Field || !Field->IsValid() || (*Field)->Type != EJson::Number || !(*Field)->TryGetNumber(Out) || !FMath::IsFinite(Out))
        {
            Error = FString::Printf(TEXT("%s must be a finite number."), Name);
            return false;
        }
        return true;
    }

    bool ReadFloat(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, float& Out, FString& Error, double Min, double Max)
    {
        double Number;
        if (!ReadNumber(Object, Name, Number, Error)) return false;
        if (Number < Min || Number > Max)
        {
            Error = FString::Printf(TEXT("%s must be between %g and %g."), Name, Min, Max);
            return false;
        }
        Out = static_cast<float>(Number);
        return true;
    }

    bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, int32& Out, FString& Error)
    {
        double Number;
        if (!ReadNumber(Object, Name, Number, Error)) return false;
        if (Number < MIN_int32 || Number > MAX_int32 || Number != FMath::FloorToDouble(Number))
        {
            Error = FString::Printf(TEXT("%s must be an integer."), Name);
            return false;
        }
        Out = static_cast<int32>(Number);
        return true;
    }
}

FString UTrainingPreferences::GetPreferencesFilePath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Trainer/preferences.json")));
}

FString UTrainingPreferences::GetExportFilePath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Trainer/settings_export.json")));
}

FString UTrainingPreferences::GetImportFilePath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Trainer/settings_import.json")));
}

FTrainerPreferences UTrainingPreferences::Load()
{
    FTrainerPreferences Preferences;
    FString Json, Error;
    if (FFileHelper::LoadFileToString(Json, *GetPreferencesFilePath()) && !FromJson(Json, Preferences, Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("Trainer preferences rejected: %s"), *Error);
    }
    return Preferences;
}

bool UTrainingPreferences::Save(const FTrainerPreferences& Preferences, FString& Error)
{
    return WritePreferences(GetPreferencesFilePath(), Preferences, Error);
}

bool UTrainingPreferences::Export(const FTrainerPreferences& Preferences, FString& Error)
{
    return WritePreferences(GetExportFilePath(), Preferences, Error);
}

bool UTrainingPreferences::Import(FTrainerPreferences& InOut, FString& Error)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *GetImportFilePath()))
    {
        Error = TEXT("Place a settings JSON file at Saved/Trainer/settings_import.json first.");
        return false;
    }
    FTrainerPreferences Candidate;
    if (!FromJson(Json, Candidate, Error) || !Save(Candidate, Error)) return false;
    InOut = Candidate;
    return true;
}

bool UTrainingPreferences::Validate(const FTrainerPreferences& P, FString& Error)
{
    const auto Range = [&Error](const TCHAR* Name, double Value, double Min, double Max)
    {
        if (!FMath::IsFinite(Value) || Value < Min || Value > Max)
        {
            Error = FString::Printf(TEXT("%s must be between %g and %g."), Name, Min, Max);
            return false;
        }
        return true;
    };
    if (!Range(TEXT("sensitivity"), P.Sensitivity, 0.001, 20.0) || !Range(TEXT("dpi"), P.Dpi, 50, 20000)
        || !Range(TEXT("crosshairLength"), P.CrosshairLength, 1, 40) || !Range(TEXT("crosshairGap"), P.CrosshairGap, 0, 30)
        || !Range(TEXT("crosshairThickness"), P.CrosshairThickness, 1, 8) || !Range(TEXT("volume"), P.Volume, 0, 1)
        || !Range(TEXT("qualityLevel"), P.QualityLevel, 0, 3) || !Range(TEXT("lastMode"), P.LastMode, 0, 5)
        || !Range(TEXT("lastDifficulty"), P.LastDifficulty, 0, 2) || !Range(TEXT("lastPoint"), P.LastPoint, 0, 10000)
        || !Range(TEXT("crosshairColor.r"), P.CrosshairColor.R, 0, 1) || !Range(TEXT("crosshairColor.g"), P.CrosshairColor.G, 0, 1)
        || !Range(TEXT("crosshairColor.b"), P.CrosshairColor.B, 0, 1) || !Range(TEXT("crosshairColor.a"), P.CrosshairColor.A, 0, 1)) return false;
    if (!P.LastRegion.Equals(TEXT("A"), ESearchCase::CaseSensitive) && !P.LastRegion.Equals(TEXT("B"), ESearchCase::CaseSensitive)
        && !P.LastRegion.Equals(TEXT("Mid"), ESearchCase::CaseSensitive) && !P.LastRegion.Equals(TEXT("All"), ESearchCase::CaseSensitive))
    {
        Error = TEXT("lastRegion must be A, B, Mid or All.");
        return false;
    }
    Error.Empty();
    return true;
}

FString UTrainingPreferences::ToJson(const FTrainerPreferences& P)
{
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("sensitivity"), P.Sensitivity);
    Object->SetNumberField(TEXT("dpi"), P.Dpi);
    TSharedRef<FJsonObject> Color = MakeShared<FJsonObject>();
    Color->SetNumberField(TEXT("r"), P.CrosshairColor.R);
    Color->SetNumberField(TEXT("g"), P.CrosshairColor.G);
    Color->SetNumberField(TEXT("b"), P.CrosshairColor.B);
    Color->SetNumberField(TEXT("a"), P.CrosshairColor.A);
    Object->SetObjectField(TEXT("crosshairColor"), Color);
    Object->SetNumberField(TEXT("crosshairLength"), P.CrosshairLength);
    Object->SetNumberField(TEXT("crosshairGap"), P.CrosshairGap);
    Object->SetNumberField(TEXT("crosshairThickness"), P.CrosshairThickness);
    Object->SetNumberField(TEXT("volume"), P.Volume);
    Object->SetNumberField(TEXT("qualityLevel"), P.QualityLevel);
    Object->SetNumberField(TEXT("lastMode"), P.LastMode);
    Object->SetStringField(TEXT("lastRegion"), P.LastRegion);
    Object->SetNumberField(TEXT("lastDifficulty"), P.LastDifficulty);
    Object->SetNumberField(TEXT("lastPoint"), P.LastPoint);
    FString Json;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json));
    return Json;
}

bool UTrainingPreferences::FromJson(const FString& Json, FTrainerPreferences& InOut, FString& Error)
{
    if (!FStrictJsonSyntax(Json, Error).Check()) return false;
    TSharedPtr<FJsonObject> Object;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
    {
        Error = TEXT("Settings file must contain a valid JSON object.");
        return false;
    }
    FTrainerPreferences Candidate;
    if (!ReadFloat(Object, TEXT("sensitivity"), Candidate.Sensitivity, Error, 0.001, 20.0) || !ReadInt(Object, TEXT("dpi"), Candidate.Dpi, Error)
        || !ReadFloat(Object, TEXT("crosshairLength"), Candidate.CrosshairLength, Error, 1, 40) || !ReadFloat(Object, TEXT("crosshairGap"), Candidate.CrosshairGap, Error, 0, 30)
        || !ReadFloat(Object, TEXT("crosshairThickness"), Candidate.CrosshairThickness, Error, 1, 8) || !ReadFloat(Object, TEXT("volume"), Candidate.Volume, Error, 0, 1)
        || !ReadInt(Object, TEXT("qualityLevel"), Candidate.QualityLevel, Error) || !ReadInt(Object, TEXT("lastMode"), Candidate.LastMode, Error)
        || !ReadInt(Object, TEXT("lastDifficulty"), Candidate.LastDifficulty, Error) || !ReadInt(Object, TEXT("lastPoint"), Candidate.LastPoint, Error)) return false;
    const TSharedPtr<FJsonValue>* Region = Object->Values.Find(TEXT("lastRegion"));
    if (!Region || !Region->IsValid() || (*Region)->Type != EJson::String || !(*Region)->TryGetString(Candidate.LastRegion))
    {
        Error = TEXT("lastRegion must be a string.");
        return false;
    }
    const TSharedPtr<FJsonObject>* Color = nullptr;
    if (!Object->TryGetObjectField(TEXT("crosshairColor"), Color) || !Color || !Color->IsValid())
    {
        Error = TEXT("crosshairColor must be an object with r, g, b and a components.");
        return false;
    }
    if (!ReadFloat(*Color, TEXT("r"), Candidate.CrosshairColor.R, Error, 0, 1) || !ReadFloat(*Color, TEXT("g"), Candidate.CrosshairColor.G, Error, 0, 1)
        || !ReadFloat(*Color, TEXT("b"), Candidate.CrosshairColor.B, Error, 0, 1) || !ReadFloat(*Color, TEXT("a"), Candidate.CrosshairColor.A, Error, 0, 1)
        || !Validate(Candidate, Error)) return false;
    InOut = Candidate;
    return true;
}

float UTrainingPreferences::CentimetersPer360(float Sensitivity, int32 Dpi)
{
    if (!FMath::IsFinite(Sensitivity) || Sensitivity <= 0.0f || Dpi <= 0) return 0.0f;
    return static_cast<float>(360.0 * 2.54 / (0.07 * static_cast<double>(Sensitivity) * Dpi));
}

void UTrainingPreferences::Apply(APlayerController* Controller, const FTrainerPreferences& Preferences)
{
    FString Error;
    if (!Controller || !Validate(Preferences, Error)) return;
    if (AAscCharacter* Character = Cast<AAscCharacter>(Controller->GetPawn())) Character->ApplyPreferences(Preferences);
    if (UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings())
    {
        Settings->SetOverallScalabilityLevel(Preferences.QualityLevel);
        Settings->ApplyNonResolutionSettings();
    }
    if (UWorld* World = Controller->GetWorld())
    {
        if (FAudioDevice* Audio = World->GetAudioDeviceRaw()) Audio->SetTransientPrimaryVolume(Preferences.Volume);
    }
}
