#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "cubiomes/generator.h"
#include "cubiomes/util.h"
#include "layerFunctions.h"

const uint64_t SUPPORTED_LAYERS = (UINT64_C(1) << L_CONTINENT_4096)
	| (UINT64_C(1) << L_ZOOM_4096)
	| (UINT64_C(1) << L_LAND_4096)
	| (UINT64_C(1) << L_ZOOM_2048)
	| (UINT64_C(1) << L_LAND_2048)
	| (UINT64_C(1) << L_ZOOM_1024)
	| (UINT64_C(1) << L_LAND_1024_A)
	| (UINT64_C(1) << L_LAND_1024_B)
	| (UINT64_C(1) << L_LAND_1024_C)
	| (UINT64_C(1) << L_ISLAND_1024)
	| (UINT64_C(1) << L_SNOW_1024)
	| (UINT64_C(1) << L_LAND_1024_D)
	| (UINT64_C(1) << L_COOL_1024)
	| (UINT64_C(1) << L_HEAT_1024)
	| (UINT64_C(1) << L_SPECIAL_1024)
	| (UINT64_C(1) << L_ZOOM_512)
	| (UINT64_C(1) << L_LAND_512)
	| (UINT64_C(1) << L_ZOOM_256)
	| (UINT64_C(1) << L_LAND_256)
	| (UINT64_C(1) << L_MUSHROOM_256)
	| (UINT64_C(1) << L_DEEP_OCEAN_256)
	| (UINT64_C(1) << L_NOISE_256)
	| (UINT64_C(1) << L_BIOME_256)
	| (UINT64_C(1) << L_BAMBOO_256)
	| (UINT64_C(1) << L_ZOOM_128)
	| (UINT64_C(1) << L_ZOOM_64)
;

int saveAsImage(const Configuration *const configuration, const int *const biomes, const char *filepath) {
	if (!configuration) return 1;
	if (!biomes) return 2;
	if (!filepath) return 3;

	unsigned char biomeColors[256][3];
	initBiomeColors(biomeColors);
	unsigned char pixels[3*configuration->width*configuration->height];
	biomesToImage(pixels, biomeColors, biomes, configuration->width, configuration->height, 1, false);
	int errorCode = savePPM(filepath, pixels, configuration->width, configuration->height);
	if (errorCode == -1) return 4;
	if (errorCode == 1) return 5;
	return 0;
}

