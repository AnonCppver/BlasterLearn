// 一些常用函数库

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Blaster/BlasterComponent/InvComponent.h"
#include "Blaster/BlasterComponent/InvItemComponent.h"
#include "InvUtils.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API UInvUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static int32 GetIndexFromPosition(const FIntPoint& Position, int32 Col);
	static FIntPoint GetPositionFromIndex(int32 Index, int32 Col);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static UInvComponent* GetInventoryComponent(const APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static EInvItemCategory GetItemCategoryFromItemComp(UInvItemComponent* ItemComp);

	template<typename T, typename FuncT>
	static void ForEach2D(TArray<T>& Array, int32 Index, const FIntPoint& Range2D, int32 GridColumns, const FuncT& Function);

};

template<typename T, typename FuncT>
void UInvUtils::ForEach2D(TArray<T>& Array, int32 Index, const FIntPoint& Range2D, int32 GridColumns, const FuncT& Function)
{
	for (int32 j = 0; j < Range2D.Y; ++j)
	{
		for (int32 i = 0; i < Range2D.X; ++i)
		{
			const FIntPoint Coordinates = UInvUtils::GetPositionFromIndex(Index, GridColumns) + FIntPoint(i, j);
			const int32 TileIndex = UInvUtils::GetIndexFromPosition(Coordinates, GridColumns);
			if (Array.IsValidIndex(TileIndex))
			{
				Function(Array[TileIndex]);
			}
		}
	}
}
