module random;

public struct Random {
    private uint32_t _state;
    private [mutating] uint32_t xorshift32() {
    		// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
    		uint32_t x = _state;
    		x ^= x << 13;
    		x ^= x >> 17;
    		x ^= x << 5;
    		_state = x;
    		return x;
    	};

    __init(uint32_t seed = 0x12345678u) {
    		_state = seed;
    		xorshift32();
    	};

    [mutating] uint32_t uint32() { return xorshift32(); };

    [mutating] float float() { return (float)(xorshift32() >> 8) / 0x1000000; };
}

