#include "../core/bruteforce.h"
#include "../cubiomes/finders.h"
#include "../reversal.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <utility>

#define ITERATE_ON_CHUNK_SEEDS false

const uint64_t GLOBAL_START_INTEGER = 0;
const uint64_t GLOBAL_NUMBER_OF_INTEGERS = UINT64_MAX/((UINT64_C(1) << 25)) - GLOBAL_START_INTEGER + (!!GLOBAL_START_INTEGER);
// const uint64_t GLOBAL_NUMBER_OF_INTEGERS = 7;
const int GLOBAL_NUMBER_OF_WORKERS = 7;
const char *INPUT_FILEPATH = NULL;
const char *OUTPUT_FILEPATH = NULL;
const bool TIME_PROGRAM = false;

const int VERSION = MC_B1_8;
const bool LARGE_BIOMES_FLAG = false;
const Pos CENTER_TILE = {0, 0};
const Pos INITIAL_TILE_DIMENSIONS = {13, 13};
const bool UPDATE_THRESHOLD = true;

DEFAULT_LOCALS_INITIALIZATION

// Threshold variable + mutex for it
volatile Pos tileDimensions;
std::mutex tileDimensionsMutex;

// The program involves ranking the possible lowest 25 bits of the starting coordinate's "chunk seed" (internal state from which it makes the oceans decision). We store that using an array of {tile count, lowest 25 bits} entries.
std::pair<uint64_t, uint32_t> *lowest25BitsArray;
// The size of the array.
// One bit is removed because only one parity is possible.
const size_t LOWEST_25_BITS_ARRAY_SIZE = UINT64_C(1) << (25 - 1);

// Utilities for the pair entries.
[[nodiscard]] bool sortPairsAscending(const std::pair<int, uint32_t>& first, const std::pair<int, uint32_t>& second) {
    return first.first < second.first;
}

struct pairHash {
    [[nodiscard]] std::size_t operator()(std::pair<int, int> const &v) const {
        // return ((v.first + v.second) * (v.first + v.second + 1) / 2) + v.second;
        return (v.second << 16) ^ v.first;
    }
};

// The 1:N scale that the world exists at when the ocean tiles are placed. 
const int TILE_SCALE = 4096 * (1 + (VERSION == MC_B1_8) + 3*(LARGE_BIOMES_FLAG && VERSION >= MC_1_3));
const int TILE_TO_SAMPLING = static_cast<int>(log2(TILE_SCALE) - log2(256));

// Ocean will be placed if nextInt(10) != 0, which is guaranteed if a hypothetical nextInt(2) returned 0, which only happens if the 25th bit is 1.
// Therefore this spirals outwards to find the ocean count if the start seed was only 25 bits, and the check was only nextInt(2) != 0. This creates an upper bound on the actual possible # of ocean tiles.
[[nodiscard]] uint64_t testLowest25Bits(uint32_t startSeedLowest25Bits) {
    const uint64_t ITERATIONS = static_cast<uint64_t>(INITIAL_TILE_DIMENSIONS.x) * INITIAL_TILE_DIMENSIONS.z;
    uint64_t islandCount = 0;
    int tileX = CENTER_TILE.x, tileZ = CENTER_TILE.z, tileDx = 0, tileDz = -1;
    for (uint64_t iteration = 0; iteration < ITERATIONS; ++iteration) {
        if ((!tileX && !tileZ) || !((getChunkSeed(startSeedLowest25Bits, tileX, tileZ) >> 24) & 1)) ++islandCount;
        if (tileX == tileZ || (tileX < 0 && tileX == -tileZ) || (tileX > 0 && tileX == 1 - tileZ)) {
            std::swap(tileDx, tileDz);
            tileDx = -tileDx;
        }
        tileX += tileDx;
        tileZ += tileDz;
    }
    return islandCount;
}

// Spirals outwards to find the number of ocean tiles with the full 64-bit start seed, and the check being nextInt(10) != 0.
[[nodiscard]] uint64_t countIslandsAt(uint64_t startSeed) {
    uint64_t iterations = static_cast<uint64_t>(tileDimensions.x) * tileDimensions.z;
    uint64_t islandCount = 0;
    int tileX = CENTER_TILE.x, tileZ = CENTER_TILE.z, tileDx = 0, tileDz = -1;
    for (uint64_t iteration = 0; iteration < iterations; ++iteration) {
        if ((!tileX && !tileZ) || mcFirstIsZero(getChunkSeed(startSeed, tileX, tileZ), 10)) ++islandCount;
        if (tileX == tileZ || (tileX < 0 && tileX == -tileZ) || (tileX > 0 && tileX == 1 - tileZ)) {
            std::swap(tileDx, tileDz);
            tileDx = -tileDx;
        }
        tileX += tileDx;
        tileZ += tileDz;
    }
    return islandCount;
}

