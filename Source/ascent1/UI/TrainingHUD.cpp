#include "TrainingHUD.h"
#include "Core/AscPlayerController.h"
#include "Training/TrainerSession.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"

void ATrainingHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;
    const auto PC=Cast<AAscPlayerController>(PlayerOwner);if(!PC||PC->IsMenuOpen())return;
    const auto S=PC->GetSession();if(!S||!S->IsRunning())return;
    const auto& P=PC->GetPreferences();
    if(S->GetFlashAlpha()>0)DrawRect(FLinearColor(1,1,1,S->GetFlashAlpha()),0,0,Canvas->ClipX,Canvas->ClipY);
    const FVector2D C(Canvas->ClipX*.5f,Canvas->ClipY*.5f);const float Gap=P.CrosshairGap,L=P.CrosshairLength;
    const FVector2D A[]={FVector2D(C.X-Gap-L,C.Y),FVector2D(C.X+Gap,C.Y),FVector2D(C.X,C.Y-Gap-L),FVector2D(C.X,C.Y+Gap)};
    const FVector2D B[]={FVector2D(C.X-Gap,C.Y),FVector2D(C.X+Gap+L,C.Y),FVector2D(C.X,C.Y-Gap),FVector2D(C.X,C.Y+Gap+L)};
    for(int I=0;I<4;++I){FCanvasLineItem Line(A[I],B[I]);Line.SetColor(P.CrosshairColor);Line.LineThickness=P.CrosshairThickness;Canvas->DrawItem(Line);}
    DrawRect(FLinearColor(.008,.015,.025,.8),18,18,Canvas->ClipX-270,108);
    DrawText(S->GetModeName()+TEXT(" · ")+S->GetPointName(),FLinearColor(.18,.96,.85),30,25,nullptr,1.15f);
    DrawText(FString::Printf(TEXT("生命 %d    击杀 %d / %d    射击 %d    命中 %d    爆头 %d"),S->Health,S->Kills,S->RequiredKills,S->Shots,S->Hits,S->Headshots),FLinearColor::White,30,54,nullptr,1.f);
    DrawText(FString::Printf(TEXT("命中 %.1f%%    爆头 %.1f%%    最近 %.0fms    最佳 %.0fms"),S->Shots?100.f*S->Hits/S->Shots:0,S->Hits?100.f*S->Headshots/S->Hits:0,S->LastReactionMs,S->BestReactionMs),FLinearColor(.75,.82,.87),30,83,nullptr,.9f);
    DrawText(FString::Printf(TEXT("开枪速度 %.0f cm/s  ·  停稳 %.0f%%  ·  瞄准偏差 %.2f°"),S->LastShotSpeed,S->Shots?100.f*S->StoppedShots/S->Shots:0,S->AverageAimErrorDegrees),FLinearColor(.68,.79,.85),30,108,nullptr,.85f);
    if(S->Mode==ECombatPracticeMode::Hold&&!S->IsFinished())
    {
        const float Remaining=S->GetHoldReactionRemainingMs();
        const FLinearColor Color=Remaining>80?FLinearColor(.18f,.96f,.85f):FLinearColor(1,.3f,.25f);
        DrawRect(FLinearColor(.008f,.015f,.025f,.88f),18,136,340,54);
        DrawText(S->IsHoldReactionStarted()?FString::Printf(TEXT("架枪时限 200ms · 剩余 %.0fms"),Remaining):TEXT("架枪时限 200ms · 等待假人露出"),Color,30,145,nullptr,.95f);
        DrawRect(FLinearColor(.12f,.16f,.20f),30,174,316,5);
        DrawRect(Color,30,174,316*Remaining/200.f,5);
    }
    DrawRect(FLinearColor(.008,.015,.025,.85),18,Canvas->ClipY-90,Canvas->ClipX-36,70);
    DrawText(S->GetStatus(),S->IsFinished()?(S->bWon?FLinearColor::Green:FLinearColor(1,.35,.35)):FLinearColor::White,30,Canvas->ClipY-80,nullptr,1.05f);
    DrawText(TEXT("WASD 移动 | Shift 静步 | Ctrl 蹲 | R 重练 | N 下一点 | Esc 暂停 | F5/F6/F7 个人点位"),FLinearColor(.65,.75,.81),30,Canvas->ClipY-48,nullptr,.85f);
    if(S->IsFinished())
    {
        const float ResultX=Canvas->ClipX*.5f-260,ResultY=Canvas->ClipY*.28f;
        DrawRect(FLinearColor(.008,.015,.025,.92),ResultX,ResultY,520,105);
        DrawText(S->bWon?TEXT("本轮完成"):TEXT("本轮失败"),S->bWon?FLinearColor::Green:FLinearColor(1,.35,.35),ResultX+22,ResultY+14,nullptr,1.6f);
        DrawText(TEXT("训练已结束：按 R 重练 / N 下一点 / Esc 菜单"),FLinearColor::White,ResultX+22,ResultY+64,nullptr,1.05f);
    }
    if(!Minimap)Minimap=LoadObject<UTexture2D>(nullptr,TEXT("/Game/CombatTraining/UI/T_AscentMap.T_AscentMap"));
    const float X=Canvas->ClipX-232,Y=18,W=214,H=160.5;
    DrawRect(FLinearColor(.008,.015,.025,.9),X-4,Y-4,W+8,H+32);
    if(Minimap)DrawTexture(Minimap,X,Y,W,H,0,0,1,1,FLinearColor::White,BLEND_Opaque);
    auto Mark=[this,X,Y,W,H](const FVector& Pos,FLinearColor Color)
    {const float U=FMath::Clamp((Pos.X-1450.f)/19400.f+.5f,0.f,1.f),V=FMath::Clamp((Pos.Y+4570.f)/14550.f+.5f,0.f,1.f);DrawRect(FLinearColor::Black,X+U*W-4,Y+V*H-4,8,8);DrawRect(Color,X+U*W-3,Y+V*H-3,6,6);};
    if(PC->GetPawn())Mark(PC->GetPawn()->GetActorLocation(),FLinearColor(0,1,.8));
    if(S->GetCurrentTarget())Mark(S->GetTargetFloor(),FLinearColor(1,.25,.45));
    DrawText(TEXT("青色 玩家 · 粉色 敌人"),FLinearColor(.78,.9,.92),X,Y+H+4,nullptr,.8f);
    DrawFeedback(S);
}

