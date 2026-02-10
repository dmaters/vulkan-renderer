
const static uint SIZE = 512;

enum AXIS {
	Z,
	Y,
	X
};
constexpr void encodeAxis(uint position, AXIS axis, inout uint mortonCoord) {
	for (uint i = 0; i < 10; i++) {
		uint bitValue = (position & (1 << i));
		uint offset = axis + i * 3;
		mortonCoord = mortonCoord | (bitValue << offset);
	}
}

uint32_t encodePosition(uint3 position) {
	uint32_t value = 0;
	encodeAxis(position.z, AXIS::Z, value);
	encodeAxis(position.y, AXIS::Y, value);
	encodeAxis(position.x, AXIS::X, value);
	return value;
}
uint getAxisValue(AXIS axis, uint mortonValue) {
	uint value = 0;

	for (uint i = 0; i < 10; i++) {
		uint bitPosition = i * 3 + uint(axis);
		uint bitValue = (mortonValue >> bitPosition) & 1;
		value |= bitValue << i;
	}
	return value;
}

struct OctreeBuilder {
	RWStructuredBuffer<Atomic<uint32_t>> tree;

	__init(RWStructuredBuffer<Atomic<uint32_t>> treeData) { tree = treeData; }

	[mutating] void addLeaf(uint3 position) {
		uint32_t morton = encodePosition(position);

		uint level1Index = 1 << ((morton >> 24) & 0xF);
		tree[0] |= level1Index;

		uint level2Index = 1 << ((morton >> 16) & 0xF);
		tree[level1Index + 1] |= level2Index;

		uint level3Index = 1 << ((morton >> 8) & 0xF);
		tree[level2Index + 1 + 8] |= level3Index;

		uint level4Index = 1 << ((morton) & 0xF);
		tree[level3Index + 1 + 8 + 64] |= level4Index;
	}
}

struct Octree {
	StructuredBuffer<uint32_t> tree;
	float sceneSize;
	__init(StructuredBuffer<uint32_t> treeData, float sceneSize) {
		tree = treeData;
		sceneSize = sceneSize;
	}

	uint8_t query(uint3 position) {};

	uint8_t queryAxis(AXIS axis, uint8_t level) {}

	bool trace(float3 direction, float3 position) {};
