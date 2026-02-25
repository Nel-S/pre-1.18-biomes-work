#include "../core/bruteforce.h"
#include "../cubiomes/finders.h"
#include "../reversal.h"
#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <unordered_set>
#include <utility>

// #define ITERATE_ON_CHUNK_SEEDS true

const uint64_t GLOBAL_START_INTEGER = 7;
const uint64_t GLOBAL_NUMBER_OF_INTEGERS = UINT64_MAX/((UINT64_C(1) << 26) * 25) - GLOBAL_START_INTEGER + (!!GLOBAL_START_INTEGER);
// const uint64_t GLOBAL_NUMBER_OF_INTEGERS = 7;
const int GLOBAL_NUMBER_OF_WORKERS = 7;
const char *INPUT_FILEPATH = NULL;
const char *OUTPUT_FILEPATH = NULL;
const bool TIME_PROGRAM = false;

const int VERSION = MC_1_6;
const bool LARGE_BIOMES_FLAG = false;
const Pos START_TILE_COORDINATE = {30, 20};
const int MINIMUM_TILES_THRESHOLD = 9;
const bool UPDATE_THRESHOLD = false;
// const int MINIMUM_TILES_THRESHOLD = 6;
// const bool UPDATE_THRESHOLD = true;

DEFAULT_LOCALS_INITIALIZATION

// Threshold variable + mutex for it
volatile int minimumTilesThreshold;
std::mutex minimumTilesThresholdMutex;

// The program involves ranking the possible lowest 26 bits of the starting coordinate's "chunk seed" (internal state from which it makes the mushroom island decision). We store that using an array of {tile count, lowest 26 bits} entries.
std::pair<int, uint32_t> *lowest26BitsArray;
// The size of the array.
// #if ITERATE_ON_CHUNK_SEEDS
// Two bits are removed because the uppermost two bits must be 0; another bit is removed because only one parity is possible.
const size_t LOWEST_26_BITS_ARRAY_SIZE = UINT64_C(1) << (26 - 2 - 1);
// #else
// // One bit removed because only one parity is possible
// const size_t LOWEST_26_BITS_ARRAY_SIZE = UINT64_C(1) << (26 - 1);
// #endif

// Utilities for the pair entries.
[[nodiscard]] bool pairComparer(const std::pair<int, uint32_t>& first, const std::pair<int, uint32_t>& second) {
    return first.first > second.first;
}

struct pairHash {
    [[nodiscard]] std::size_t operator()(std::pair<int, int> const &v) const {
        // return ((v.first + v.second) * (v.first + v.second + 1) / 2) + v.second;
        return (v.second << 16) ^ v.first;
    }
};

// The 1:N scale that the world exists at when the mushroom tiles are placed. 
const int TILE_SCALE = 256 * (1 + 3*LARGE_BIOMES_FLAG);

// Mushroom islands generate if nextInt(100) == 0, which only happens if a hypothetical nextInt(4) would have returned 0, which only happens if the 25th and 26th bits are 0.
// Therefore this recursively flood-fills to find the area of the mushroom island if the start seed was only 26 bits, and the check was only nextInt(4) == 0. This creates an upper bound on the actual possible # of tiles.
[[nodiscard]] int testLowest26Bits(uint32_t startSeedLowest26Bits, int tileX, int tileZ, std::unordered_set<std::pair<int, int>, pairHash> &checkedTiles) {
    // Add current coordinate to checked-tiles set
    checkedTiles.insert(std::pair(tileX, tileZ));

    // If a mushroom tile wouldn't generate, return failure
    if ((getChunkSeed(startSeedLowest26Bits, tileX, tileZ) >> 24) & 3) return 0;
    
    // Check orthagonal tiles
    int count = 1;
    if (checkedTiles.find(std::pair(tileX + 1, tileZ)) == checkedTiles.end() && tileX + 1 <= 29999999/TILE_SCALE) count += testLowest26Bits(startSeedLowest26Bits, tileX + 1, tileZ, checkedTiles);
    if (checkedTiles.find(std::pair(tileX - 1, tileZ)) == checkedTiles.end() && tileX - 1 >= -30000000/TILE_SCALE) count += testLowest26Bits(startSeedLowest26Bits, tileX - 1, tileZ, checkedTiles);
    if (checkedTiles.find(std::pair(tileX, tileZ + 1)) == checkedTiles.end() && tileZ + 1 <=  29999999/TILE_SCALE) count += testLowest26Bits(startSeedLowest26Bits, tileX, tileZ + 1, checkedTiles);
    if (checkedTiles.find(std::pair(tileX, tileZ - 1)) == checkedTiles.end() && tileZ - 1 >= -30000000/TILE_SCALE) count += testLowest26Bits(startSeedLowest26Bits, tileX, tileZ - 1, checkedTiles);
    return count;
}

