#include "../core/bruteforce.h"
#include "../cubiomes/finders.h"
#include "../reversal.h"
#include <stdint.h>
#include <stdlib.h>

#define ITERATE_ON_CHUNK_SEEDS true
#define DEBUG false

const uint64_t GLOBAL_START_INTEGER = 0;
#if ITERATE_ON_CHUNK_SEEDS
    const uint64_t GLOBAL_NUMBER_OF_INTEGERS = ((UINT64_C(1) << (64 - 25)) + ((UINT64_C(1) << (64 - 25)) % 3))/3 - GLOBAL_START_INTEGER + (!!GLOBAL_START_INTEGER);
#else
    const uint64_t GLOBAL_NUMBER_OF_INTEGERS = CHECK_THIS_INTEGER_AND_FOLLOWING(GLOBAL_START_INTEGER, 39);
#endif
// const uint64_t GLOBAL_NUMBER_OF_INTEGERS = 1000000000;
const int GLOBAL_NUMBER_OF_WORKERS = 7;
const char *INPUT_FILEPATH = NULL;
const char *OUTPUT_FILEPATH = NULL;
// const bool TIME_PROGRAM = false;
const bool TIME_PROGRAM = true;
const int VERSION = MC_B1_8;
const bool LARGE_BIOMES_FLAG = false;

enum BetaBiomes {
    DESERT, FOREST, MOUNTAINS, SWAMP, PLAINS, TAIGA
};

// COMMON_STRUCT(BiomeData) {
STRUCT(BiomeData) {
    Pos coordinate;
    enum BetaBiomes nextIntResult;
};

// Worldseed: 8675309
// BiomeData BIOME_DATA[] = {
//     {{   0/256,    0/256}, PLAINS},
//     {{-256/256, -256/256}, DESERT},
//     {{ 256/256,  256/256}, PLAINS},
//     {{   0/256,  256/256}, MOUNTAINS},
//     {{-512/256, -512/256}, MOUNTAINS},
//     {{ 512/256, -256/256}, DESERT},
//     {{ 512/256,    0/256}, MOUNTAINS},
//     {{ 512/256,  256/256}, PLAINS},
//     {{ 512/256,  512/256}, DESERT},
//     {{ 256/256,  512/256}, TAIGA},
//     {{   0/256,  512/256}, DESERT},
//     {{-768/256, -256/256}, SWAMP},
//     {{-768/256, -512/256}, PLAINS},
//     {{-768/256, -768/256}, MOUNTAINS},
//     {{-256/256, -768/256}, FOREST},
//     {{ 512/256, -768/256}, PLAINS},
//     {{ 768/256, -768/256}, FOREST},
//     {{ 768/256, -256/256}, SWAMP},
//     {{ 768/256,    0/256}, TAIGA},
//     {{ 768/256,  256/256}, PLAINS},
//     {{ 768/256,  512/256}, FOREST},
//     {{ 768/256,  768/256}, DESERT},
//     {{ 512/256,  768/256}, TAIGA},
//     {{ 256/256,  768/256}, DESERT},
//     {{   0/256,  768/256}, TAIGA},
// };

// Worldseed: 1111111
BiomeData BIOME_DATA[] = {
    // 0
    {{   0/256,    0/256}, TAIGA},
    // 256
    {{-256/256, -256/256}, TAIGA},
    {{   0/256, -256/256}, MOUNTAINS},
    { {256/256, -256/256}, PLAINS},
    {{ 256/256,    0/256}, PLAINS},
    {{ 256/256,  256/256}, PLAINS},
    // 512
    {{-512/256, -512/256}, DESERT},
    {{-256/256, -512/256}, MOUNTAINS},
    {{   0/256, -512/256}, PLAINS},
    {{ 256/256, -512/256}, SWAMP},
    {{ 512/256, -512/256}, MOUNTAINS},
    {{ 512/256, -256/256}, PLAINS},
    {{ 512/256,    0/256}, DESERT},
    {{ 512/256,  256/256}, PLAINS},
    {{ 512/256,  512/256}, SWAMP},
    {{   0/256,  512/256}, TAIGA},
    {{-256/256,  512/256}, DESERT},
    {{-512/256,  512/256}, PLAINS},
    {{-512/256, -256/256}, DESERT},
    // 768
    {{-768/256, -768/256}, TAIGA},
    {{-512/256, -768/256}, FOREST},
    {{-256/256, -768/256}, FOREST},
    {{   0/256, -768/256}, MOUNTAINS},
    {{ 256/256, -768/256}, PLAINS},
    {{ 512/256, -768/256}, MOUNTAINS},
    {{ 768/256, -768/256}, FOREST},
    {{ 768/256, -512/256}, DESERT},
};

