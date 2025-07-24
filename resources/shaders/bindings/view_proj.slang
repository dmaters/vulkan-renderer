#pragma once


[[vk::binding(0, 0)]]
cbuffer ViewProj {
	float4x4 VIEW;
	float4x4 PROJECTION;
};