module raytracing;
namespace raytracing{

public struct RayQueryResult {
    public uint primitiveID;
    public uint baseIndex;
    public float3 barycentrics;
};
public
Optional<RayQueryResult> RayQuery(float3 origin, float3 direction, float rayLength, RaytracingAccelerationStructure as){
   	RayQuery<RAY_FLAG_NONE> query;
	RayDesc desc;
	desc.Direction = direction;
	desc.Origin = origin;
	desc.TMin = 0;
	desc.TMax = rayLength;
	query.TraceRayInline(as, RAY_FLAG_NONE, 0xFF, desc);
	while (query.Proceed());
	if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT) return none;
	RayQueryResult res;


	res.primitiveID = query.CommittedInstanceID();
	res.baseIndex = query.CommittedPrimitiveIndex() * 3;
	float2 bary = query.CommittedTriangleBarycentrics();
	res.barycentrics = float3(1.0 - bary.x - bary.y, bary.x, bary.y);

	return res;

}


public
void TraceRay<T>(float3 origin, float3 direction, float rayLength, RaytracingAccelerationStructure accellerationStructure, T data){
    RayDesc ray;
	ray.Origin = origin.xyz;
	ray.Direction = direction;
	ray.TMin = 0.01;
	ray.TMax = rayLength;


	TraceRay<T>(
	    accellerationStructure,
		RAY_FLAG_NONE,
		0xFF,
		0,
		0,
		0,
		ray,
		data
	);
}

public
float3 UnpackBarycentrics(float2 bary) {
    return float3(1 - bary.x - bary.y, bary.x, bary.y);
}

};
