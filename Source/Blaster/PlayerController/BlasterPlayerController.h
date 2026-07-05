#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDShield(float Shield, float MaxShield);
	void SetHUDScore(float Score);
	void SetHUDDefeats(int32 Defeats);
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
	void SetHUDMatchCountdown(float CountdownTime);
	void SetHUDAnnouncementCountdown(float CountdownTime);

	virtual float GetServerTime();
	virtual void ReceivedPlayer() override;

	void OnMatchStateSet(FName State, bool bTeamsMatch = false);
	void HandleMatchHasStarted(bool bTeamsMatch = false);
	void HandleCooldown();

	UFUNCTION(Client, Reliable)
	void ClientPlayEliminationSound();

	float SingleTripTime = 0.f;

	void BroadcastElim(APlayerState* Attacker, APlayerState* Victim);

	UFUNCTION(Client, Reliable)
	void ClientElimAnnouncement(APlayerState* Attacker, APlayerState* Victim);

	UPROPERTY(ReplicatedUsing = OnRep_ShowTeamScores)
	bool bShowTeamScores = false;

	UFUNCTION()
	void OnRep_ShowTeamScores();

	// 这里客户端其实并没有在加入后获得队伍分数，而是
	void HideTeamScores();
	void InitTeamScores();
	void SetHUDRedTeamScore(int32 RedScore);
	void SetHUDBlueTeamScore(int32 BlueScore);
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;



	void SetHUDTime();
	void PollInit();

	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	float ClientServerDelta = 0.f;

	UPROPERTY(EditAnywhere, Category = Time)
	float TimeSyncFrequency = 5.f;

	float TimeSyncRunningTime = 0.f;
	void CheckTimeSync(float DeltaTime);

	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();

	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime);

	void HighPingWarning();
	void StopHighPingWarning();
	void CheckPing(float DeltaTime);

	void ShowReturnToMainMenu();

private:
	UPROPERTY()
	class ABlasterHUD* BlasterHUD;

	UPROPERTY(EditAnywhere, Category = "Sound")
	class USoundCue* EliminationSound;

	// Match time synchronization variables
	float LevelStartingTime = 0.f;
	float MatchTime = 0.f;
	float WarmupTime = 0.f;
	float CooldownTime = 0.f;
	uint32 CountdownInt = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;

	UFUNCTION()
	void OnRep_MatchState();

	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players);
	FString GetTeamsInfoText(class ABlasterGameState* BlasterGameState);

	UPROPERTY()
	class UCharacterOverlay* CharacterOverlay;

	// HUD variables
	float HUDHealth;
	bool bInitializeHealth = false;
	float HUDMaxHealth;
	float HUDScore;
	bool bInitializeScore = false;
	int32 HUDDefeats;
	bool bInitializeDefeats = false;
	float HUDShield;
	bool bInitializeShield = false;
	float HUDMaxShield;
	float HUDCarriedAmmo;
	bool bInitializeCarriedAmmo = false;
	float HUDWeaponAmmo;
	bool bInitializeWeaponAmmo = false;
	bool bInitializeTeamScores = false;
	bool bHideTeamScores = false;

	// Network
	float HighPingRunningTime = 0.f;

	UPROPERTY(EditAnywhere)
	float HighPingDuration = 5.f;

	float PingAnimationRunningTime = 0.f;

	UPROPERTY(EditAnywhere)
	float CheckPingFrequency = 20.f;

	UPROPERTY(EditAnywhere)
	float HighPingThreshold = 50.f;

	FTimerHandle MatchCountdownTimer;

	/**
	* Return to main menu
	*/

	UPROPERTY(EditAnywhere, Category = HUD)
	TSubclassOf<class UUserWidget> InGameMenuWidget;

	UPROPERTY()
	class UInGameMenu* InGameMenu;

	/*
	* Inventory
	*/
	UPROPERTY(EditDefaultsOnly, Category = Inventory)
	TObjectPtr<class UInputMappingContext> InventoryMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = Inventory)
	TObjectPtr<class UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, Category = Inventory)
	TObjectPtr<class UInputAction> ToggleInventoryAction;

	UPROPERTY(EditDefaultsOnly, Category = Inventory)
	TEnumAsByte<ECollisionChannel> ItemTraceChannel;

	TWeakObjectPtr<class UInvComponent> InventoryComponent;

	TWeakObjectPtr<AActor> FocusedItem;
	TWeakObjectPtr<AActor> LastFocusedItem;

	void PrimaryInteract();

	void TraceItem();

	void NoRoomInInventory();
public:
	UFUNCTION(BlueprintCallable)
	void ToggleInventory();
	FORCEINLINE void setLevelStartingTime(float Time) { LevelStartingTime = Time; }
};