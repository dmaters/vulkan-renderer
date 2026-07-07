#pragma once
#include "../BuildContext.hpp"
#include "../Task.hpp"
struct SceneData {
	static Task::Dependencies Setup(Task::SetupContext& context);
	static void Build(Task::BuildContext& context);

	enum Slot {
		Camera = 0,
		Lights = 1,
	};
};
