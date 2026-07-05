// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

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

private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FGameplayTag FragmentTag = FGameplayTag::EmptyTag;
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
struct FInvImageFragment : public FInvFragment
{
	GENERATED_BODY()

	UTexture2D* GetIcon() const { return Icon; }

private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TObjectPtr<UTexture2D>Icon{ nullptr };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FVector2D IconDimension{ 64,64 };
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