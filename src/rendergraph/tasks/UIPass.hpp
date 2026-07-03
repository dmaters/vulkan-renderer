
#pragma once

#include "rendergraph/BuildContext.hpp"
#include "rendergraph/Task.hpp"
#include "ui/UI.hpp"

void UIPass(Task::BuildContext& context) { UI::Render(context.commandBuffer); }
