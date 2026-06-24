// Fill out your copyright notice in the Description page of Project Settings.


#include "Item.h"
#include "Components/WidgetComponent.h"

AItem::AItem()
{
	PrimaryActorTick.bCanEverTick = true;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	SetRootComponent(ItemMesh);

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
}

void AItem::BeginPlay()
{
	Super::BeginPlay();

	if (PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}
	
}

void AItem::ShowPickupWidget(bool bShowWidget)
{
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}

	if(ItemMesh&&PickupWidgetMaterial)
	{
		ItemMesh->SetOverlayMaterial(bShowWidget ? PickupWidgetMaterial : nullptr);
	}
}

	void AItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

