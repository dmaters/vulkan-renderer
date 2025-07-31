#pragma once



    constexpr float4x4 orthoProjectionMatrix(float top, float bottom, float left, float right, float depth) {
        return float4x4(
            2/(right - left), 0,0,- (right + left) / (right - left),
            0,2/(top-bottom),0,- (top + bottom) / (top - bottom),
            0,0,-1/depth,0,
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
    
    

