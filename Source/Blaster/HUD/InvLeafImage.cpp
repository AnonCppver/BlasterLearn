#include "InvLeafImage.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"

void UInvLeafImage::SetImage(UTexture2D* Texture) const
{
	Image_Icon->SetBrushFromTexture(Texture);
}

void UInvLeafImage::SetBoxSize(const FVector2D& Size) const
{
	SizeBox_Icon->SetWidthOverride(Size.X);
	SizeBox_Icon->SetHeightOverride(Size.Y);
}

void UInvLeafImage::SetImageSize(const FVector2D& Size) const
{
	Image_Icon->SetDesiredSizeOverride(Size);
}

FVector2D UInvLeafImage::GetImageSize() const
{
	return Image_Icon->GetDesiredSize();
}