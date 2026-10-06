#if WITH_DEV_AUTOMATION_TESTS

#include "TrainingPreferences.h"
#include "TrainingSettings.h"
#include "Player/AscCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainerPreferencesRoundTripTest,
    "ascent1.Training.Preferences.RoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerPreferencesRoundTripTest::RunTest(const FString& Parameters)
{
    FTrainerPreferences Original;
    Original.Sensitivity = 0.63f;
    Original.Dpi = 1600;
    Original.CrosshairColor = FLinearColor(0.2f, 0.5f, 0.8f, 0.75f);
    Original.CrosshairLength = 17.0f;
    Original.CrosshairGap = 9.0f;
    Original.CrosshairThickness = 3.0f;
    Original.Volume = 0.25f;
    Original.QualityLevel = 3;
    Original.LastMode = 5;
    Original.LastRegion = TEXT("Mid");
    Original.LastDifficulty = 2;
    Original.LastPoint = 18;
    FTrainerPreferences Restored;
    FString Error;
    TestTrue(TEXT("Exported settings import successfully"), UTrainingPreferences::FromJson(UTrainingPreferences::ToJson(Original), Restored, Error));
    TestEqual(TEXT("All fields survive round trip"), UTrainingPreferences::ToJson(Restored), UTrainingPreferences::ToJson(Original));
    TestTrue(TEXT("Success clears stale error"), Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainerPreferencesAtomicParseTest,
    "ascent1.Training.Preferences.InvalidImportIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerPreferencesAtomicParseTest::RunTest(const FString& Parameters)
{
    FTrainerPreferences Current;
    Current.Sensitivity = 0.8f;
    Current.LastRegion = TEXT("B");
    const FString Before = UTrainingPreferences::ToJson(Current);
    FTrainerPreferences Candidate;
    Candidate.Sensitivity = 1.2f; // Would change current state if validation mutated it early.
    const FString Valid = UTrainingPreferences::ToJson(Candidate);
    TArray<FString> Invalid;
    Invalid.Add(TEXT("{broken json"));
    Invalid.Add(TEXT("{}"));
    Invalid.Add(TEXT("[]"));
    // Construct invalid payloads without depending on JSON writer whitespace/float formatting.
    const int32 ClosingBrace = Valid.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    const FString Prefix = Valid.Left(ClosingBrace);
    for (const FString& Field : TArray<FString>{
        TEXT("\"sensitivity\": -1"), TEXT("\"sensitivity\": \"fast\""), TEXT("\"sensitivity\": \"NaN\""),
        TEXT("\"dpi\": 800.5"), TEXT("\"volume\": 2"), TEXT("\"qualityLevel\": true"),
        TEXT("\"dpi\": 49"), TEXT("\"dpi\": 20001"), TEXT("\"qualityLevel\": 4"),
        TEXT("\"crosshairLength\": 0"), TEXT("\"crosshairGap\": 31"), TEXT("\"crosshairThickness\": 9"),
        TEXT("\"lastMode\": 6"), TEXT("\"lastRegion\": \"Outside\""),
        TEXT("\"lastDifficulty\": -1"), TEXT("\"lastPoint\": -1"),
        TEXT("\"sensitivity\": 20.00000001"), TEXT("\"volume\": 1.00000001"),
        TEXT("\"crosshairColor\": {\"r\": 2, \"g\": 1, \"b\": 1, \"a\": 1}")})
    {
        Invalid.Add(Prefix + TEXT(",") + Field + TEXT("}"));
    }
    // JSON's numeric overflow must not become non-finite preferences.
    Invalid.Add(Prefix + TEXT(",\"sensitivity\":1e400}"));
    // UE's reader tolerates these malformed unknown fields unless we guard the original text.
    Invalid.Add(Prefix + TEXT(",\"ignored\":[1,]}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":[1, \r\n\t]}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":{\"value\":1,}}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":\"raw\nnewline\"}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":\"raw\ttab\"}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":\"backslash before raw\\\nnewline\"}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":TRUE}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":FALSE}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":NULL}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":[1}}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":{\"value\":1]}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":01}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":1e}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":1.}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":+1}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":\"\\u12G4\"}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":\"\\u123\"}"));
    Invalid.Add(Prefix + TEXT(",\"ignored\":\"\\q\"}"));
    Invalid.Add(Prefix + TEXT(",\"lastRegion\":\"a\"}"));
    Invalid.Add(Prefix + TEXT(",\"lastRegion\":\"MID\"}"));
    FString TooDeep = Prefix + TEXT(",\"ignored\":");
    for (int32 Depth = 0; Depth < 64; ++Depth) TooDeep += TEXT("[");
    TooDeep += TEXT("0");
    for (int32 Depth = 0; Depth < 64; ++Depth) TooDeep += TEXT("]");
    Invalid.Add(TooDeep + TEXT("}")); // Root object plus 64 arrays exceeds the 64-container limit.
    for (const FString& Json : Invalid)
    {
        FString Error;
        TestFalse(TEXT("Invalid import is rejected"), UTrainingPreferences::FromJson(Json, Current, Error));
        TestFalse(TEXT("Rejected import explains error"), Error.IsEmpty());
        TestEqual(TEXT("Rejected import preserves every current field"), UTrainingPreferences::ToJson(Current), Before);
    }
    FString Error;
    const FString Escaped = Prefix + TEXT(",\"ignored\":\"quoted \\\" and slash \\\\ and comma ,] and newline \\n\"}");
    TestTrue(TEXT("Escaped quotes, backslashes, newline escapes and comma-brackets inside strings remain valid"),
        UTrainingPreferences::FromJson(Escaped, Current, Error));
    TestEqual(TEXT("Valid escaped extension imports the candidate preferences"), UTrainingPreferences::ToJson(Current), Valid);
    const FString ValidSyntax = Prefix + TEXT(",\"ignored\":[true,false,null,-0.5e+2,0,\"\\u0041\",{}]}");
    TestTrue(TEXT("Lowercase literals, valid number forms, unicode escapes and matching nested containers are accepted"),
        UTrainingPreferences::FromJson(ValidSyntax, Current, Error));
    FString MaximumDepth = Prefix + TEXT(",\"ignored\":");
    for (int32 Depth = 0; Depth < 63; ++Depth) MaximumDepth += TEXT("[");
    MaximumDepth += TEXT("0");
    for (int32 Depth = 0; Depth < 63; ++Depth) MaximumDepth += TEXT("]");
    TestTrue(TEXT("Exactly 64 nested containers are accepted"), UTrainingPreferences::FromJson(MaximumDepth + TEXT("}"), Current, Error));
    TestFalse(TEXT("A second JSON document after the preferences is rejected"), UTrainingPreferences::FromJson(Valid + TEXT(" {}"), Current, Error));
    TestEqual(TEXT("Extra document rejection preserves the imported preferences"), UTrainingPreferences::ToJson(Current), Valid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainerPreferencesSensitivityTest,
    "ascent1.Training.Preferences.ValorantSensitivity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerPreferencesSensitivityTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("0.4 sensitivity / 800 DPI gives 40.8214 cm/360"),
        FMath::IsNearlyEqual(UTrainingPreferences::CentimetersPer360(0.4f, 800), 40.82143f, 0.0001f));
    TestTrue(TEXT("Doubling DPI halves physical turn distance"),
        FMath::IsNearlyEqual(UTrainingPreferences::CentimetersPer360(0.4f, 1600), 20.410715f, 0.0001f));
    TestEqual(TEXT("Invalid hardware values have no physical turn distance"), UTrainingPreferences::CentimetersPer360(0.0f, 800), 0.0f);
    TestEqual(TEXT("Zero DPI cannot divide by zero"), UTrainingPreferences::CentimetersPer360(0.4f, 0), 0.0f);
    FTrainerPreferences Invalid;
    FString Error;
    Invalid.Sensitivity = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite runtime settings cannot be saved"), UTrainingPreferences::Validate(Invalid, Error));
    Invalid = FTrainerPreferences();
    Invalid.CrosshairColor.R = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN color components cannot be saved"), UTrainingPreferences::Validate(Invalid, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainerCharacterPreferencesTest,
    "ascent1.Training.Preferences.CharacterAppliesInputAndMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerCharacterPreferencesTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world created"), World)) return false;
    AAscCharacter* Character = World->SpawnActor<AAscCharacter>();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Character spawned"), Character) || !TestNotNull(TEXT("Controller spawned"), Controller))
    {
        World->DestroyWorld(false);
        return false;
    }
    Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
    Controller->Possess(Character);
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Controller->SetDeprecatedInputYawScale(2.5f);
    Controller->SetDeprecatedInputPitchScale(-2.5f);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();
    const bool bPreviousSmoothing = InputSettings->bEnableMouseSmoothing;
    const bool bPreviousFovScaling = InputSettings->bEnableFOVScaling;
    FTrainerPreferences Preferences;
    Character->ApplyPreferences(Preferences);
    FInputAxisProperties Axis;
    TestTrue(TEXT("Mouse X axis is configured"), Controller->PlayerInput->GetAxisProperties(EKeys::MouseX, Axis));
    TestEqual(TEXT("Each legacy mouse count receives 0.07 axis scale"), Axis.Sensitivity, 0.07f);
    Controller->RotationInput = FRotator::ZeroRotator;
    Character->Turn(Axis.Sensitivity);
    Character->LookUp(-Axis.Sensitivity);
    TestTrue(TEXT("One count produces 0.028 degrees of yaw at 0.4 sensitivity"), FMath::IsNearlyEqual(Controller->RotationInput.Yaw, 0.028, 0.000001));
    TestTrue(TEXT("One inverted Y count produces -0.028 degrees of pitch"), FMath::IsNearlyEqual(Controller->RotationInput.Pitch, -0.028, 0.000001));
    TestFalse(TEXT("Mouse smoothing is disabled"), InputSettings->bEnableMouseSmoothing);
    TestFalse(TEXT("FOV cannot silently multiply mouse sensitivity"), InputSettings->bEnableFOVScaling);
    FTrainerSettings Gameplay;
    Gameplay.Movement.RunSpeed = 600.0f;
    Gameplay.Movement.WalkMultiplier = 0.5f;
    Gameplay.Movement.CrouchMultiplier = 0.4f;
    Character->ApplyTrainerSettings(Gameplay);
    Character->WalkPressed();
    Character->Tick(0.0f);
    TestEqual(TEXT("Walking uses configured multiplier"), Character->GetMoveSpeed(), 300.0f);
    Character->WalkReleased();
    Character->bIsCrouched = true;
    Character->Tick(0.0f);
    TestEqual(TEXT("Crouching uses configured multiplier"), Character->GetMoveSpeed(), 240.0f);
    TestEqual(TEXT("Crouching speed reaches CharacterMovement"), Character->GetCharacterMovement()->MaxWalkSpeedCrouched, 240.0f);
    InputSettings->bEnableMouseSmoothing = bPreviousSmoothing;
    InputSettings->bEnableFOVScaling = bPreviousFovScaling;
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainerMouseDirectionMappingTest,
    "ascent1.Training.Preferences.MouseDirectionMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainerMouseDirectionMappingTest::RunTest(const FString& Parameters)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("Mouse mapping test world"),World)) return false;
    auto Character=World->SpawnActor<AAscCharacter>();
    auto Controller=World->SpawnActor<APlayerController>();
    if(!Character || !Controller){World->DestroyWorld(false);return false;}
    Controller->PlayerInput=NewObject<UPlayerInput>(Controller);
    Controller->Possess(Character);
    auto Settings=GetMutableDefault<UInputSettings>();
    const bool Smoothing=Settings->bEnableMouseSmoothing,FovScaling=Settings->bEnableFOVScaling;
    Character->ApplyPreferences(FTrainerPreferences());
    float YawScale=0,PitchScale=0;int32 YawMappings=0,PitchMappings=0;
    for(const auto& Mapping:Settings->GetAxisMappings())
    {
        if(Mapping.AxisName==TEXT("Turn") && Mapping.Key==EKeys::MouseX){YawScale=Mapping.Scale;++YawMappings;}
        if(Mapping.AxisName==TEXT("LookUp") && Mapping.Key==EKeys::MouseY){PitchScale=Mapping.Scale;++PitchMappings;}
    }
    TestEqual(TEXT("Exactly one horizontal mouse mapping"),YawMappings,1);
    TestEqual(TEXT("Exactly one vertical mouse mapping"),PitchMappings,1);
    FInputAxisProperties X,Y;
    Controller->PlayerInput->GetAxisProperties(EKeys::MouseX,X);
    Controller->PlayerInput->GetAxisProperties(EKeys::MouseY,Y);
    // SceneViewport converts screen deltas to MouseX=dx and MouseY=-dy.
    // Moving the mouse right/up by 100 counts must turn right/look up by 2.8 degrees.
    Controller->RotationInput=FRotator::ZeroRotator;
    Character->Turn(100*X.Sensitivity*YawScale);
    Character->LookUp(100*Y.Sensitivity*PitchScale);
    TestTrue(TEXT("Mouse right turns right with unchanged sensitivity"),FMath::IsNearlyEqual(Controller->RotationInput.Yaw,2.8,.00001));
    TestTrue(TEXT("Mouse up looks up with unchanged sensitivity"),FMath::IsNearlyEqual(Controller->RotationInput.Pitch,2.8,.00001));
    Controller->RotationInput=FRotator::ZeroRotator;
    Character->Turn(-100*X.Sensitivity*YawScale);
    Character->LookUp(-100*Y.Sensitivity*PitchScale);
    TestTrue(TEXT("Mouse left turns left"),FMath::IsNearlyEqual(Controller->RotationInput.Yaw,-2.8,.00001));
    TestTrue(TEXT("Mouse down looks down"),FMath::IsNearlyEqual(Controller->RotationInput.Pitch,-2.8,.00001));
    Settings->bEnableMouseSmoothing=Smoothing;Settings->bEnableFOVScaling=FovScaling;
    World->DestroyWorld(false);
    return true;
}

#endif
