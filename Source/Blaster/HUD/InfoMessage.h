#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InfoMessage.generated.h"

class UTextBlock;
UCLASS()
class BLASTER_API UInfoMessage : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void MessageShow();

	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void MessageHide();

	void SetMessage(const FString& Message);

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextMessage;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float MessageLifetime{ 3.f };

	FTimerHandle MessageTimer;
	bool bIsMessageActive{ false };
};