DEFAULT_LOCALS_INITIALIZATION

// The program involves first filtering the possible lowest 25 bits of either the first datapoint's "chunk seed" (internal state from which it makes the biome decision) if ITERATE_ON_CHUNK_SEEDS is true, or the world's "start seed" (shared internal state that all chunks derive their chunk seeds from) if not. We store either using an array of lowest-25-bits entries.
uint32_t *lowest25BitsArray;
size_t lowest25BitsArraySize;
// The capacity of the array.
// One bit is removed because only one parity is possible.
// If chunk seeds are iterated over, one bit is removed because the uppermost bit is known from the nextInt call.
// COMMON_CONSTEXPR_VARIABLE size_t LOWEST_25_BITS_ARRAY_CAPACITY = UINT64_C(1) << (25 - 1 - ITERATE_ON_CHUNK_SEEDS);
const size_t LOWEST_25_BITS_ARRAY_CAPACITY = UINT64_C(1) << (25 - 1 - ITERATE_ON_CHUNK_SEEDS);

// COMMON_CONSTEXPR_FUNCTION int64_t signExtend39(int64_t x) {
//     const int64_t BITMAP = UINT64_C(1) << 38;
//     return (x ^ BITMAP) - BITMAP;
// }

// Biomes will be placed depending on the result of nextInt(6), which can only return an even value iff a nextInt(2) would have returned 0 (and vice versa for odd values).
// Therefore this runs the biome checks if the start seed was only 25 bits, and the check was only nextInt(2). Only lowest-25-bits passing all checks could be the lowest 25 bits of the actual world's start seed.
// COMMON_NODISCARD bool testLowest25Bits(uint32_t startSeedLowest25Bits) {
bool testLowest25Bits(uint32_t startSeedLowest25Bits) {
    for (size_t i = ITERATE_ON_CHUNK_SEEDS; i < sizeof(BIOME_DATA)/sizeof(*BIOME_DATA); ++i) {
        BiomeData currentBiomeData = BIOME_DATA[i];
        if (((getChunkSeed(startSeedLowest25Bits, currentBiomeData.coordinate.x, currentBiomeData.coordinate.z) >> 24) & 1) != (currentBiomeData.nextIntResult & 1)) return false;
    }
    return true;
}

// Tests all biomes to see if their 
// COMMON_NODISCARD bool testAllBits(uint64_t startSeed) {
bool testAllBits(uint64_t startSeed) {
    for (size_t i = ITERATE_ON_CHUNK_SEEDS; i < sizeof(BIOME_DATA)/sizeof(*BIOME_DATA); ++i) {
        BiomeData currentBiomeData = BIOME_DATA[i];
        // #if DEBUG
        //     if (startSeed == 5833167203422234280) {
        //         printf("%zd, %" PRIu64 ", %d\n", i, (getChunkSeed(startSeed, currentBiomeData.coordinate.x, currentBiomeData.coordinate.z) >> 24), currentBiomeData.nextIntResult & 1);
        //     }
        // #endif
        // if (mcFirstInt(getChunkSeed(startSeed, currentBiomeData.coordinate.x, currentBiomeData.coordinate.z), 6) != COMMON_STATIC_CAST(int, currentBiomeData.nextIntResult)) return false;
        if (mcFirstInt(getChunkSeed(startSeed, currentBiomeData.coordinate.x, currentBiomeData.coordinate.z), 6) != (int)(currentBiomeData.nextIntResult)) return false;
    }
    return true;
}

