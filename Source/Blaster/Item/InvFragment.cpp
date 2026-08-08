#include "InvFragment.h"

#include "Blaster/HUD/InvCompositeBase.h"
#include "Blaster/HUD/InvLeafImage.h"
#include "Blaster/HUD/InvLeafText.h"
#include "Blaster/HUD/InvLeafLabeledValue.h"

void FInvAssimilateFragment::Assimilate(UInvCompositeBase* Composite) const
{
	if (!MatchesWidgetTag(Composite)) return;
	Composite->Expand();
}

void FInvImageFragment::Assimilate(UInvCompositeBase* Composite) const
{
	FInvAssimilateFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UInvLeafImage* Image = Cast<UInvLeafImage>(Composite);
	if (!IsValid(Image)) return;

	Image->SetImage(Icon);
	Image->SetBoxSize(IconDimensions);
	Image->SetImageSize(IconDimensions);
}

void FInvTextFragment::Assimilate(UInvCompositeBase* Composite) const
{
	FInvAssimilateFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UInvLeafText* LeafText = Cast<UInvLeafText>(Composite);
	if (!IsValid(LeafText)) return;

	LeafText->SetText(FragmentText);
}

void FInvLabeledNumberFragment::Assimilate(UInvCompositeBase* Composite) const
{
	FInvAssimilateFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite)) return;

	UInvLeafLabeledValue* LabeledValue = Cast<UInvLeafLabeledValue>(Composite);
	if (!IsValid(LabeledValue)) return;

	LabeledValue->SetText_Label(Text_Label, bCollapseLabel);

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = MinFractionalDigits;
	Options.MaximumFractionalDigits = MaxFractionalDigits;

	LabeledValue->SetText_Value(FText::AsNumber(Value, &Options), bCollapseValue);
}

void FInvLabeledNumberFragment::Manifest()
{
	FInvAssimilateFragment::Manifest();

	if (bRandomizeOnManifest)
	{
		Value = FMath::FRandRange(Min, Max);
	}
	bRandomizeOnManifest = false;
}

void FInvConsumableFragment::Assimilate(UInvCompositeBase* Composite) const
{
	FInvAssimilateFragment::Assimilate(Composite);
	for (const auto& Modifier : ConsumeModifiers)
	{
		const auto& ModRef = Modifier.Get();
		ModRef.Assimilate(Composite);
	}
}

void FInvConsumableFragment::Manifest()
{
	FInvAssimilateFragment::Manifest();
	for (auto& Modifier : ConsumeModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.Manifest();
	}
}

bool FInvAssimilateFragment::MatchesWidgetTag(const UInvCompositeBase* Composite) const
{
	return Composite->GetFragmentTag().MatchesTagExact(GetFragmentTag());
}
