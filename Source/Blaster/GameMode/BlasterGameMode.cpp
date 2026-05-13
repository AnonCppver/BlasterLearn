//#include "BlasterGameMode.h"
//#include "Blaster/Character/BlasterCharacter.h"
//#include "Blaster/PlayerController/BlasterPlayerController.h"
//#include "Kismet/GameplayStatics.h"
//#include "GameFramework/PlayerStart.h"
//#include "Blaster/PlayerState/BlasterPlayerState.h"
//#include "Blaster/GameState/BlasterGameState.h"
//
//namespace MatchState
//{
//	const FName Cooldown = FName("Cooldown");
//}
//
//void ABlasterGameMode::PlayerEliminated(class ABlasterCharacter* ElimmedCharacter, class ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController)
//{
//	if (AttackerController == nullptr || AttackerController->PlayerState == nullptr) return;
//	if (VictimController == nullptr || VictimController->PlayerState == nullptr) return;
//	ABlasterPlayerState* AttackerPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
//	ABlasterPlayerState* VictimPlayerState = VictimController ? Cast<ABlasterPlayerState>(VictimController->PlayerState) : nullptr;
//
//	ABlasterGameState* BlasterGameState = GetGameState<ABlasterGameState>();
//
//	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && BlasterGameState)
//	{
//		AttackerPlayerState->AddToScore(1.f);
//		BlasterGameState->UpdateTopScore(AttackerPlayerState);
//		AttackerController->ClientPlayEliminationSound();
//	}
//	if (VictimPlayerState)
//	{
//		VictimPlayerState->AddToDefeats(1);
//	}
//	if (ElimmedCharacter)
//	{
//		ElimmedCharacter->Elim();
//	}
//}
//
//void ABlasterGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
//{
//	if (ElimmedCharacter)
//	{
//		ElimmedCharacter->Reset();
//		ElimmedCharacter->Destroy();
//	}
//	if (ElimmedController)
//	{
//		TArray<AActor*> PlayerStarts;
//		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
//		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
//		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
//	}
//}
//
//ABlasterGameMode::ABlasterGameMode()
//{
//	bDelayedStart = true; // MatchState::WaitingToStart
//}
//
//void ABlasterGameMode::BeginPlay()
//{
//	Super::BeginPlay();
//	LevelStartingTime = GetWorld()->GetTimeSeconds();
//	UE_LOG(LogTemp, Warning,
//		TEXT("[TravelDebug] GameMode::BeginPlay LevelStartingTime=%f %s"),
//		LevelStartingTime,
//		*BuildTravelDebugString()
//	);
//	// [2026.05.10-11.19.45:951][662]LogTemp: Warning: Controller::ClientJoinMidgame complete and LevelStartingTime = 0.000000
//	// [2026.05.10 - 11.19.45:951] [662] LogTemp: Warning: GameMode::BeginPlay and set LevelStartingTime = 7.039889
//	// 服务端的ServerCheckMatchState似乎一直早于GameMode::BeginPlay 导致使用了未初始化的LevelStartingTime
//	// 在这里强制同步
//	UE_LOG(LogTemp, Warning, TEXT("GameMode::BeginPlay and set LevelStartingTime = %f"), LevelStartingTime);
//	ABlasterPlayerController* BlasterPlayerController =
//		Cast<ABlasterPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
//
//	if (BlasterPlayerController)
//	{
//		BlasterPlayerController->setLevelStartingTime(LevelStartingTime);
//	}
//}
//
//void ABlasterGameMode::Tick(float DeltaTime)
//{
//	Super::Tick(DeltaTime);
//
//	if (MatchState == MatchState::WaitingToStart)
//	{
//		CountdownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
//		if (CountdownTime <= 0.f)
//		{
//			StartMatch();
//		}
//	}
//	else if (MatchState == MatchState::InProgress)
//	{
//		CountdownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
//		if (CountdownTime <= 0.f)
//		{
//			SetMatchState(MatchState::Cooldown);
//		}
//	}
//	else if (MatchState == MatchState::Cooldown)
//	{
//		CountdownTime = CooldownTime + WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
//		if (CountdownTime <= 0.f)
//		{
//			RestartGame();
//		}
//	}
//}
//
//void ABlasterGameMode::OnMatchStateSet()
//{
//	Super::OnMatchStateSet();
//
//	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
//	{
//		ABlasterPlayerController* BlasterPlayer = Cast<ABlasterPlayerController>(*It);
//		if (BlasterPlayer)
//		{
//			BlasterPlayer->OnMatchStateSet(MatchState);
//		}
//	}
//}

