module samplers;

public enum SamplerType {
	LinearClamp,
	LinearRepeat,
	NearestClamp,
	NearestRepeat
};


public [vk_binding(1, 1)] SamplerState[4] _samplers;
