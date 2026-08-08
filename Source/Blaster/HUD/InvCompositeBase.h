#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "InvCompositeBase.generated.h"

/**
 *
 */
	UCLASS()
	class BLASTER_API UInvCompositeBase : public UUserWidget
{
	GENERATED_BODY()
public:
	FGameplayTag GetFragmentTag() const { return FragmentTag; }
	void SetFragmentTag(const FGameplayTag& Tag) { FragmentTag = Tag; }
	virtual void Collapse();
	void Expand();

	using FuncType = TFunction<void(UInvCompositeBase*)>;
	virtual void ApplyFunction(FuncType Function) {}
private:

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FGameplayTag FragmentTag;
};