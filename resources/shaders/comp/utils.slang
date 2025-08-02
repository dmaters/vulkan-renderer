#pragma once

    constexpr float4x4 viewInv(float4x4 view) {
        
        
        float4x4 inv = view;
        float3 position = view[3].xyz;
        view[3] = float4(0);
        view = transpose(view);
        view[3] = float4(mul(float3x3(view),position),1);

        return view;
    };

    constexpr float4x4 orthoProjectionMatrix(float left, float right, float bottom,float top, float near, float far) {
        return float4x4(
            2/(right - left), 0,0,- (right + left) / (right - left),
            0,2/(top-bottom),0,- (top + bottom) / (top - bottom), 
            0,0,-1/(far - near), - near/(far - near),
            0,0,0,1
        );
    };
    
    constexpr float4x4 orthoProjectionMatrix_simple(float frustumSize) {
        return float4x4(
            1/(frustumSize),0,0,0,
            0,1/frustumSize,0,0,
            0,0,-1/(frustumSize), 0,
            0,0,0,1,
        );
    }
    
    

