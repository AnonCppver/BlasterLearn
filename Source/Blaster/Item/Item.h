// 可以被拾取的物品
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Item.generated.h"

UCLASS()
class BLASTER_API AItem : public AActor
{
	GENERATED_BODY()
	
public:	
	AItem();

	void ShowPickupWidget(bool bShow);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	UStaticMeshComponent* ItemMesh;

	UPROPERTY(VisibleAnywhere, Category = "Properties")
	class UWidgetComponent* PickupWidget;

	UPROPERTY(EditAnywhere, Category = "Properties")
	class UMaterial* PickupWidgetMaterial;

	UPROPERTY(EditAnywhere, Category = "Properties")
	class UInvItemComponent* InvItemComponent;

public:	
	virtual void Tick(float DeltaTime) override;

};
