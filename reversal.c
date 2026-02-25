#include "reversal.h"

static const uint64_t A = UINT64_C(6364136223846793005);
static const uint64_t B = UINT64_C(1442695040888963407);

// Solves Ax^2 + Bx + C = 0 (mod 2^n) when A is even and B is odd.
// Algorithm from S. M. Dehnavi et al (https://doi.org/10.7546/nntdm.2019.25.1.75-83).
static inline uint64_t solveWithEvenA(uint64_t evenA, uint64_t oddB, uint64_t c, size_t n) {
	uint64_t output = 0;
	for (size_t i = 0; i < n; ++i) {
		if (c & 1) {
			output |= UINT64_C(1) << i;
			c = evenA/2 + oddB/2 + c/2 + 1;
			oddB = 2*evenA + oddB;
		} else c /= 2;
		evenA *= 2;
	}
	return output;
}

// Returns the even solution for Ax^2 + Bx + C = 0 (mod 2^n) when A and B are odd, and C is even. (If C is also odd, no solution exists.)
// Algorithm from S. M. Dehnavi et al (https://doi.org/10.7546/nntdm.2019.25.1.75-83).
// TODO: Are any optimizations possible since A and B are fixed?
static inline uint64_t getEvenSolution(uint64_t oddA, uint64_t oddB, uint64_t evenC) {
	return 2*solveWithEvenA(2*oddA, oddB, evenC/2, 63);
}

// Returns the odd solution for Ax^2 + Bx + C = 0 (mod 2^n) when A and B are odd, and C is even. (If C is also odd, no solution exists.)
// Algorithm from S. M. Dehnavi et al (https://doi.org/10.7546/nntdm.2019.25.1.75-83).
// TODO: Are any optimizations possible since A and B are fixed?
static inline uint64_t getOddSolution(uint64_t oddA, uint64_t oddB, uint64_t evenC) {
	return 2*solveWithEvenA(2*oddA, 2*oddA + oddB, (oddA + oddB + evenC)/2, 63) + 1;
}



uint64_t reverseMcStepSeed(uint64_t output, uint64_t salt, uint64_t previousSalt) {
	return (previousSalt & 1 ? getOddSolution : getEvenSolution)(A, B, salt - output);
}

uint64_t chunkSeedToStartSeed(uint64_t chunkSeed, int32_t tileX, int32_t tileZ) {
	uint64_t interimState = reverseMcStepSeed(chunkSeed, tileZ, tileX);
    interimState = reverseMcStepSeed(interimState, tileX, tileZ);
    return reverseMcStepSeed(interimState, tileZ, tileX) - tileX;
}

uint64_t startSeedToStartSalt(uint64_t startSeed, uint64_t layerSalt) {
	return reverseMcStepSeed(startSeed, 0, layerSalt);
}

uint64_t startSaltToWorldseed(uint64_t startSalt, uint64_t layerSalt, bool returnOddWorldseed) {
	uint64_t interimState = reverseMcStepSeed(startSalt, layerSalt, layerSalt);
    interimState = reverseMcStepSeed(interimState, layerSalt, layerSalt);
    return reverseMcStepSeed(interimState, layerSalt, returnOddWorldseed);
}

uint64_t startSeedToWorldseed(uint64_t startSeed, uint64_t layerSalt, bool returnOddWorldseed) {
	uint64_t startSalt = startSeedToStartSalt(startSeed, layerSalt);
	return startSaltToWorldseed(startSalt, layerSalt, returnOddWorldseed);
}

uint64_t chunkSeedToWorldseed(uint64_t chunkSeed, int32_t tileX, int32_t tileZ, uint64_t layerSalt, bool returnOddWorldseed) {
	uint64_t startSeed = chunkSeedToStartSeed(chunkSeed, tileX, tileZ);
	return startSeedToWorldseed(startSeed, layerSalt, returnOddWorldseed);
}