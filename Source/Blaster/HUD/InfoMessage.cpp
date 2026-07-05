#include "InfoMessage.h"

#include "Components/TextBlock.h"

void UInfoMessage::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	TextMessage->SetText(FText::GetEmpty());
	MessageHide();
}

void UInfoMessage::SetMessage(const FString& Message)
{
	TextMessage->SetText(FText::FromString(Message));

	if (!bIsMessageActive)
	{
		MessageShow();
	}
	bIsMessageActive = true;

	GetWorld()->GetTimerManager().SetTimer(MessageTimer, [this]()
		{
			MessageHide();
			bIsMessageActive = false;
		}, MessageLifetime, false);
}