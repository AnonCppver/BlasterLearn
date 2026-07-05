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
	FIntPoint TileCoordinats{};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Inventory")
	int32 TileIndex{ INDEX_NONE };

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Inventory")
	EInvTileQuadrant TileQuadrant{ EInvTileQuadrant::None };
};

inline bool operator==(const FInvTileParameters& A, const FInvTileParameters& B)
{
	return A.TileCoordinats == B.TileCoordinats && A.TileIndex == B.TileIndex && A.TileQuadrant == B.TileQuadrant;
}

USTRUCT()
struct FInvSpaceQueryResult
{
	GENERATED_BODY()

	// True if the space queried has no items in it
	bool bHasSpace{ false };

	// Valid if there's a single item we can swap with
	TWeakObjectPtr<UInvItem> ValidItem = nullptr;

	// Upper left index of the valid item, if there is one
	int32 UpperLeftIndex{ INDEX_NONE };
};