#pragma once

#include "CoreMinimal.h"

// -------------------------
// Simple RNG (LCG)
// - deterministic
// - fast
// - NOT crypto-secure
// -------------------------

class FRandom32
{
public:
	explicit FRandom32(uint32 seed = 0x12345678u) : state_(seed ? seed : 0x12345678u) {}
    
	uint32 NextU32()
	{
		state_ = 1664525u * state_ + 1013904223u;
		return state_;
	}
    
	uint32 NextU32(uint32 maxExclusive)
	{
		if (maxExclusive == 0) return 0;
		return NextU32() % maxExclusive;
	}
    
	int32 NextI32(int32 minInclusive, int32 maxInclusive)
	{
		if (minInclusive > maxInclusive) return minInclusive;
		const uint32 span = static_cast<uint32>(maxInclusive - minInclusive + 1);
		return minInclusive + static_cast<int32>(NextU32(span));
	}
    
	double Next01()
	{
		// 24비트 정밀도의 [0.0, 1.0) 실수 반환
		return static_cast<double>(NextU32() & 0x00FFFFFFu) / static_cast<double>(0x01000000u);
	}
    
	float NextFloat01()
	{
		return static_cast<float>((NextU32() >> 8) * (1.0 / 16777216.0));
	}
    
	bool Chance(double p)
	{
		if (p <= 0.0) return false;
		if (p >= 1.0) return true;
		return Next01() < p;
	}
    
private:
	uint32 state_;
    
};