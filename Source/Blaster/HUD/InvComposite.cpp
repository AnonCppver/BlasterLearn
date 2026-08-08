#include "InvComposite.h"
#include "Blueprint/WidgetTree.h"

void UInvComposite::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (UInvCompositeBase* Composite = Cast<UInvCompositeBase>(Widget); IsValid(Composite))
			{
				Children.Add(Composite);
				Composite->Collapse();
			}
		});
}

void UInvComposite::ApplyFunction(FuncType Function)
{
	for (auto& Child : Children)
	{
		Child->ApplyFunction(Function);
	}
}

void UInvComposite::Collapse()
{
	for (auto& Child : Children)
	{
		Child->Collapse();
	}
}