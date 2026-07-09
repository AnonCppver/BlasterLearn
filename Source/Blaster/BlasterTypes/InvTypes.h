#pragma once

#include "InvTypes.generated.h"

class UInvItem;

UENUM(BlueprintType)
enum class EInvItemCategory : uint8
{
	Equippable,
	Consumable,
	Craftable,
	None
};

USTRUCT()
struct FInvSlotAvailability
{
	GENERATED_BODY()

	FInvSlotAvailability() {}
	FInvSlotAvailability(int32 ItemIndex, int32 Room, bool bHasItem) : Index(ItemIndex), AmountToFill(Room), bItemAtIndex(bHasItem) {}

	int32 Index{ INDEX_NONE };
	int32 AmountToFill{ 0 };
	bool bItemAtIndex{ false };
};

USTRUCT()
struct FInvSlotAvailabilityResult
{
	GENERATED_BODY()

	FInvSlotAvailabilityResult() {}

	TWeakObjectPtr<UInvItem> Item;
	int32 TotalRoomToFill{ 0 };
	int32 Remainder{ 0 };
	bool bStackable{ false };
	TArray<FInvSlotAvailability> SlotAvailabilities;
};

// 将每个格子分为四个象限，
UENUM(BlueprintType)
enum class EInvTileQuadrant : uint8
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	None
};

USTRUCT(BlueprintType)
struct FInvTileParameters
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Inventory")
	FIntPoint TileCoordinats{};// 物品在格子中的坐标

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Inventory")
	int32 TileIndex{ INDEX_NONE };// 坐标对应的格子索引

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Inventory")
	EInvTileQuadrant TileQuadrant{ EInvTileQuadrant::None };// 坐标对应的格子象限
};

inline bool operator==(const FInvTileParameters& A, const FInvTileParameters& B)
{
	return A.TileCoordinats == B.TileCoordinats && A.TileIndex == B.TileIndex && A.TileQuadrant == B.TileQuadrant;
}

USTRUCT()
struct FInvSpaceQueryResult
{
	GENERATED_BODY()

	// 空间没有物品为true
	bool bHasSpace{ false };

	// 如果有一个可以交换的物品，则为有效
	TWeakObjectPtr<UInvItem> ValidItem = nullptr;

	// 如果有一个有效物品，则为其左上角索引
	int32 UpperLeftIndex{ INDEX_NONE };
};