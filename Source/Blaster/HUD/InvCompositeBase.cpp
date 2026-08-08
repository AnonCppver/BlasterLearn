#include "InvCompositeBase.h"

void UInvCompositeBase::Collapse()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UInvCompositeBase::Expand()
{
	SetVisibility(ESlateVisibility::Visible);
}