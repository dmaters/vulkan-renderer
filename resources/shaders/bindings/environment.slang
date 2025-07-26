#pragma once

[vk::binding(1,0)] cbuffer EnvironmentData {
    float3 environment_color;
    float scene_size;
} uEnvironment;