#include "BlasterGameMode.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/GameState/BlasterGameState.h"

#include "Engine/World.h"
#include "Engine/Level.h"

namespace MatchState
{
	const FName Cooldown = FName("Cooldown");
}

void ABlasterGameMode::PlayerEliminated(class ABlasterCharacter* ElimmedCharacter, class ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController)
{
	if (AttackerController == nullptr || AttackerController->PlayerState == nullptr) return;
	if (VictimController == nullptr || VictimController->PlayerState == nullptr) return;

	ABlasterPlayerState* AttackerPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
	ABlasterPlayerState* VictimPlayerState = VictimController ? Cast<ABlasterPlayerState>(VictimController->PlayerState) : nullptr;

	ABlasterGameState* BlasterGameState = GetGameState<ABlasterGameState>();

	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && BlasterGameState)
	{
		AttackerPlayerState->AddToScore(1.f);
		BlasterGameState->UpdateTopScore(AttackerPlayerState);
		AttackerController->ClientPlayEliminationSound();
	}

	if (VictimPlayerState)
	{
		VictimPlayerState->AddToDefeats(1);
	}

	if (ElimmedCharacter)
	{
		ElimmedCharacter->Elim();
	}
}

void ABlasterGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
	if (ElimmedCharacter)
	{
		ElimmedCharacter->Reset();
		ElimmedCharacter->Destroy();
	}

	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

		int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);

		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
	}
}

ABlasterGameMode::ABlasterGameMode()
{
	bDelayedStart = true; // MatchState::WaitingToStart
}

void ABlasterGameMode::BeginPlay()
{
	Super::BeginPlay();

	LevelStartingTime = GetWorld()->GetTimeSeconds();
	// [2026.05.10-11.19.45:951][662]LogTemp: Warning: Controller::ClientJoinMidgame complete and LevelStartingTime = 0.000000
	// [2026.05.10 - 11.19.45:951] [662] LogTemp: Warning: GameMode::BeginPlay and set LevelStartingTime = 7.039889
	// 服务端的ServerCheckMatchState似乎一直早于GameMode::BeginPlay 导致使用了未初始化的LevelStartingTime
	// 在这里强制同步
	UE_LOG(LogTemp, Warning, TEXT("GameMode::BeginPlay and set LevelStartingTime = %f"), LevelStartingTime);

	ABlasterPlayerController* BlasterPlayerController =
		Cast<ABlasterPlayerController>(UGameplayStatics::GetPlayerController(this, 0));

	if (BlasterPlayerController)
	{
		BlasterPlayerController->setLevelStartingTime(LevelStartingTime);
	}
}

void ABlasterGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MatchState == MatchState::WaitingToStart)
	{
		CountdownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			StartMatch();
		}
	}
	else if (MatchState == MatchState::InProgress)
	{
		CountdownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			SetMatchState(MatchState::Cooldown);
		}
	}
	else if (MatchState == MatchState::Cooldown)
	{
		CountdownTime = CooldownTime + WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f)
		{
			RestartGame();
		}
	}
}

void ABlasterGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABlasterPlayerController* BlasterPlayer = Cast<ABlasterPlayerController>(*It);
		if (BlasterPlayer)
		{
			BlasterPlayer->OnMatchStateSet(MatchState);
		}
	}
}