// Recursively flood-fills to find the area of the mushroom island with the full 64-bit start seed, and the check being nextInt(100) == 0.
[[nodiscard]] int testForMushroomIslandAt(uint64_t startSeed, int tileX, int tileZ, std::unordered_set<std::pair<int, int>, pairHash> &checkedTileSet) {
    // Add current coordinate to checked-tiles set
    checkedTileSet.insert(std::pair(tileX, tileZ));

    // If a mushroom tile wouldn't generate, return failure
    if (!mcFirstIsZero(getChunkSeed(startSeed, tileX, tileZ), 100)) return 0;
    
    // Check orthagonal tiles
    int count = 1;
    if (checkedTileSet.find(std::pair(tileX + 1, tileZ)) == checkedTileSet.end() && tileX + 1 <= 29999999/TILE_SCALE) count += testForMushroomIslandAt(startSeed, tileX + 1, tileZ, checkedTileSet);
    if (checkedTileSet.find(std::pair(tileX - 1, tileZ)) == checkedTileSet.end() && tileX - 1 >= -30000000/TILE_SCALE) count += testForMushroomIslandAt(startSeed, tileX - 1, tileZ, checkedTileSet);
    if (checkedTileSet.find(std::pair(tileX, tileZ + 1)) == checkedTileSet.end() && tileZ + 1 <=  29999999/TILE_SCALE) count += testForMushroomIslandAt(startSeed, tileX, tileZ + 1, checkedTileSet);
    if (checkedTileSet.find(std::pair(tileX, tileZ - 1)) == checkedTileSet.end() && tileZ - 1 >= -30000000/TILE_SCALE) count += testForMushroomIslandAt(startSeed, tileX, tileZ - 1, checkedTileSet);
    return count;
}

void initializeGlobals() {
    // Set threshold
    minimumTilesThreshold = MINIMUM_TILES_THRESHOLD;
    std::unordered_set<std::pair<int, int>, pairHash> checkedTiles;

    lowest26BitsArray = reinterpret_cast<std::pair<int, uint32_t> *>(calloc(LOWEST_26_BITS_ARRAY_SIZE, sizeof(*lowest26BitsArray)));
    // #if ITERATE_ON_CHUNK_SEEDS
    // Lowest 26 bits must match z-coordinate's parity, and uppermost 2 bits must be 0
    for (uint32_t lowest26Bits = (START_TILE_COORDINATE.z & 1), i = 0; lowest26Bits < UINT32_C(1) << 24 && i < LOWEST_26_BITS_ARRAY_SIZE; lowest26Bits += 2, ++i) {
        checkedTiles.clear();
        uint32_t startSeedLowest26Bits = static_cast<uint32_t>(chunkSeedToStartSeed(lowest26Bits, START_TILE_COORDINATE.x, START_TILE_COORDINATE.z));
        int tileCount = testLowest26Bits(startSeedLowest26Bits, START_TILE_COORDINATE.x, START_TILE_COORDINATE.z, checkedTiles);
    // #else
    // // Lowest 26 bits must be even
    // for (uint32_t lowest26Bits = 0, i = 0; lowest26Bits < UINT32_C(1) << 26 && i < LOWEST_26_BITS_ARRAY_SIZE; lowest26Bits += 2, ++i) {
    //     checkedTiles.clear();
    //     int tileCount = testLowest26Bits(lowest26Bits, START_TILE_COORDINATE.x, START_TILE_COORDINATE.z, checkedTiles);
    // #endif
        lowest26BitsArray[i] = {tileCount, lowest26Bits};
    }
    std::sort(lowest26BitsArray, lowest26BitsArray + LOWEST_26_BITS_ARRAY_SIZE - 1, pairComparer);
    outputString("[Queue range: %d - %d]\n", lowest26BitsArray[0].first, lowest26BitsArray[LOWEST_26_BITS_ARRAY_SIZE - 1].first);
}

