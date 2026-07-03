#pragma once
#include "../BuildContext.hpp"
#include "../Task.hpp"
struct SceneData {
	static void Setup(Task::SetupContext& context);
	static void Build(Task::BuildContext& context);
};
