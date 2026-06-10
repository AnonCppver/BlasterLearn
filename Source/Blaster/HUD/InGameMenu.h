// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InGameMenu.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API UInGameMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	bool bIsMenuOpen = false;
	void MenuSetup();
	void MenuTearDown();
	UFUNCTION()
	void OnPlayerLeftGame();

protected:
	virtual bool Initialize() override;

	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);

private:
	UPROPERTY(meta = (BindWidget))
	class UButton* ReturnMainButton;

	UPROPERTY(meta = (BindWidget))
	class UButton* ReturnGameButton;

	UFUNCTION()
	void ReturnMainButtonClicked();

	UFUNCTION()
	void ReturnGameButtonClicked();

	UPROPERTY()
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	UPROPERTY()
	class APlayerController* PlayerController;
};
