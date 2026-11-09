module random;
namespace random {
public
struct Random {
private
	uint32_t _state;

private
	[mutating] uint32_t xorshift32() {
		// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
		uint32_t x = _state;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		_state = x;
		return x;
	};

public
	__init(uint32_t seed = 0x12345678u) {
		_state = seed;
		xorshift32();
	};

public
	[mutating] uint32_t uint32() { return xorshift32(); };

public
	[mutating] float float() { return (float)(xorshift32() >> 8) / 0x1000000; };
}
}  // namespace random
