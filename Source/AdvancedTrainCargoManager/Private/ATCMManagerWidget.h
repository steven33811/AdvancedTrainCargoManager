#pragma once

#include "Delegates/Delegate.h"
#include "Widgets/SWidget.h"

TSharedRef<SWidget> CreateATCMManagerWidget(
	FSimpleDelegate OnRequestClose);