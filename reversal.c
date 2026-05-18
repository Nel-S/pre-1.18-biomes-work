#include "reversal.h"

static const uint64_t A = UINT64_C(6364136223846793005);
static const uint64_t B = UINT64_C(1442695040888963407);

// Solves Ax^2 + Bx + C = 0 (mod 2^64).
// Contributed by Andrew (https://github.com/Gaider10/)
static inline uint64_t rev_quad(uint64_t a, uint64_t b, uint64_t c, bool oddSolution) {
	uint64_t x = oddSolution;
	uint64_t df_inv = 1;
	for (int i = 1; i < 64; i *= 2) {
		uint64_t f = a * x * x + b * x + c;
		uint64_t df = 2 * a * x + b;
		df_inv += df_inv - df * df_inv * df_inv;
		uint64_t dx = -f * df_inv;
		x += dx;
	}
	return x;
}

// Returns the even solution for Ax^2 + Bx + C = 0 (mod 2^n) when A and B are odd, and C is even. (If C is also odd, no solution exists.)
// TODO: Are any optimizations possible since A and B are fixed?
static inline uint64_t getEvenSolution(uint64_t oddA, uint64_t oddB, uint64_t evenC) {
	return rev_quad(oddA, oddB, evenC, false);
}

// Returns the odd solution for Ax^2 + Bx + C = 0 (mod 2^n) when A and B are odd, and C is even. (If C is also odd, no solution exists.)
// TODO: Are any optimizations possible since A and B are fixed?
static inline uint64_t getOddSolution(uint64_t oddA, uint64_t oddB, uint64_t evenC) {
	return rev_quad(oddA, oddB, evenC, true);
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