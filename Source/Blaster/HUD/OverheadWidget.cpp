// Fill out your copyright notice in the Description page of Project Settings.


#include "OverheadWidget.h"
#include "GameFramework/PlayerState.h"
#include "Components/TextBlock.h"

void UOverheadWidget::SetDisplayText(FString TextToDisplay, FSlateColor ColorToDisaplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
		DisplayText->SetColorAndOpacity(
			FSlateColor(
				FLinearColor(1.f, 1.f, 1.f, 0.4f)
			)
		);
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn, FSlateColor ColorToDisaplay)
{
	if (InPawn == nullptr) 
	{
		return;
	}

	APlayerState* PS = InPawn->GetPlayerState();
	if (PS == nullptr)
	{
		return;
	}

	FString PlayerName = PS->GetPlayerName();

	SetDisplayText(PlayerName, ColorToDisaplay);
}

void UOverheadWidget::NativeDestruct()
{
	RemoveFromParent();
	Super::NativeDestruct();
}