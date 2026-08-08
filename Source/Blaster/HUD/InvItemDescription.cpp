#include "InvItemDescription.h"

#include "Components/SizeBox.h"

FVector2D UInvItemDescription::GetBoxSize() const
{
	return SizeBox->GetDesiredSize();
}

void UInvItemDescription::SetVisibility(ESlateVisibility InVisibility)
{
	for (auto Child : GetChildren())
	{
		Child->Collapse();
	}
	Super::SetVisibility(InVisibility);
}