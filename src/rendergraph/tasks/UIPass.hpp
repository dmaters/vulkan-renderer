
#pragma once

#include "rendergraph/tasks/TaskContext.hpp"
#include "ui/UI.hpp"

void UIPass(TaskContext& context) { UI::Render(context.commandBuffer); }