void initializeGlobals() {
    // #if DEBUG
    //     // int64_t correctWorldseed = 8675309;
    //     int64_t correctWorldseed = 1111111;
    //     int64_t correctLayerSalt = getLayerSalt(200);
    //     int64_t correctStartSeed = COMMON_STATIC_CAST(int64_t, getStartSeed(correctWorldseed, correctLayerSalt));
    //     int64_t correctChunkSeed = COMMON_STATIC_CAST(int64_t, getChunkSeed(correctStartSeed, 0, 0));
    //     int64_t correctStartSeedUpper39Bits = correctStartSeed >> 25;
    //     int64_t correctStartSeedLower25Bits = correctStartSeed & ((INT64_C(1) << 25) - 1);
    //     int64_t correctChunkSeedUpper39Bits = correctChunkSeed >> 25;
    //     int64_t correctChunkSeedLower25Bits = correctChunkSeed & ((INT64_C(1) << 25) - 1);
    //     printf("Correct layer salt: %" PRId64 "\n", correctLayerSalt);
    //     printf("Correct start seed: %" PRId64 " = %" PRId64 " | %" PRIu64 ", index %" PRId64 "\n", correctStartSeed, correctStartSeedUpper39Bits, correctStartSeedLower25Bits, (correctStartSeedUpper39Bits & ((INT64_C(1) << 39) - 1))/3);
    //     printf("Correct chunk seed: %" PRId64 " = %" PRId64 " | %" PRIu64 ", index %" PRId64 "\n", correctChunkSeed, correctChunkSeedUpper39Bits, correctChunkSeedLower25Bits, (correctChunkSeedUpper39Bits & ((INT64_C(1) << 39) - 1))/3);
    // #endif


    // lowest25BitsArray = COMMON_REINTERPRET_CAST(uint32_t *, calloc(LOWEST_25_BITS_ARRAY_CAPACITY, sizeof(*lowest25BitsArray)));
    lowest25BitsArray = (uint32_t *)calloc(LOWEST_25_BITS_ARRAY_CAPACITY, sizeof(*lowest25BitsArray));
    lowest25BitsArraySize = 0;
    #if ITERATE_ON_CHUNK_SEEDS
        // Lowest 25 bits must match z-coordinate's parity
        for (uint32_t chunkSeedLowest25Bits = ((BIOME_DATA[0].nextIntResult & 1) << 24) + (BIOME_DATA[0].coordinate.z & 1); chunkSeedLowest25Bits < UINT32_C(1) << 25 && lowest25BitsArraySize < LOWEST_25_BITS_ARRAY_CAPACITY; chunkSeedLowest25Bits += 2) {
            // uint32_t startSeedLowest25Bits = COMMON_STATIC_CAST(uint32_t, chunkSeedToStartSeed(chunkSeedLowest25Bits, BIOME_DATA[0].coordinate.x, BIOME_DATA[0].coordinate.z));
            uint32_t startSeedLowest25Bits = (uint32_t)chunkSeedToStartSeed(chunkSeedLowest25Bits, BIOME_DATA[0].coordinate.x, BIOME_DATA[0].coordinate.z);
            if (!testLowest25Bits(startSeedLowest25Bits)) continue;
            lowest25BitsArray[lowest25BitsArraySize] = chunkSeedLowest25Bits;
            ++lowest25BitsArraySize;
        }
    #else
        // Lowest 25 bits must be even
        for (uint32_t startSeedLowest25Bits = 0; startSeedLowest25Bits < UINT32_C(1) << 25 && lowest25BitsArraySize < LOWEST_25_BITS_ARRAY_CAPACITY; startSeedLowest25Bits += 2) {
            if (!testLowest25Bits(startSeedLowest25Bits)) continue;
            lowest25BitsArray[lowest25BitsArraySize] = startSeedLowest25Bits;
            ++lowest25BitsArraySize;
        }
    #endif
    // #if DEBUG
    //     for (size_t i = 0; i < lowest25BitsArraySize; ++i) printf("%" PRIu32 "\n", lowest25BitsArray[i]);
    // #endif
    if (!lowest25BitsArraySize) {
        outputString("[No worlds can generate matching your input data.]\n");
        return;
    }
    else if (lowest25BitsArraySize > 1) outputString("[Warning: A %zdx slowdown exists due to not enough coordinates being in your input data.]\n", lowest25BitsArraySize);
}