void *runWorker(void* workerIndex) {
    std::unordered_set<std::pair<int, int>, pairHash> checkedTiles;

    Generator g;
    setupGenerator(&g, VERSION, LARGE_BIOMES_FLAG);
    Layer *const mushroomLayer = &g.ls.layers[L_MUSHROOM_256];
    const uint64_t layerSalt = mushroomLayer->layerSalt;

    int *const map = reinterpret_cast<int *>(calloc(getMinLayerCacheSize(mushroomLayer, 1, 1), sizeof(*map)));
    if (!map) return NULL;

    uint64_t currentIndex;
    if (!getNextInteger(workerIndex, &currentIndex)) return NULL;
    do {
        if (currentIndex >= LOWEST_26_BITS_ARRAY_SIZE) break;
        std::pair<int, uint32_t> currentElement = lowest26BitsArray[currentIndex];
        if (currentElement.first < minimumTilesThreshold) break;
        // Recover start seed from successful chunk seed
        // #if ITERATE_ON_CHUNK_SEEDS
        for (uint64_t upper38Bits = 0; upper38Bits < UINT64_C(1) << (64 - 26); upper38Bits += (100 >> 2)) {
            uint64_t chunkSeed = (upper38Bits << 26) + static_cast<uint64_t>(currentElement.second);
            uint64_t startSeed = chunkSeedToStartSeed(chunkSeed, START_TILE_COORDINATE.x, START_TILE_COORDINATE.z);
        // #else
        // for (uint64_t upper38Bits = 0; upper38Bits < UINT64_C(1) << (64 - 26); ++upper38Bits) {
        //     uint64_t startSeed = (upper38Bits << 26) + static_cast<uint64_t>(currentElement.second);
        // #endif
            

            // Reset checked tiles set
            checkedTiles.clear();
            // See if the current startSeed could even generate a mushroom island bigger than the largest found thus far
            int tileCount = testForMushroomIslandAt(startSeed, START_TILE_COORDINATE.x, START_TILE_COORDINATE.z, checkedTiles);
            if (tileCount < minimumTilesThreshold) continue;

            // Recover corresponding worldseed
            uint64_t worldseed = startSeedToWorldseed(startSeed, layerSalt, false);

            // Sample and make sure the starting coordinate is actually mushroom island
            setLayerSeed(mushroomLayer, worldseed);
            if (genArea(mushroomLayer, map, START_TILE_COORDINATE.x, START_TILE_COORDINATE.z, 1, 1) || map[0] != mushroom_fields) continue;

            // Check if # of tiles is larger
            if (UPDATE_THRESHOLD) minimumTilesThresholdMutex.lock();
            if (tileCount < minimumTilesThreshold) {
                if (UPDATE_THRESHOLD) minimumTilesThresholdMutex.unlock();
                continue;
            }
            // If so, update best count if relevant, and print
            if (UPDATE_THRESHOLD) {
                if (tileCount > minimumTilesThreshold) minimumTilesThreshold = tileCount;
                minimumTilesThresholdMutex.unlock();
            }
            outputString("%" PRId64 "\t%d\t%d\t%d\n", worldseed, START_TILE_COORDINATE.x*TILE_SCALE, START_TILE_COORDINATE.z*TILE_SCALE, tileCount);
        }
    } while (getNextInteger(NULL, &currentIndex));
    return NULL;
}