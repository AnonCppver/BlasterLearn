#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InvComposite.h"

#include "InvItemDescription.generated.h"

class USizeBox;
UCLASS()
class BLASTER_API UInvItemDescription : public UInvComposite
{
	GENERATED_BODY()

public:

	FVector2D GetBoxSize() const;
	virtual void SetVisibility(ESlateVisibility InVisibility) override;

private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox;
};