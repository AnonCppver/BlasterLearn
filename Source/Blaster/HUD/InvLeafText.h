// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InvLeaf.h"
#include "InvLeafText.generated.h"

class UTextBlock;
UCLASS()
class BLASTER_API UInvLeafText : public UInvLeaf
{
	GENERATED_BODY()
public:
	void SetText(const FText& Text) const;
	virtual void NativePreConstruct() override;

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_LeafText;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 FontSize{ 12 };
};