void runWorker(void *workerIndex) {
    Generator g;
    setupGenerator(&g, VERSION, LARGE_BIOMES_FLAG);
    const Layer *const biomeLayer = &g.ls.layers[L_BIOME_256];
    const uint64_t biomeLayerSalt = biomeLayer->layerSalt;

    uint64_t currentIndex;
    for (size_t i = 0; i < lowest25BitsArraySize; ++i) {
        uint32_t currentLowest25Bits = lowest25BitsArray[i];
        if (!getNextInteger(workerIndex, &currentIndex)) return;
        do {
            #if ITERATE_ON_CHUNK_SEEDS
                /* The upper 39 bits must be congruent to (BIOME_DATA[0].nextIntResult >> 1) modulo (6 >> 1) 
                = 3. If the upper 39 bits are less than 2^38, that is simply achieved by putting currentIndex 
                into the form 3x+(BIOME_DATA[0].nextIntResult >> 1).*/
                uint64_t upper39Bits = currentIndex*3 + (BIOME_DATA[0].nextIntResult >> 1);
                /* If they are greater than or equal to 2^38, however, the actual corresponding 64-bit chunkseed would be negative, and the corresponding negative upper 39 bits formed by 3*currentIndex won't actually be a multiple of 3. Instead, they will have remainder (-2^63 mod 3), so we simply subtract [the positive equivalent of] that out so the number is congruent to (BIOME_DATA[0].nextIntResult >> 1) mod 3 again.*/
                upper39Bits -= (upper39Bits >= UINT64_C(1) << 38)*((INT64_MIN % 3) + 3);
                // Reconstruct full chunkseed, and reverse back to corresponding startseed
                // uint64_t chunkSeed = (upper39Bits << 25) | COMMON_STATIC_CAST(uint64_t, currentLowest25Bits);
                uint64_t chunkSeed = (upper39Bits << 25) | (uint64_t)(currentLowest25Bits);
                // #if DEBUG
                //     if (currentIndex == GLOBAL_START_INTEGER) printf("!, %" PRId64 ", %d, %" PRId64 ", %" PRId64 "\n", signExtend39(currentIndex*3), BIOME_DATA[0].nextIntResult >> 1, upper39Bits, chunkSeed);
                // #endif
                uint64_t startSeed = chunkSeedToStartSeed(chunkSeed, BIOME_DATA[0].coordinate.x, BIOME_DATA[0].coordinate.z);
            #else
                uint64_t upper39Bits = currentIndex;
                if (upper39Bits >= UINT64_C(1) << (64 - 25)) break;
                // uint64_t startSeed = (upper39Bits << 25) + COMMON_STATIC_CAST(uint64_t, currentLowest25Bits);
                uint64_t startSeed = (upper39Bits << 25) | (uint64_t)(currentLowest25Bits);
            #endif
                
            // #if DEBUG
            //     if (startSeed == 5833167203422234280) {
            //         printf("This is the right one!");
            //     }
            // #endif

            // Test if the current startseed matches all specified biomes
            if (!testAllBits(startSeed)) continue;

            // If successful, recover and print corresponding worldseeds
            uint64_t evenWorldseed = startSeedToWorldseed(startSeed, biomeLayerSalt, false);
            uint64_t oddWorldseed = getShadow(evenWorldseed);
            outputString("%" PRId64 " or %" PRId64 "\n", evenWorldseed, oddWorldseed);
        } while (getNextInteger(NULL, &currentIndex));
    }
}