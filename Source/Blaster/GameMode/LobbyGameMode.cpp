// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameFramework/GameStateBase.h"
//#include "MultiplayerSessionsSubsystem.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		//UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		//check(Subsystem);

		if (NumberOfPlayers == 2/*Subsystem->DesiredNumPublicConnections*/)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, FString(TEXT("2 players in lobby")));
			}
			UWorld* World = GetWorld();
			if (World)
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow, FString(TEXT("try travelling seamlessly")));
				}
				bUseSeamlessTravel = true;

				//FString MatchType = Subsystem->DesiredMatchType;
				FString MatchType = TEXT("FreeShooting");
				World->ServerTravel(FString("/Game/LowerSector_Mod/Maps/LowerSector_Mod?listen"));
			}
		}
	}
}