int emulateBiomes(const Configuration *const configuration, int *const biomes, size_t biomesCapacity, int *const requiredMargin) {
	if (!configuration) return 1;
	if (!biomes) return 2;
	if (configuration->startingLayerID >= L_NUM) return 3;
	if (biomesCapacity < (size_t)configuration->width * (size_t)configuration->height) return 4;

	int *const tempBuffer = (int *const)calloc(configuration->width * configuration->height, sizeof(*tempBuffer));

	// 1:8192 for Beta 1.8, 1:4096 for 1.0+
	// 0 = ocean, 1 = non-ocean
	// Allows 0 -> 1
	if (requiredMargin) *requiredMargin = 0;
	islandLayer(biomes, 1, configuration);
	if (configuration->startingLayerID == L_CONTINENT_4096) {
		free(tempBuffer);
		return 0;
	}

	// (Fuzzy)
	// 1:4096 for Beta 1.8, 1:2048 for 1.0+
	// 0 = ocean, 1 = non-ocean
	// Allows 0 -> 1, 1 -> 0 (on coordinates odd on 1+ axes)
	if (requiredMargin) *requiredMargin = ceil(*requiredMargin/2.);
	zoomLayer(biomes, tempBuffer, true, 2000, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_4096 : L_ZOOM_2048)) {
		free(tempBuffer);
		return 0;
	}

	// 1:4096 for Beta 1.8, 1:2048 for 1.0+
	// 0 = ocean, 1 = non-ocean
	// Allows 0 -> 1, 1 -> 0
	if (requiredMargin) *requiredMargin += 1;
	addIslandLayer(biomes, tempBuffer, 1, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_4096 : L_LAND_2048)) {
		free(tempBuffer);
		return 0;
	}

	// 1:2048 for Beta 1.8, 1:1024 for 1.0+
	// 0 = ocean, 1 = non-ocean
	// Allows 0 -> 1, 1 -> 0 (on coordinates odd on 1+ axes)
	if (requiredMargin) *requiredMargin = ceil(*requiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 2001, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_2048 : L_ZOOM_1024)) {
		free(tempBuffer);
		return 0;
	}

	// 1:2048 for Beta 1.8, 1:1024 for 1.0+
	// 0 = ocean, 1 = non-ocean
	// Allows 0 -> 1, 1 -> 0
	if (requiredMargin) *requiredMargin += 1;
	addIslandLayer(biomes, tempBuffer, 2, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_2048 : L_LAND_1024_A)) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_7) {
		// 1:1024
		// 0 = ocean, 1 = non-ocean
		// Allows 0 -> 1, 1 -> 0
		if (requiredMargin) *requiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 50, configuration);
		if (configuration->startingLayerID == L_LAND_1024_B) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024
		// 0 = ocean, 1 = non-ocean
		// Allows 0 -> 1, 1 -> 0
		if (requiredMargin) *requiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 70, configuration);
		if (configuration->startingLayerID == L_LAND_1024_C) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024
		// 0 = ocean, 1 = non-ocean
		// Allows 0 -> 1
		if (requiredMargin) *requiredMargin += 1;
		removeTooMuchOceanLayer(biomes, tempBuffer, 2, configuration);
		if (configuration->startingLayerID == L_ISLAND_1024) {
			free(tempBuffer);
			return 0;
		}
	}

	if (configuration->version >= MC_1_0) {
		// 1:1024
		// 1.6-: 0 = ocean, 1 = plains, 12 = snowy tundra
		// 		Allows 1 -> 12
		// 1.7+: 0 = ocean, 1 = Warm, 3 = Cold, 4 = Freezing
		// 		Allows 1 -> 3, 1 -> 4
		addSnowLayer(biomes, 2, configuration);
		if (configuration->startingLayerID == L_SNOW_1024) {
			free(tempBuffer);
			return 0;
		}
	}

	if (configuration->version >= MC_1_7) {
		// 1:1024
		// 0 = ocean, 1 = Warm, 3 = Cold, 4 = Freezing
		// 		Allows 0 -> 1, 0 -> 3, 0 -> 4, 1 -> 0, 3 -> 0
		if (requiredMargin) *requiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 3, configuration);
		if (configuration->startingLayerID == L_LAND_1024_D) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024
		// 0 = ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing
		// 		Allows 1 -> 2
		if (requiredMargin) *requiredMargin += 1;
		addEdgeLayerCoolWarm(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_COOL_1024) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024
		// 0 = ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing
		// 		Allows 4 -> 3
		if (requiredMargin) *requiredMargin += 1;
		addEdgeLayerHeatIce(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_HEAT_1024) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024
		// 0 = ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing,
		// [257, 513, 769, 1025, 1281, 1537, 1793, 2049, 2305, 2561, 2817, 3073, 3329, 3585, 3841] = Warm Special,
		// [258, 514, 770, 1026, 1282, 1538, 1794, 2050, 2306, 2562, 2818, 3074, 3330, 3586, 3842] = Lush Special,
		// [259, 515, 771, 1027, 1283, 1539, 1795, 2051, 2307, 2563, 2819, 3075, 3331, 3587, 3843] = Cold Special,
		// [260, 516, 772, 1028, 1284, 1540, 1796, 2052, 2308, 2564, 2820, 3076, 3332, 3588, 3844] = Freezing Special,
		// 		Allows 1 -> [257, 513, 769, 1025, 1281, 1537, 1793, 2049, 2305, 2561, 2817, 3073, 3329, 3585, 3841], 2 -> [258, 514, 770, 1026, 1282, 1538, 1794, 2050, 2306, 2562, 2818, 3074, 3330, 3586, 3842], 3 -> [259, 515, 771, 1027, 1283, 1539, 1795, 2051, 2307, 2563, 2819, 3075, 3331, 3587, 3843], 4 -> [260, 516, 772, 1028, 1284, 1540, 1796, 2052, 2308, 2564, 2820, 3076, 3332, 3588, 3844]
		addEdgeLayerIntroduceSpecial(biomes, 3, configuration);
		if (configuration->startingLayerID == L_SPECIAL_1024) {
			free(tempBuffer);
			return 0;
		}
	}

	// 1:1024 for Beta 1.8, 1:512 for 1.0+
	// Beta 1.8: 0 = ocean, 1 = plains
	//		Allows 0 -> 1, 1 -> 0 (on coordinates odd on 1+ axes)
	// 1.0-1.6: 0 = ocean, 1 = plains, 12 = snowy tundra
	//		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7+: 0 = ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing,
	// [257, 513, 769, 1025, 1281, 1537, 1793, 2049, 2305, 2561, 2817, 3073, 3329, 3585, 3841] = Warm Special,
	// [258, 514, 770, 1026, 1282, 1538, 1794, 2050, 2306, 2562, 2818, 3074, 3330, 3586, 3842] = Lush Special,
	// [259, 515, 771, 1027, 1283, 1539, 1795, 2051, 2307, 2563, 2819, 3075, 3331, 3587, 3843] = Cold Special,
	// [260, 516, 772, 1028, 1284, 1540, 1796, 2052, 2308, 2564, 2820, 3076, 3332, 3588, 3844] = Freezing Special,
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (requiredMargin) *requiredMargin = ceil(*requiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 2002, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_1024 : L_ZOOM_512)) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version <= MC_1_6) {
		// 1:1024 for Beta 1.8, 1:512 for 1.0+
		// Beta 1.8: 0 = ocean, 1 = plains
		//		Allows 0 -> 1, 1 -> 0
		// 1.0-1.6: 0 = ocean, 1 = plains, 10 = frozen ocean, 12 = snowy tundra
		//		Allows 0 -> 1, 0 -> 10, 0 -> 12, 1 -> 0, 10 -> 12, 12 -> 10
		if (requiredMargin) *requiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 3, configuration);
		if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_1024_A : L_LAND_512)) {
			free(tempBuffer);
			return 0;
		}
	}

	// 1:512 for Beta 1.8, 1:256 for 1.0+
	// Beta 1.8: 0 = ocean, 1 = plains
	//		Allows 0 -> 1, 1 -> 0 (on coordinates odd on 1+ axes)
	// 1.0-1.6: 0 = ocean, 1 = plains, 10 = frozen ocean, 12 = snowy tundra
	//		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7+: 0 = ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing,
	// [257, 513, 769, 1025, 1281, 1537, 1793, 2049, 2305, 2561, 2817, 3073, 3329, 3585, 3841] = Warm Special,
	// [258, 514, 770, 1026, 1282, 1538, 1794, 2050, 2306, 2562, 2818, 3074, 3330, 3586, 3842] = Lush Special,
	// [259, 515, 771, 1027, 1283, 1539, 1795, 2051, 2307, 2563, 2819, 3075, 3331, 3587, 3843] = Cold Special,
	// [260, 516, 772, 1028, 1284, 1540, 1796, 2052, 2308, 2564, 2820, 3076, 3332, 3588, 3844] = Freezing Special,
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (requiredMargin) *requiredMargin = ceil(*requiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 2003, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_512 : L_ZOOM_256)) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version == MC_B1_8) {
		// 1:512
		// 0 = ocean, 1 = plains
		//		Allows 0 -> 1, 1 -> 0
		if (requiredMargin) *requiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 3, configuration);
		if (configuration->startingLayerID == L_LAND_512) {
			free(tempBuffer);
			return 0;
		}

		// 1:256
		// 0 = ocean, 1 = plains
		//		Allows 0 -> 1, 1 -> 0 (on coordinates odd on 1+ axes)
		if (requiredMargin) *requiredMargin = ceil(*requiredMargin/2.);
		zoomLayer(biomes, tempBuffer, false, 2004, configuration);
		if (configuration->startingLayerID == L_ZOOM_256) {
			free(tempBuffer);
			return 0;
		}
	}

	// 1:256
	if (requiredMargin) *requiredMargin += 1;
	addIslandLayer(biomes, tempBuffer, configuration->version == MC_B1_8 ? 3 : 4, configuration);
	if (configuration->startingLayerID == L_LAND_256) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_0) {
		// 1:256
		if (requiredMargin) *requiredMargin += 1;
		addMushroomIslandLayer(biomes, tempBuffer, 5, configuration);
		if (configuration->startingLayerID == L_MUSHROOM_256) {
			free(tempBuffer);
			return 0;
		}
	}

	if (configuration->version >= MC_1_7) {
		// 1:256
		if (requiredMargin) *requiredMargin += 1;
		addDeepOceanLayer(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_DEEP_OCEAN_256) {
			free(tempBuffer);
			return 0;
		}
	}

	int *const rivers = (int *const)calloc(configuration->width * configuration->height, sizeof(*rivers));
	memcpy(rivers, biomes, configuration->width*configuration->height*sizeof(*biomes));
	// 1:256
	riverInitLayer(rivers, 100, configuration);
	if (configuration->startingLayerID == L_NOISE_256) {
		memcpy(biomes, rivers, configuration->width*configuration->height*sizeof(*rivers));
		free(rivers);
		free(tempBuffer);
		return 0;
	}

	// 1:256
	biomeInitLayer(biomes, 200, configuration);
	if (configuration->startingLayerID == L_BIOME_256) {
		free(rivers);
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_14) {
		// 1:256
		addBambooLayer(biomes, 1001, configuration);
		if (configuration->startingLayerID == L_BAMBOO_256) {
			free(rivers);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:128
	if (requiredMargin) *requiredMargin = ceil(*requiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1000, configuration);
	if (configuration->startingLayerID == L_ZOOM_128) {
		free(rivers);
		free(tempBuffer);
		return 0;
	}

	// 1:64
	if (requiredMargin) *requiredMargin = ceil(*requiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1001, configuration);
	if (configuration->startingLayerID == L_ZOOM_64) {
		free(rivers);
		free(tempBuffer);
		return 0;
	}

	free(rivers);
	free(tempBuffer);
	return 5;
}

int getTrueBiomes(const Configuration *const configuration, int *const biomes, size_t biomesCapacity) {
	if (!configuration) return 1;
	if (!biomes) return 2;
	if (configuration->startingLayerID >= L_NUM) return 3;
	Generator g;
	setupGenerator(&g, configuration->version, configuration->largeBiomes);
	applySeed(&g, DIM_OVERWORLD, configuration->worldseed);
	int scale = g.ls.layers[configuration->startingLayerID].scale;
	if (scale >= 256) scale = 256;
	else if (scale >= 64) scale = 64;
	else if (scale >= 16) scale = 16;
	else if (scale >= 4) scale = 4;
	else scale = 1;
	if (!scale) return 4;
	if (biomesCapacity < getMinCacheSize(&g, scale, configuration->width, 1, configuration->height)) return 5;

	if (genArea(&g.ls.layers[configuration->startingLayerID], biomes, configuration->minimumX, configuration->minimumZ, configuration->width, configuration->height)) return 6;
	return 0;
}

int main() {
	Configuration configuration = {
		0,
		8675309,
		// -246117,
		// 111111111111111111,
		false,
		-100, -100, 100, 100, 201, 201,
		0
	};
	// For each version:
	for (int version = MC_B1_8; version <= MC_1_17; ++version) {
		configuration.version = version;
		// For both normal and large biomes (for supported versions):
		for (int largeBiomes = 0; largeBiomes <= (version >= MC_1_3); ++largeBiomes) {
			configuration.largeBiomes = largeBiomes;
			// Obtain list of valid layers for that version+mode
			Generator g;
			setupGenerator(&g, configuration.version, configuration.largeBiomes);
			applySeed(&g, DIM_OVERWORLD, configuration.worldseed);
			// For each layer that was initialized:
			for (size_t startingLayerID = 0; startingLayerID < L_NUM; ++startingLayerID) {
				if (!(SUPPORTED_LAYERS & (UINT64_C(1) << startingLayerID))) continue;
				if (!g.ls.layers[startingLayerID].mc) continue;
				configuration.startingLayerID = startingLayerID;
				// Initialize arrays
				int scale = g.ls.layers[configuration.startingLayerID].scale;
				if (scale >= 256) scale = 256;
				else if (scale >= 64) scale = 64;
				else if (scale >= 15) scale = 16;
				else if (scale >= 4) scale = 4;
				else scale = 1;
				size_t cacheSize = getMinCacheSize(&g, scale, configuration.width, 1, configuration.height);
				int *const emulatedBiomes = (int *const)calloc(cacheSize, sizeof(*emulatedBiomes));
				int *const trueBiomes = (int *const)calloc(cacheSize, sizeof(*trueBiomes));

				// Fill emulated array
				int errorCode, requiredMargin = 0;
				errorCode = emulateBiomes(&configuration, emulatedBiomes, cacheSize, &requiredMargin);
				if (errorCode) {
					printf("Error: emulateBiomes() under %s%s, layer %s returned nonzero error code %d.\n", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID), errorCode);
					free(emulatedBiomes);
					free(trueBiomes);
					continue;
				}
				// Fill true array
				errorCode = getTrueBiomes(&configuration, trueBiomes, cacheSize);
				if (errorCode) {
					printf("Error: getTrueBiomes() under %s%s, layer %s returned nonzero error code %d.\n", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID), errorCode);
					free(emulatedBiomes);
					free(trueBiomes);
					continue;
				}
				// Ensure the two match.
				bool identical = true;
				// NOTE: Temporary workaround to handle fact that AddIslandLayer requires coordinates outside desired region
				for (int z = configuration.minimumZ + requiredMargin; z <= configuration.maximumZ - requiredMargin; ++z) {
					for (int x = configuration.minimumX + requiredMargin; x <= configuration.maximumX - requiredMargin; ++x) {
						if (emulatedBiomes[flatten(x, z, &configuration)] != trueBiomes[flatten(x, z, &configuration)]) identical = false;
					}
				}
				if (identical) {
				// if (!memcmp(trueBiomes, emulatedBiomes, configuration.width*configuration.height*sizeof(*emulatedBiomes))) {
					free(emulatedBiomes);
					free(trueBiomes);
					continue;
				}
				// If they don't match, print warning
				printf("%s%s, layer %s: Emulated biomes DO NOT match true biomes.\n", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
				char filepath[500];
				// Save emulated biomes as image
				snprintf(filepath, sizeof(filepath)/sizeof(*filepath), "./Emulation Maps/%s%s %s Emulated Biomes.ppm", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
				errorCode = saveAsImage(&configuration, emulatedBiomes, filepath);
				if (errorCode) printf("Error: saveAsImage(emulatedBiomes) returned non-zero error code %d.\n", errorCode);
				// Save true biomes as image
				snprintf(filepath, sizeof(filepath)/sizeof(*filepath), "./Emulation Maps/%s%s %s True Biomes.ppm", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
				errorCode = saveAsImage(&configuration, trueBiomes, filepath);
				if (errorCode) printf("Error: saveAsImage(trueBiomes) returned non-zero error code %d.\n", errorCode);
				// Save difference as image
				for (size_t i = 0; i < cacheSize; ++i) emulatedBiomes[i] = (emulatedBiomes[i] == trueBiomes[i]);
				snprintf(filepath, sizeof(filepath)/sizeof(*filepath), "./Emulation Maps/%s%s %s Difference.ppm", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
				errorCode = saveAsImage(&configuration, emulatedBiomes, filepath);
				if (errorCode) printf("Error: saveAsImage(Difference) returned non-zero error code %d.\n", errorCode);

				free(emulatedBiomes);
				free(trueBiomes);
				continue;
			}
		}
	}
	printf("All versions checked.\n");
	return 0;
}