void ATrainingHUD::DrawFeedback(const ATrainerSession* Session)
{
    const auto& Hit=Session->GetHitFeedback();
    const float Scale=FMath::Min(Canvas->ClipX/1280.f,Canvas->ClipY/720.f);
    const FVector2D Center(Canvas->ClipX*.5f,Canvas->ClipY*.5f);
    auto Line=[this,Scale](FVector2D A,FVector2D B,FLinearColor Color,float Thickness=2.f)
    {FCanvasLineItem Item(A,B);Item.SetColor(Color);Item.LineThickness=Thickness*Scale;Canvas->DrawItem(Item);};
    auto CenteredText=[this](const FString& Text,FLinearColor Color,float X,float Y,float TextScale)
    {float W=0,H=0;GetTextSize(Text,W,H,nullptr,TextScale);DrawText(Text,Color,X-W*.5f,Y,nullptr,TextScale);};
    const float Hurt=Session->GetPlayerHurtAlpha();
    if(Hurt>0)
    {
        // Leave the view center clear while showing an immediate health loss.
        for(int32 I=0;I<8;++I)
        {
            const float Inset=I*5.f*Scale;
            const FLinearColor Red(1,.055f,.075f,Hurt*.23f*(1.f-I/8.f));
            DrawRect(Red,Inset,Inset,Canvas->ClipX-2*Inset,5*Scale);
            DrawRect(Red,Inset,Canvas->ClipY-Inset-5*Scale,Canvas->ClipX-2*Inset,5*Scale);
            DrawRect(Red,Inset,Inset,5*Scale,Canvas->ClipY-2*Inset);
            DrawRect(Red,Canvas->ClipX-Inset-5*Scale,Inset,5*Scale,Canvas->ClipY-2*Inset);
        }
        CenteredText(FString::Printf(TEXT("受到攻击 · 生命 %d"),Session->Health),FLinearColor(1,.3f,.35f,Hurt),Center.X,Center.Y+100*Scale,.95f*Scale);
    }
    if(Hit.HitRemaining>0)
    {
        const bool Head=Hit.Region==EHitRegion::Head;
        const float Alpha=FMath::Clamp(Hit.HitRemaining/.15f,0.f,1.f);
        const FLinearColor Color=Head?FLinearColor(1,.77f,.22f,Alpha):FLinearColor(1,1,1,Alpha);
        const float Inner=10*Scale,Outer=(Head?21:17)*Scale;
        for(int32 X:{-1,1})for(int32 Y:{-1,1})
            Line(Center+FVector2D(X*Inner,Y*Inner),Center+FVector2D(X*Outer,Y*Outer),Color,Head?2.7f:2.f);
        const TCHAR* Region=Head?TEXT("头部"):Hit.Region==EHitRegion::Legs?TEXT("腿部"):TEXT("身体");
        const FString Text=Hit.bKilled?FString::Printf(TEXT("%s -%.0f"),Region,Hit.Damage):FString::Printf(TEXT("%s -%.0f · 剩余 %.0f"),Region,Hit.Damage,Hit.RemainingHealth);
        CenteredText(Text,Color,Center.X,Center.Y+36*Scale,.95f*Scale);
    }
    if(Hit.KillRemaining>0)
    {
        const float Alpha=FMath::Clamp(Hit.KillRemaining/.3f,0.f,1.f);
        const float Pop=1.f+.12f*FMath::Clamp((Hit.KillRemaining-1.f)/.2f,0.f,1.f);
        const FVector2D Badge(Center.X,Canvas->ClipY*.77f);
        const float Radius=32*Scale*Pop;
        const FLinearColor Accent=Hit.bHeadshotKill?FLinearColor(1,.72f,.17f,Alpha):FLinearColor(.22f,1,.82f,Alpha);
        DrawRect(FLinearColor(.008f,.015f,.025f,.72f*Alpha),Badge.X-90*Scale,Badge.Y-44*Scale,180*Scale,118*Scale);
        // Original skull badge, with a red headshot cross and a small expanding ring.
        for(int32 I=0;I<32;++I)
        {
            const float A=I*2*PI/32,B=(I+1)*2*PI/32;
            Line(Badge+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius,Badge+FVector2D(FMath::Cos(B),FMath::Sin(B))*Radius,Accent,2.f);
        }
        const float U=Scale*Pop;
        DrawRect(FLinearColor(1,1,1,Alpha),Badge.X-13*U,Badge.Y-16*U,26*U,23*U);
        DrawRect(FLinearColor(1,1,1,Alpha),Badge.X-8*U,Badge.Y+7*U,16*U,8*U);
        DrawRect(FLinearColor(.012f,.02f,.028f,Alpha),Badge.X-9*U,Badge.Y-5*U,6*U,6*U);
        DrawRect(FLinearColor(.012f,.02f,.028f,Alpha),Badge.X+3*U,Badge.Y-5*U,6*U,6*U);
        for(int32 I:{-1,0,1})DrawRect(FLinearColor(.012f,.02f,.028f,Alpha),Badge.X+I*5*U-1*U,Badge.Y+9*U,2*U,6*U);
        if(Hit.bHeadshotKill)
        {
            const FLinearColor Red(1,.13f,.2f,Alpha);
            Line(Badge+FVector2D(-6,-14)*U,Badge+FVector2D(6,-2)*U,Red,3.f);
            Line(Badge+FVector2D(6,-14)*U,Badge+FVector2D(-6,-2)*U,Red,3.f);
        }
        CenteredText(Hit.bHeadshotKill?TEXT("爆头击杀"):TEXT("击杀确认"),Accent,Center.X,Badge.Y+40*Scale,1.15f*Scale);
        CenteredText(FString::Printf(TEXT("%d / %d"),Session->Kills,Session->RequiredKills),FLinearColor(1,1,1,Alpha),Center.X,Badge.Y+65*Scale,.8f*Scale);
    }
}
