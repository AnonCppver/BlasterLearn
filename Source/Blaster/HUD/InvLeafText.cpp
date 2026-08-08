#include "InvLeafText.h"

#include "Components/TextBlock.h"

void UInvLeafText::SetText(const FText& Text) const
{
	Text_LeafText->SetText(Text);
}

void UInvLeafText::NativePreConstruct()
{
	Super::NativePreConstruct();

	FSlateFontInfo FontInfo = Text_LeafText->GetFont();
	FontInfo.Size = FontSize;

	Text_LeafText->SetFont(FontInfo);
}