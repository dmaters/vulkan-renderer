#pragma once
#include <iostream>
#define CHECK_ERROR(res, msg)                 \
	if (res.result != vk::Result::eSuccess) { \
		std::cerr << msg << std::endl;        \
		throw res.result;                     \
	}
