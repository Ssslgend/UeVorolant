#include "AscPlayerController.h"
#include "Training/TrainerSession.h"
#include "Training/TrainingSaveGame.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

namespace
{
    TSharedRef<STextBlock> Label(const FString& Text,int Size=16,FLinearColor Color=FLinearColor(.87,.91,.94))
    { return SNew(STextBlock).Text(FText::FromString(Text)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color).AutoWrapText(true); }
    TSharedRef<SButton> Button(const FString& Text,TFunction<FReply()> Action,bool Selected=false)
    { return SNew(SButton).ContentPadding(FMargin(15,10)).ButtonColorAndOpacity(Selected?FLinearColor(.06,.38,.40):FLinearColor(.055,.085,.12)).OnClicked_Lambda([Action](){return Action();})[SNew(STextBlock).Text(FText::FromString(Text)).Font(FCoreStyle::GetDefaultFontStyle("Regular",16)).ColorAndOpacity(FLinearColor(.87,.91,.94))]; }
}
ATrainerSession* AAscPlayerController::GetSession() const { return Cast<ATrainerSession>(UGameplayStatics::GetActorOfClass(this,ATrainerSession::StaticClass())); }
void AAscPlayerController::EndPlay(const EEndPlayReason::Type R)
{
    if(MenuRoot && GetWorld() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
    MenuRoot.Reset(); Super::EndPlay(R);
}
void AAscPlayerController::OpenMenu()
{
    bMenuOpen=true; if(auto S=GetSession()) S->Pause(true);
    if(auto C=Cast<ACharacter>(GetPawn())) C->GetCharacterMovement()->StopMovementImmediately();
    SetPause(true); ResetIgnoreMoveInput();SetIgnoreMoveInput(true);ResetIgnoreLookInput();SetIgnoreLookInput(true);
    bShowMouseCursor=true; BuildMenu(); ApplyPreferences();
}
void AAscPlayerController::CloseMenu()
{
    if(MenuRoot && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
    MenuRoot.Reset();bMenuOpen=false;SetPause(false);ResetIgnoreMoveInput();ResetIgnoreLookInput();
    if(auto S=GetSession()) S->Pause(false);bShowMouseCursor=false;SetInputMode(FInputModeGameOnly());
}
void AAscPlayerController::ToggleMenu(){if(bMenuOpen && GetSession() && GetSession()->IsRunning()) CloseMenu();else OpenMenu();}
void AAscPlayerController::ApplyPreferences(){FString Error;if(!UTrainingPreferences::Save(Preferences,Error)) MenuMessage=Error; UTrainingPreferences::Apply(this,Preferences);}
void AAscPlayerController::StartPractice(int32 M,FString R,int32 D,int32 P)
{
    SetPause(false);
    if(auto S=GetSession()) {if(S->Start(M,R,D,P)){Preferences=UTrainingPreferences::Load();CloseMenu();}else{MenuMessage=S->GetStatus();OpenMenu();}}
}
void AAscPlayerController::RestartPractice(){if(!bMenuOpen)if(auto S=GetSession())S->Restart();}
void AAscPlayerController::NextPractice(){if(!bMenuOpen)if(auto S=GetSession())S->Next();}
void AAscPlayerController::CapturePlayerPoint(){if(!bMenuOpen)if(auto S=GetSession())S->CapturePlayer();}
void AAscPlayerController::CaptureEnemyPoint(){if(!bMenuOpen)if(auto S=GetSession())S->CaptureEnemy();}
void AAscPlayerController::SavePersonalPoint(){if(!bMenuOpen)if(auto S=GetSession())S->SavePersonalPoint();}

void AAscPlayerController::BuildMenu()
{
    auto Viewport=GetWorld()->GetGameViewport();if(!Viewport)return;
    if(MenuRoot) Viewport->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
    auto Body=SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(0,0,0,4)[Label(TEXT("ASCENT  /  亚海悬城"),28,FLinearColor(.15,.91,.84))];
    Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Label(TEXT("对枪训练 · 原地图战斗区域 · 原墙面与箱体高度参照"),13,FLinearColor(.52,.65,.72))];
    auto Nav=SNew(SHorizontalBox);
    const TCHAR* Tabs[]={TEXT("训练"),TEXT("设置"),TEXT("训练记录")};
    for(int32 I=0;I<3;++I)Nav->AddSlot().AutoWidth().Padding(0,0,8,0)[Button(Tabs[I],[this,I](){MenuPage=I;MenuMessage.Empty();BuildMenu();return FReply::Handled();},MenuPage==I)];
    Nav->AddSlot().FillWidth(1);
    if(auto S=GetSession(); S && S->IsRunning()) Nav->AddSlot().AutoWidth()[Button(TEXT("返回训练"),[this](){CloseMenu();return FReply::Handled();})];
    Nav->AddSlot().AutoWidth().Padding(8,0,0,0)[Button(TEXT("退出"),[this](){UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);return FReply::Handled();})];
    Body->AddSlot().AutoHeight().Padding(0,0,0,18)[Nav];
    auto Content=SNew(SVerticalBox);
    if(MenuPage==0)
    {
        auto Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(0,0,8,8));
        for(int32 I=0;I<6;++I)Grid->AddSlot(I%3,I/3)[Button(ATrainerSession::ModeName(I),[this,I](){SelectedMode=I;BuildMenu();return FReply::Handled();},I==SelectedMode)];
        Content->AddSlot().AutoHeight()[Grid];
        const TCHAR* Descriptions[]={TEXT("按真实路线自由走图，熟悉拐角和高低差。F5 记录玩家位置，F6 记录敌人位置，F7 保存个人点位。"),TEXT("从掩体后探身，利用原地图爆头高度参照预瞄敌人。暴露后敌人会按难度开枪。"),TEXT("你架住拐角，敌人随机延时探身。训练准星位置、反应和急停射击。"),TEXT("探身触发闪光提示；转身避闪，再回头击杀敌人。遮挡、距离和朝向决定致盲强度。"),TEXT("守住位置应对 5 波敌人；随机延时和可见的不同进攻角度。"),TEXT("连续击杀 3 个敌人。敌人可蹲姿探身，有 100 生命值；移动射击降低精准度。")};
        Content->AddSlot().AutoHeight().Padding(0,8,0,18)[Label(Descriptions[FMath::Clamp(SelectedMode,0,5)],14)];
        auto Regions=SNew(SHorizontalBox);const TCHAR* Codes[]={TEXT("All"),TEXT("A"),TEXT("Mid"),TEXT("B")};const TCHAR* Names[]={TEXT("全部区域"),TEXT("A 区"),TEXT("中路"),TEXT("B 区")};
        for(int I=0;I<4;++I){const FString Code=Codes[I];Regions->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(Names[I],[this,Code](){SelectedRegion=Code;SelectedPoint=0;BuildMenu();return FReply::Handled();},SelectedRegion==Code)];}
        Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Regions];
        TArray<FCombatTrainingPoint> Filtered;
        if(auto S=GetSession()) for(auto P:S->GetPoints())if(SelectedRegion==TEXT("All")||P.Region==SelectedRegion)Filtered.Add(P);
        SelectedPoint=Filtered.IsEmpty()?0:FMath::Clamp(SelectedPoint,0,Filtered.Num()-1);
        const FString PointName=Filtered.IsEmpty()?TEXT("缺少点位数据"):Filtered[SelectedPoint].Name;
        Content->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SComboButton).ContentPadding(FMargin(12,10)).ButtonContent()[Label(TEXT("点位：")+PointName)]
            .OnGetMenuContent_Lambda([this,Filtered](){auto List=SNew(SVerticalBox);for(int I=0;I<Filtered.Num();++I)List->AddSlot().AutoHeight()[Button(Filtered[I].Name,[this,I](){SelectedPoint=I;BuildMenu();return FReply::Handled();})];return List;})];
        auto DifficultyRow=SNew(SHorizontalBox);const TCHAR* Levels[]={TEXT("简单 · 650ms"),TEXT("普通 · 400ms"),TEXT("困难 · 250ms")};
        for(int I=0;I<3;++I)DifficultyRow->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(Levels[I],[this,I](){SelectedDifficulty=I;BuildMenu();return FReply::Handled();},SelectedDifficulty==I)];
        Content->AddSlot().AutoHeight().Padding(0,0,0,18)[DifficultyRow];
        auto Actions=SNew(SHorizontalBox);
        Actions->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(TEXT("开始训练"),[this](){StartPractice(SelectedMode,SelectedRegion,SelectedDifficulty,SelectedPoint);return FReply::Handled();},true)];
        Actions->AddSlot().FillWidth(1)[Button(TEXT("继续上次训练"),[this](){auto P=UTrainingPreferences::Load();StartPractice(P.LastMode,P.LastRegion,P.LastDifficulty,P.LastPoint);return FReply::Handled();})];
        Content->AddSlot().AutoHeight()[Actions];
        Content->AddSlot().AutoHeight().Padding(0,16,0,0)[Label(TEXT("WASD 移动 · Shift 静步 · Ctrl 蹲姿 · 空格跳跃 · 左键射击\nR 重练 · N 下一点 · Esc 暂停/返回 · F5/F6/F7 记录个人点位"),13,FLinearColor(.52,.65,.72))];
    }
    else if(MenuPage==1)
    {
        auto FloatRow=[this,Content](const FString& Name,float FTrainerPreferences::*Field,float Min,float Max)
        {
            auto Row=SNew(SHorizontalBox);Row->AddSlot().FillWidth(1).VAlign(VAlign_Center)[Label(Name,14)];
            Row->AddSlot().FillWidth(.7f)[SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max).Value_Lambda([this,Field](){return Preferences.*Field;}).OnValueChanged_Lambda([this,Field](float V){Preferences.*Field=V;ApplyPreferences();})];
            Content->AddSlot().AutoHeight().Padding(0,0,0,10)[Row];
        };
        FloatRow(TEXT("Valorant 灵敏度"),&FTrainerPreferences::Sensitivity,.001f,20.f);
        Content->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Label(TEXT("鼠标 DPI（用于距离换算）"),14)]
            +SHorizontalBox::Slot().FillWidth(.7f)[SNew(SSpinBox<int32>).MinValue(50).MaxValue(20000).Value_Lambda([this](){return Preferences.Dpi;}).OnValueChanged_Lambda([this](int32 V){Preferences.Dpi=V;ApplyPreferences();})]];
        Content->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(FString::Printf(TEXT("转身距离：%.2f cm / 360°"),UTrainingPreferences::CentimetersPer360(Preferences.Sensitivity,Preferences.Dpi)));}).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).ColorAndOpacity(FLinearColor(.15,.91,.84))];
        FloatRow(TEXT("准星长度"),&FTrainerPreferences::CrosshairLength,1,40);FloatRow(TEXT("准星间距"),&FTrainerPreferences::CrosshairGap,0,30);FloatRow(TEXT("准星粗细"),&FTrainerPreferences::CrosshairThickness,1,8);
        auto Colors=SNew(SHorizontalBox);const FLinearColor Palette[]={FLinearColor(0,1,1),FLinearColor(0,1,0),FLinearColor::White,FLinearColor(1,.2,.4)};const TCHAR* ColorNames[]={TEXT("青色"),TEXT("绿色"),TEXT("白色"),TEXT("粉色")};
        for(int I=0;I<4;++I)Colors->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(ColorNames[I],[this,I](){const FLinearColor C[]={FLinearColor(0,1,1),FLinearColor(0,1,0),FLinearColor::White,FLinearColor(1,.2,.4)};Preferences.CrosshairColor=C[I];ApplyPreferences();BuildMenu();return FReply::Handled();},Preferences.CrosshairColor.Equals(Palette[I]))];
        Content->AddSlot().AutoHeight().Padding(0,0,0,14)[Colors];FloatRow(TEXT("音量"),&FTrainerPreferences::Volume,0,1);
        auto Quality=SNew(SHorizontalBox);const TCHAR* QNames[]={TEXT("低画质"),TEXT("中画质"),TEXT("高画质"),TEXT("极高")};
        for(int I=0;I<4;++I)Quality->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(QNames[I],[this,I](){Preferences.QualityLevel=I;ApplyPreferences();BuildMenu();return FReply::Handled();},Preferences.QualityLevel==I)];
        Content->AddSlot().AutoHeight().Padding(0,0,0,14)[Quality];
        Content->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Button(TEXT("导出设置"),[this](){FString Error;MenuMessage=UTrainingPreferences::Export(Preferences,Error)?TEXT("已导出：")+UTrainingPreferences::GetExportFilePath():Error;BuildMenu();return FReply::Handled();})]
            +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("导入设置"),[this](){FString Error;if(UTrainingPreferences::Import(Preferences,Error)){UTrainingPreferences::Apply(this,Preferences);MenuMessage=TEXT("设置已导入");}else MenuMessage=Error;BuildMenu();return FReply::Handled();})]];
        Content->AddSlot().AutoHeight().Padding(0,12,0,0)[Label(TEXT("设置立即生效并保存。导入文件放在：")+UTrainingPreferences::GetImportFilePath(),12)];
    }
    else
    {
        Content->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("最近训练记录 · 保存在本机"),18)];
        auto Save=UTrainingSaveGame::Load();
        if(Save->Records.IsEmpty()) Content->AddSlot().AutoHeight()[Label(TEXT("完成一轮训练后，这里显示命中率、爆头率和反应时间。"),14)];
        for(int I=Save->Records.Num()-1;I>=0;--I)
        {
            const auto& R=Save->Records[I];Content->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SBorder).Padding(12).BorderBackgroundColor(FLinearColor(.05,.08,.10))
                [Label(FString::Printf(TEXT("%s  |  %s  |  %s\n%s  ·  射击 %d / 命中 %d / 爆头 %d  ·  命中 %.1f%% / 爆头 %.1f%%\n首次反应 %.0fms  ·  最佳 %.0fms  ·  停稳 %.0f%%  ·  偏差 %.2f°"),*R.Date,*R.ModeName,R.bWon?TEXT("完成"):TEXT("未完成"),*R.MapName,R.TotalShots,R.TotalHits,R.Headshots,R.HitRate,R.HeadshotRate,R.AverageReactionTimeMs,R.BestReactionTimeMs,R.StoppedShotRate,R.AverageAimErrorDegrees),13)]];
        }
    }
    if(!MenuMessage.IsEmpty())Body->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(MenuMessage,13,FLinearColor(.95,.71,.31))];
    Body->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Content]];
    MenuRoot=SNew(SOverlay)+SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.008,.015,.025,.97))]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(850).HeightOverride(680)[SNew(SBorder).Padding(28).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.018,.032,.047))[Body]]];
    Viewport->AddViewportWidgetContent(MenuRoot.ToSharedRef(),100);FInputModeGameAndUI Input;Input.SetWidgetToFocus(MenuRoot);Input.SetHideCursorDuringCapture(false);Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Input);
}
