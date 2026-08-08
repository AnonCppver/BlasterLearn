// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InvLeaf.h"
#include "InvLeafLabeledValue.generated.h"

class UTextBlock;

UCLASS()
class BLASTER_API UInvLeafLabeledValue : public UInvLeaf
{
	GENERATED_BODY()
public:

	void SetText_Label(const FText& Text, bool bCollapse) const;
	void SetText_Value(const FText& Text, bool bCollapse) const;
	virtual void NativePreConstruct() override;

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Value;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 FontSize_Label{ 12 };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 FontSize_Value{ 18 };
};