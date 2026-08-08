#include "InvItemPopUp.h"

#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

void UInvItemPopUp::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Button_Split->OnClicked.AddDynamic(this, &ThisClass::SplitButtonClicked);
	Button_Drop->OnClicked.AddDynamic(this, &ThisClass::DropButtonClicked);
	Button_Consume->OnClicked.AddDynamic(this, &ThisClass::ConsumeButtonClicked);
	Slider_Split->OnValueChanged.AddDynamic(this, &ThisClass::SliderValueChanged);
}

void UInvItemPopUp::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	// 当光标离开弹窗范围时，移除弹窗
	Super::NativeOnMouseLeave(InMouseEvent);
	RemoveFromParent();
}

int32 UInvItemPopUp::GetSplitAmount() const
{
	return FMath::Floor(Slider_Split->GetValue());
}

void UInvItemPopUp::SplitButtonClicked()
{
	if (OnSplit.ExecuteIfBound(GetSplitAmount(), GridIndex))
	{
		RemoveFromParent();
	}
}

void UInvItemPopUp::DropButtonClicked()
{
	if (OnDrop.ExecuteIfBound(GridIndex))
	{
		RemoveFromParent();
	}
}

void UInvItemPopUp::ConsumeButtonClicked()
{
	if (OnConsume.ExecuteIfBound(GridIndex))
	{
		RemoveFromParent();
	}
}

void UInvItemPopUp::SliderValueChanged(float Value)
{
	Text_SplitAmount->SetText(FText::AsNumber(FMath::Floor(Value)));
}

void UInvItemPopUp::CollapseSplitButton() const
{
	Button_Split->SetVisibility(ESlateVisibility::Collapsed);
	Slider_Split->SetVisibility(ESlateVisibility::Collapsed);
	Text_SplitAmount->SetVisibility(ESlateVisibility::Collapsed);
}

void UInvItemPopUp::CollapseConsumeButton() const
{
	Button_Consume->SetVisibility(ESlateVisibility::Collapsed);
}

void UInvItemPopUp::SetSliderParams(const float Max, const float Value) const
{
	Slider_Split->SetMaxValue(Max);
	Slider_Split->SetMinValue(1);
	Slider_Split->SetValue(Value);
	Text_SplitAmount->SetText(FText::AsNumber(FMath::Floor(Value)));
}

FVector2D UInvItemPopUp::GetBoxSize() const
{
	return FVector2D(SizeBox_Root->GetWidthOverride(), SizeBox_Root->GetHeightOverride());
}