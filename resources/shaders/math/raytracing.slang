module raytracing;

public float3 unpackBarycentrics(float2 bary) { return float3(1 - bary.x - bary.y, bary.x, bary.y); }
