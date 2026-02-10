
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

	uint8_t query(uint3 position) {
		uint32_t morton = encodePosition(position);

		uint8_t hit = 0;
		uint level1Index = 1 << ((morton >> 24) & 0xF);
		hit |= uint8_t(tree[0] & (1 << level1Index));

		uint level2Index = 1 << ((morton >> 16) & 0xF);
		hit |= uint8_t(tree[level1Index + 1] & (1 << level2Index));

		uint level3Index = 1 << ((morton >> 8) & 0xF);
		hit |= uint8_t(tree[level2Index + 1 + 8] & (1 << level3Index));

		uint level4Index = 1 << ((morton) & 0xF);
		hit |= uint8_t(tree[0] & (1 << level1Index));
	};

	uint8_t queryAxis(AXIS axis, uint8_t level) {}
	// Con morton puoi fare il query veloce se un octante è pieno (ogni 3
	// bit c'è un valore successivo)
	// Con i 3 valori (xyz) devi proseguire fino a quando tutti e 3 non
	// hanno come distanza minima (moltiplicatore di direzione) lo stesso
	// valore
	// Scorri per i byte in base alla direzione
	//
	//
	// COntrolli, per asse con mask i valori
	// Ottieni se prendi l'elemento 1 o 2
	// Fai la somma delle maschere, ottieni i valori validi
	// Con i valori scendi di livello e testa la stessa cosa

	float2 minMaxSlabDistances(float axisPos, float axisScale, float slabSize) {
		float slabTo0 = slabSize / axisScale;
		float posTo0 = axisPos / axisScale;

		float2 t = { posTo0 - slabTo0, posTo0 + slabTo0 };
	}
	uint8_t getMask(float adjustedPosition, float octantSize, AXIS axis) {
		int suboctantCount = int(ceil(adjustedPosition / octantSize));
	}

	typealias Sector = uint8_t;
	bool trace(float3 direction, float3 position) {
		// 1 (z)xy -> loop -> found
		// 2 (z)xy -> loop -> found
		float3 baseMaskSize = direction.zzx / direction.xyz;
		float3 maskSize = baseMaskSize * 0.5;

		int suboctantCountX = int(ceil(position.x / maskSize.x));
		int suboctantCountY = int(ceil(position.y / maskSize.y));
		int suboctantCountZ = int(ceil(position.z / maskSize.z));

		int maskBase =
			max(suboctantCountZ, max(suboctantCountX, suboctantCountY));

		uint8_t maskX = uint8_t(0xFF << (suboctantCountX - maskBase) * 4);
		int yCount = (suboctantCountY - maskBase);
		uint8_t maskY = uint8_t(0xFF << yCount * 2 - (yCount & 1));
		uint8_t maskZ = uint8_t(0xFF << (suboctantCountZ - maskBase) * 2);

		uint8_t maskedValue = tree[0] & maskX & maskY & maskZ;

		if (maskedValue == 0) return false;
	};
