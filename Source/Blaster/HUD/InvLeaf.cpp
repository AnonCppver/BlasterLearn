#include "InvLeaf.h"

void UInvLeaf::ApplyFunction(FuncType Function)
{
	Function(this);
}