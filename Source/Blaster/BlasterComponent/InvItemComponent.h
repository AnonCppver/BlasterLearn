// 可被拾取actor的组件，用于相关交互

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/Item/InvItemManifest.h"
#include "InvItemComponent.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Blueprintable)
class BLASTER_API UInvItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInvItemComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitItemManifest(FInvItemManifest CopyOfManifest);
	FInvItemManifest GetItemManifest() const { return ItemManifest; }
	FInvItemManifest& GetItemManifestMutable() { return ItemManifest; }
	void PickedUp();
protected:

	//UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	//void OnPickedUp();

private:

	UPROPERTY(Replicated, EditAnywhere, Category = "Inventory")
	FInvItemManifest ItemManifest;
};