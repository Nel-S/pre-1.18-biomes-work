#include <stdbool.h>
#include <stdint.h>

// Returns whichever of the two possible original states for mcStepSeed has the same parity as the previous salt in the sequence. In the middle of sequential advancements, that is the only possible internal state.
// For the very *first* mcStepSeed in the sequence, a second solution also exists that can be obtained via `possibility2 = -7379792620528906219LL - possibility1` (or `getShadow()` in Cubiomes).
// WARNING: Output and Salt *must* have the same parity for this to work; otherwise no possible original states can exist.
uint64_t reverseMcStepSeed(uint64_t output, uint64_t salt, uint64_t previousSalt);

// Returns whichever "start seed" could have generated the specified "chunk seed" at the specified coordinates.
// The start seed will always be even.
// WARNING: chunkSeed and z *must* have the same parity for this to work; otherwise no possible original start seeds can exist.
uint64_t chunkSeedToStartSeed(uint64_t chunkSeed, int32_t tileX, int32_t tileZ);

// Returns whichever "start salt" could have generated the specified "start seed", given a layer's salt.
// The start salt will always have the same parity as the layer salt.
// WARNING: startSeed *must* be even for this to work; otherwise no possible original start salts can exist.
uint64_t startSeedToStartSalt(uint64_t startSeed, uint64_t layerSalt);

// Returns one of two worldseeds that could have the specified "start salt", given a layer's salt.
// The two possible worldseeds have different parities and can be switched between via `returnOddWorldseed` (or can be converted between each other using `getShadow()` in Cubiomes).
// WARNING: startSalt and layerSalt *must* have the same parity for this to work; otherwise no possible original worldseeds can exist.
uint64_t startSaltToWorldseed(uint64_t startSalt, uint64_t layerSalt, bool returnOddWorldseed);

// Returns one of two worldseeds that could have the specified "start seed", given a layer's salt.
// The two possible worldseeds have different parities and can be switched between via `returnOddWorldseed` (or can be converted between each other using `getShadow()` in Cubiomes).
// WARNING: startSeed *must* be even for this to work; otherwise no possible original worldseeds can exist.
uint64_t startSeedToWorldseed(uint64_t startSeed, uint64_t layerSalt, bool returnOddWorldseed);

// Returns one of two worldseeds that could have the specified "chunk seed", given the tile coordinates and the layer's salt.
// The two possible worldseeds have different parities and can be switched between via `returnOddWorldseed` (or can be converted between each other using `getShadow()` in Cubiomes).
// WARNING: startSeed *must* be even for this to work; otherwise no possible original worldseeds can exist.
uint64_t chunkSeedToWorldseed(uint64_t chunkSeed, int32_t tileX, int32_t tileZ, uint64_t layerSalt, bool returnOddWorldseed);