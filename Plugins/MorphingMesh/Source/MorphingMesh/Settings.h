#pragma once

inline constexpr std::array<int, 3> NumVoxelsTable = {64, 32, 16};

enum EVoxelResolution : uint8
{
	High = 0,
	Middle = 1,
	Low = 2
};