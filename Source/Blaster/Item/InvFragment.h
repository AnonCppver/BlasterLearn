// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InstancedStruct.h"

#include "InvFragment.generated.h"


USTRUCT(BlueprintType)
struct FInvFragment
{
	GENERATED_BODY()

	FInvFragment() = default;
	FInvFragment(const FInvFragment&) = default;
	FInvFragment& operator=(const FInvFragment&) = default;
	FInvFragment(FInvFragment&&) = default;
	FInvFragment& operator=(FInvFragment&&) = default;
	virtual ~FInvFragment(){}

	FORCEINLINE void SetFragmentTag(FGameplayTag InFragmentTag) { FragmentTag = InFragmentTag; }
	FORCEINLINE FGameplayTag GetFragmentTag() const{return FragmentTag;}

	virtual void Manifest() {}

private:
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (Categories = "FragmentTags"))
	FGameplayTag FragmentTag = FGameplayTag::EmptyTag;
};

/*
 * Item fragment specifically for assimilation into a widget.
 */
// 为了
class UInvCompositeBase;
USTRUCT(BlueprintType)
struct FInvAssimilateFragment : public FInvFragment
{
	GENERATED_BODY()

	virtual void Assimilate(UInvCompositeBase* Composite) const;
protected:
	bool MatchesWidgetTag(const UInvCompositeBase* Composite) const;
};


USTRUCT(BlueprintType)
struct FInvGridFragment : public FInvFragment
{
	GENERATED_BODY()

	FIntPoint GetGridSize() const { return GridSize; }
	void SetGridSize(const FIntPoint& InGridSize) { GridSize = InGridSize; }
	float GetGridPadding() const { return GridPadding; }
	void SetGridPadding(float InGridPadding) { GridPadding = InGridPadding; }

private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FIntPoint GridSize{ 1,1 };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float GridPadding = 0.f;
};

USTRUCT(BlueprintType)
struct FInvImageFragment : public FInvAssimilateFragment
{
	GENERATED_BODY()

	UTexture2D* GetIcon() const { return Icon; }
	virtual void Assimilate(UInvCompositeBase* Composite) const override;

private:

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TObjectPtr<UTexture2D> Icon{ nullptr };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FVector2D IconDimensions{ 44.f, 44.f };
};

USTRUCT(BlueprintType)
struct FInvTextFragment : public FInvAssimilateFragment
{
	GENERATED_BODY()

	FText GetText() const { return FragmentText; }
	void SetText(const FText& Text) { FragmentText = Text; }
	virtual void Assimilate(UInvCompositeBase* Composite) const override;

private:

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FText FragmentText;
};

USTRUCT(BlueprintType)
struct FInvLabeledNumberFragment : public FInvAssimilateFragment
{
	GENERATED_BODY()

	virtual void Assimilate(UInvCompositeBase* Composite) const override;
	virtual void Manifest() override;
	float GetValue() const { return Value; }

	// When manifesting for the first time, this fragment will randomize. However, onee equipped
	// and dropped, an item should retain the same value, so randomization should not occur.
	bool bRandomizeOnManifest{ true };

private:

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FText Text_Label{};

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	float Value{ 0.f };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float Min{ 0 };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float Max{ 0 };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	bool bCollapseLabel{ false };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	bool bCollapseValue{ false };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 MinFractionalDigits{ 1 };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 MaxFractionalDigits{ 1 };
};

USTRUCT(BlueprintType)
struct FInvStackableFragment : public FInvFragment
{
	GENERATED_BODY()

	int32 GetMaxStackSize() const { return MaxStackSize; }
	int32 GetStackCount() const { return StackCount; }
	void SetStackCount(int32 Count) { StackCount = Count; }

private:

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 MaxStackSize{ 1 };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 StackCount{ 1 };
};

USTRUCT(BlueprintType)
struct FInvConsumeModifier : public FInvLabeledNumberFragment
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC) {}
};

USTRUCT(BlueprintType)
struct FInvConsumableFragment : public FInvAssimilateFragment
{
	GENERATED_BODY()

	virtual void OnConsume(APlayerController* PC){}
	virtual void Assimilate(UInvCompositeBase* Composite) const override;
	virtual void Manifest() override;
private:

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FInvConsumeModifier>> ConsumeModifiers;
};
