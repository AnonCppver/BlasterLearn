

#include "InGameMenu.h"
#include "GameFramework/PlayerController.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "Blaster/Character/BlasterCharacter.h"

void UInGameMenu::MenuSetup()
{
	if (bIsMenuOpen) return;
	bIsMenuOpen = true;
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	bIsFocusable = true;

	UWorld* World = GetWorld();
	if (World)
	{
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}
	if (ReturnMainButton && !ReturnMainButton->OnClicked.IsBound())
	{
		ReturnMainButton->OnClicked.AddDynamic(this, &UInGameMenu::ReturnMainButtonClicked);
	}
	if (ReturnGameButton && !ReturnGameButton->OnClicked.IsBound())
	{
		ReturnGameButton->OnClicked.AddDynamic(this, &UInGameMenu::ReturnGameButtonClicked);
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if (MultiplayerSessionsSubsystem)
		{
			MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UInGameMenu::OnDestroySession);
		}
	}
}

bool UInGameMenu::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	return true;
}

void UInGameMenu::OnDestroySession(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		ReturnMainButton->SetIsEnabled(true);
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		AGameModeBase* GameMode = World->GetAuthGameMode<AGameModeBase>();
		if (GameMode)
		{
			GameMode->ReturnToMainMenuHost();
		}
		else
		{
			PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
			if (PlayerController)
			{
				PlayerController->ClientReturnToMainMenuWithTextReason(FText());
			}
		}
	}
}

void UInGameMenu::MenuTearDown()
{
	if (!bIsMenuOpen) return;
	bIsMenuOpen = false;
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
	if (ReturnMainButton && ReturnMainButton->OnClicked.IsBound())
	{
		ReturnMainButton->OnClicked.RemoveDynamic(this, &UInGameMenu::ReturnMainButtonClicked);
	}
	if (ReturnGameButton && ReturnGameButton->OnClicked.IsBound())
	{
		ReturnGameButton->OnClicked.RemoveDynamic(this, &UInGameMenu::ReturnGameButtonClicked);
	}
	if (MultiplayerSessionsSubsystem && MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.IsBound())
	{
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.RemoveDynamic(this, &UInGameMenu::OnDestroySession);
	}
}

void UInGameMenu::ReturnMainButtonClicked()
{
	ReturnMainButton->SetIsEnabled(false);

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* FirstPlayerController = World->GetFirstPlayerController();
		if (FirstPlayerController)
		{
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FirstPlayerController->GetPawn());
			if (BlasterCharacter)
			{
				BlasterCharacter->OnLeftGame.AddDynamic(this, &UInGameMenu::OnPlayerLeftGame);
				BlasterCharacter->ServerLeaveGame();
			}
			else
			{
				ReturnMainButton->SetIsEnabled(true);
			}
		}
	}
}

void UInGameMenu::OnPlayerLeftGame()
{
	UE_LOG(LogTemp, Warning, TEXT("OnPlayerLeftGame()"))
		if (MultiplayerSessionsSubsystem)
		{
			UE_LOG(LogTemp, Warning, TEXT("MultiplayerSessionsSubsystem valid"))
				MultiplayerSessionsSubsystem->DestroySession();
		}
}

void UInGameMenu::ReturnGameButtonClicked()
{
	MenuTearDown();
}