void initializeGlobals() {
    // Set threshold
    tileDimensions.x = INITIAL_TILE_DIMENSIONS.x;
    tileDimensions.z = INITIAL_TILE_DIMENSIONS.z;

    lowest25BitsArray = reinterpret_cast<std::pair<uint64_t, uint32_t> *>(calloc(LOWEST_25_BITS_ARRAY_SIZE, sizeof(*lowest25BitsArray)));
    #if ITERATE_ON_CHUNK_SEEDS
    // Lowest 25 bits must match z-coordinate's parity
    for (uint32_t lowest25Bits = (CENTER_TILE.z & 1), i = 0; lowest25Bits < UINT32_C(1) << 25 && i < LOWEST_25_BITS_ARRAY_SIZE; lowest25Bits += 2, ++i) {
        uint32_t startSeedLowest25Bits = static_cast<uint32_t>(chunkSeedToStartSeed(lowest25Bits, CENTER_TILE.x, CENTER_TILE.z));
        uint64_t tileCount = testLowest25Bits(startSeedLowest25Bits);
        lowest25BitsArray[i] = {tileCount, lowest25Bits};
    }
    #else
    // Lowest 25 bits must be even
    for (uint32_t lowest25Bits = 0, i = 0; lowest25Bits < UINT32_C(1) << 25 && i < LOWEST_25_BITS_ARRAY_SIZE; lowest25Bits += 2, ++i) {
        uint64_t tileCount = testLowest25Bits(lowest25Bits);
        lowest25BitsArray[i] = {tileCount, lowest25Bits};
    }
    #endif
    std::sort(lowest25BitsArray, lowest25BitsArray + LOWEST_25_BITS_ARRAY_SIZE - 1, sortPairsAscending);
    outputString("[Queue range: %d - %d]\n", lowest25BitsArray[0].first, lowest25BitsArray[LOWEST_25_BITS_ARRAY_SIZE - 1].first);
}

void *runWorker(void* workerIndex) {
    Generator g;
    setupGenerator(&g, VERSION, LARGE_BIOMES_FLAG);
    Layer *const oceanLayer = &g.ls.layers[L_CONTINENT_4096];
    Layer *const mandateAllOceanLayer = &g.ls.layers[L_BIOME_256];
    const uint64_t oceanLayerSalt = oceanLayer->layerSalt;

    const Pos allOceanScaledCenter = {
        CENTER_TILE.x << TILE_TO_SAMPLING,
        CENTER_TILE.z << TILE_TO_SAMPLING
    };
    Pos mapDimensions = {
        INITIAL_TILE_DIMENSIONS.x << TILE_TO_SAMPLING,
        INITIAL_TILE_DIMENSIONS.z << TILE_TO_SAMPLING
    };
    int *map = reinterpret_cast<int *>(calloc(getMinLayerCacheSize(mandateAllOceanLayer, mapDimensions.x, mapDimensions.z), sizeof(*map)));
    if (!map) return NULL;

    uint64_t currentIndex;
    if (!getNextInteger(workerIndex, &currentIndex)) return NULL;
    do {
        if (currentIndex >= LOWEST_25_BITS_ARRAY_SIZE) break;
        std::pair<uint64_t, uint32_t> currentElement = lowest25BitsArray[currentIndex];
        // Recover start seed from successful chunk seed
        for (uint64_t upper39Bits = 0; upper39Bits < UINT64_C(1) << (64 - 25); ++upper39Bits) {
            #if ITERATE_ON_CHUNK_SEEDS
                if (!(currentElement.second >> 24) && !(upper39Bits % 5)) continue;
                uint64_t chunkSeed = (upper39Bits << 25) + static_cast<uint64_t>(currentElement.second);
                uint64_t startSeed = chunkSeedToStartSeed(chunkSeed, CENTER_TILE.x, CENTER_TILE.z);
            #else
                uint64_t startSeed = (upper39Bits << 25) + static_cast<uint64_t>(currentElement.second);
            #endif
            
            // See if the current startSeed could even generate a mushroom island bigger than the largest found thus far
            uint64_t tileCount = countIslandsAt(startSeed);
            if (tileCount > 1) continue;

            // Recover corresponding worldseed
            uint64_t worldseed = startSeedToWorldseed(startSeed, oceanLayerSalt, false);

            // Sample and make sure the starting coordinate is actually mushroom island
            if (tileDimensions.x << TILE_TO_SAMPLING > mapDimensions.x || tileDimensions.z << TILE_TO_SAMPLING > mapDimensions.z) {
                mapDimensions.x = tileDimensions.x << TILE_TO_SAMPLING;
                mapDimensions.z = tileDimensions.z << TILE_TO_SAMPLING;
                map = reinterpret_cast<int *>(realloc(map, getMinLayerCacheSize(mandateAllOceanLayer, mapDimensions.x, mapDimensions.z)*sizeof(*map)));
                if (!map) return NULL;
            }
            setLayerSeed(mandateAllOceanLayer, worldseed);
            if (genArea(mandateAllOceanLayer, map, allOceanScaledCenter.x - mapDimensions.x/2, allOceanScaledCenter.z - mapDimensions.z/2, mapDimensions.x, mapDimensions.z)) continue;
            for (uint64_t i = 0; i < static_cast<uint64_t>(mapDimensions.x)*mapDimensions.z; ++i) {
                if (map[i] != ocean) goto nextEntry;
            }

            // Check if # of tiles is larger
            if (UPDATE_THRESHOLD) {
                tileDimensionsMutex.lock();
                if (tileDimensions.x << TILE_TO_SAMPLING <= mapDimensions.x || tileDimensions.z << TILE_TO_SAMPLING <= mapDimensions.z) {
                    // Needs proof of correctness
                    tileDimensions.x = (mapDimensions.x >> TILE_TO_SAMPLING) + 1;
                    tileDimensions.z = (mapDimensions.z >> TILE_TO_SAMPLING) + 1;
                    tileDimensionsMutex.unlock();
                } else tileDimensionsMutex.unlock();
            }
            outputString("%" PRId64 "\t%dx%d\n", worldseed, mapDimensions.x*mandateAllOceanLayer->scale, mapDimensions.z*mandateAllOceanLayer->scale);
            nextEntry: continue;
        }
    } while (getNextInteger(NULL, &currentIndex));
    free(map);
    return NULL;
}