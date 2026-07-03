#pragma once
#include "../Task.hpp"

struct BufferUpload {
	rendergraph::ResourceIndex origin;
	rendergraph::ResourceIndex destination;

	static void build(Task::BuildContext& context, TaskIndex task);
};
