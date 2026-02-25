#ifndef LAYERUTIL_H
#define LAYERUTIL_H

#include <stdbool.h>
#include "../util.h"

#ifdef __cplusplus
extern "C" {
#endif

STRUCT(Configuration) {
	int version;
	uint64_t worldseed;
	bool largeBiomes;
	int minimumX, minimumZ, maximumX, maximumZ, width, height;
	size_t startingLayerID;
};

static inline int64_t flatten(int x, int z, const Configuration *const configuration) {
	return (z - configuration->minimumZ)*(int64_t)configuration->width + (x - configuration->minimumX);
}

static inline bool oceanCheck(int biome, int version) {
	if (version <= MC_1_6) return biome == ocean;
	return isOceanic(biome);
}

static inline bool shallowOceanCheck(int biome, int version) {
	if (version <= MC_1_6) return biome == ocean;
	return isShallowOcean(biome);
}

static inline bool similarLayerCheck(int biome1, int biome2, int version) {
	if (version <= MC_1_6) return biome1 == biome2;
	return areSimilar(version, biome1, biome2);
}

// Biomes I
void islandLayer(int *const biomes, uint64_t salt, const Configuration *const configuration);
void zoomLayer(int *const biomes, int *const tempBuffer, bool fuzzy, uint64_t salt, const Configuration *const configuration);
void addIslandLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration);
void removeTooMuchOceanLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration);
void addSnowLayer(int *const biomes, uint64_t salt, const Configuration *const configuration);
void addEdgeLayerCoolWarm(int *const biomes, int *const tempBuffer, const Configuration *const configuration);
void addEdgeLayerHeatIce(int *const biomes, int *const tempBuffer, const Configuration *const configuration);
void addEdgeLayerIntroduceSpecial(int *const biomes, uint64_t salt, const Configuration *const configuration);
void addMushroomIslandLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration);
void addDeepOceanLayer(int *const biomes, int *const tempBuffer, const Configuration *const configuration);
void biomeInitLayer(int *const biomes, uint64_t salt, const Configuration *const configuration);
void addBambooLayer(int *const biomes, uint64_t salt, const Configuration *const configuration);
void biomeEdgeLayer(int *const biomes, int *const tempBuffer, const Configuration *const configuration);

// Hills
void riverInitLayer(int *const riverNoise, uint64_t salt, const Configuration *const configuration);
void regionHillsLayer(int *const biomes, const int *const hillsNoise, int *const tempBuffer, uint64_t salt, const Configuration *const configuration);

// Biomes II
void addSunflowerLayer(int *const biomes, uint64_t salt, const Configuration *const configuration);
void shoreLayer(int *const biomes, int *const tempBuffer, const Configuration *const configuration);
void addSwampRiverLayer(int *const biomes, uint64_t salt, const Configuration *const configuration);
void smoothLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration);

// Rivers
void riverLayer(int *const riverNoise, int *const tempBuffer, const Configuration *const configuration);
void riverMixerLayer(int *const biomes, const int *const riverNoise, const Configuration *const configuration);

// Oceans
void oceanLayer(int *const oceans, const Configuration *const configuration);
void oceanMixerLayer(int *const biomes, const int *const oceanNoise, int *const tempBuffer, const Configuration *const configuration);

#ifdef __cplusplus
}
#endif

#endif