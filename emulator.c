#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "cubiomes/generator.h"
#include "cubiomes/util.h"
#include "layerutil.h"

const uint64_t SUPPORTED_LAYERS = (UINT64_C(1) << L_CONTINENT_4096)
	| (UINT64_C(1) << L_ZOOM_4096)
	| (UINT64_C(1) << L_LAND_4096)
	| (UINT64_C(1) << L_ZOOM_2048)
	| (UINT64_C(1) << L_LAND_2048)
	| (UINT64_C(1) << L_ZOOM_1024)
	| (UINT64_C(1) << L_LAND_1024_A);

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


void islandLayer(int *const biomes, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);
	
	for (int z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];
			// - (0, 0) (when scaled) always is set to non-ocean
			if (!x && !z) *entry = 1;
			// Otherwise we roll a 1/10 chance
			else {
				uint64_t random = getChunkSeed(startSeed, x, z);
				*entry = !quadraticNextInt(&random, 0, 10); // Since only one call is ever done, the start salt does not matter
			}
		}
	}
}

void zoomLayer(int *const biomes, int *const tempBuffer, bool fuzzy, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSalt = getStartSalt(configuration->worldseed, layerSalt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);
	
	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			bool xOdd = x & 1, zOdd = z & 1;
			int northwestValue = biomes[flatten(x >> 1, z >> 1, configuration)];
			// Even coordinates on both axes use the Northwest value
			if (!xOdd && !zOdd) {
				*entry = northwestValue;
				continue;
			}
			// Even x, odd z coordinates randomly choose between the Northwest and Southwest values
			uint64_t random = getChunkSeed(startSeed, x & ~1, z & ~1);
			int southwestValue = biomes[flatten(x >> 1, (z + 1) >> 1, configuration)];
			int selection = quadraticNextInt(&random, startSalt, 2) ? southwestValue : northwestValue;
			if (!xOdd && zOdd) {
				*entry = selection;
				continue;
			}
			// Odd x, even z coordinates randomly choose between the Northwest and Northeast values
			int northeastValue = biomes[flatten((x + 1) >> 1, z >> 1, configuration)];;
			selection = quadraticNextInt(&random, startSalt, 2) ? northeastValue : northwestValue;
			if (xOdd && !zOdd) {
				*entry = selection;
				continue;
			}
			// Odd x, odd z coordinates...
			int southeastValue = biomes[flatten((x + 1) >> 1, (z + 1) >> 1, configuration)];
			// If not using fuzzy zooming, try to pick whichever value is in the majority
			if (!fuzzy) {
				// If 3 of the 4 points agree, pick the majority value
				if (northeastValue == southwestValue && northeastValue == southeastValue) {
					*entry = northeastValue;
					continue;
				}
				if (northwestValue == northeastValue && northwestValue == southwestValue) {
					*entry = northwestValue;
					continue;
				}
				if (northwestValue == northeastValue && northwestValue == southeastValue) {
					*entry = northwestValue;
					continue;
				}
				if (northwestValue == southwestValue && northwestValue == southeastValue) {
					*entry = northwestValue;
					continue;
				}
				// If two adjacent points agree, and the opposite two disagree, pick the adjacent values
				if (northwestValue == northeastValue && southwestValue != southeastValue) {
					*entry = northwestValue;
					continue;
				}
				if (northwestValue == southwestValue && northeastValue != southeastValue) {
					*entry = northwestValue;
					continue;
				}
				if (northwestValue == southeastValue && northeastValue != southwestValue) {
					*entry = northwestValue;
					continue;
				}
				if (northeastValue == southwestValue && northwestValue != southeastValue) {
					*entry = northeastValue;
					continue;
				}
				if (northeastValue == southeastValue && northwestValue != southwestValue) {
					*entry = northeastValue;
					continue;
				}
				if (southwestValue == southeastValue && northwestValue != northeastValue) {
					*entry = southwestValue;
					continue;
				}
			}
			// Otherwise, choose randomly
			switch (quadraticNextInt(&random, startSalt, 4)) {
				case 0:
					*entry = northwestValue;
					continue;
				case 1:
					*entry = northeastValue;
					continue;
				case 2:
					*entry = southwestValue;
					continue;
				default:
					*entry = southeastValue;
					continue;
			}
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

void addIslandLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSalt = getStartSalt(configuration->worldseed, layerSalt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);
	
	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 1; z <= configuration->maximumZ - 1; ++z) {
		for (int64_t x = configuration->minimumX + 1; x <= configuration->maximumX - 1; ++x) {
			// Sampling
			// --------
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int southwestValue = biomes[flatten(x - 1, z + 1, configuration)];
			int southeastValue = biomes[flatten(x + 1, z + 1, configuration)];
			int northeastValue = biomes[flatten(x + 1, z - 1, configuration)];
			int northwestValue = biomes[flatten(x - 1, z - 1, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];
			
			// If the center value is shallow ocean, and one of the four corner biomes isn't:
			if (shallowOceanCheck(centerValue, configuration->version) && (!shallowOceanCheck(southwestValue, configuration->version) || !shallowOceanCheck(southeastValue, configuration->version) || !shallowOceanCheck(northwestValue, configuration->version) || !shallowOceanCheck(northeastValue, configuration->version))) {
				uint64_t random = getChunkSeed(startSeed, x, z);
				// For Beta 1.8, roll 2/3rd chance to preserve ocean, otherwise change to land
				if (configuration->version == MC_B1_8) {
					*entry = (quadraticNextInt(&random, startSalt, 3) == 2);
					continue;
				}
				// Otherwise randomly choose a land corner to potentially replace it
				int landBiomesFoundCount = 0, potentialReplacement = plains;
				if (!shallowOceanCheck(northwestValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = northwestValue;
				if (!shallowOceanCheck(northeastValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = northeastValue;
				if (!shallowOceanCheck(southwestValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = southwestValue;
				if (!shallowOceanCheck(southeastValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = southeastValue;
				// Roll a 1/3 chance to replace it
				if (!quadraticNextInt(&random, startSalt, 3)) {
					*entry = potentialReplacement;
					continue;
				}
				// Version-specific guaranteed replacements
				if (MC_1_0 <= configuration->version && configuration->version <= MC_1_6 && potentialReplacement == snowy_tundra) {
					*entry = frozen_ocean;
					continue;
				}
				if (configuration->version >= MC_1_7 && potentialReplacement == forest) {
					*entry = forest;
					continue;
				}
				// Otherwise default to center value
				*entry = centerValue;
				continue;
			}
			// Otherwise if the center value isn't shallow ocean, one of the four corner biomes is shallow ocean, and a 1/5 chance occurs:
			if (!shallowOceanCheck(centerValue, configuration->version) && (shallowOceanCheck(southwestValue, configuration->version) || shallowOceanCheck(southeastValue, configuration->version) || shallowOceanCheck(northwestValue, configuration->version) || shallowOceanCheck(northeastValue, configuration->version))) {
				uint64_t random = getChunkSeed(startSeed, x, z);
				// For Beta 1.8, roll 4/5th chance to preserve land, otherwise change to ocean
				if (configuration->version == MC_B1_8) {
					*entry = (quadraticNextInt(&random, startSalt, 5) != 4);
					continue;
				}
				// Otherwise (1.0+), roll 4/5 chance to ignore rest
				if (quadraticNextInt(&random, startSalt, 5)) {
					*entry = centerValue;
					continue;
				}
				// Otherwise immediate change in 1.0-1.6
				if (MC_1_0 <= configuration->version && configuration->version <= MC_1_6) {
					*entry = (centerValue == snowy_tundra ? frozen_ocean : ocean);
					continue;
				}
				// 1.7+
				if (centerValue == forest) {
					*entry = forest;
					continue;
				}
				if (shallowOceanCheck(northwestValue, configuration->version)) {
					*entry = northwestValue;
					continue;
				}
				if (shallowOceanCheck(southwestValue, configuration->version)) {
					*entry = southwestValue;
					continue;
				}
				if (shallowOceanCheck(northeastValue, configuration->version)) {
					*entry = northeastValue;
					continue;
				}
				if (shallowOceanCheck(southeastValue, configuration->version)) {
					*entry = southeastValue;
					continue;
				}
			}
			// Otherwise keep the center value unchanged
			*entry = centerValue;
			continue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

int emulateBiomes(const Configuration *const configuration, int *const biomes, size_t biomesCapacity) {
	if (!configuration) return 1;
	if (!biomes) return 2;
	if (configuration->startingLayerID >= L_NUM) return 3;
	if (biomesCapacity < (size_t)configuration->width * (size_t)configuration->height) return 4;

	int *const tempBuffer = (int *const)calloc(configuration->width * configuration->height, sizeof(*tempBuffer));

	// Island: 1:8192 for Beta 1.8, 1:4096 for 1.0+
	// 0 = ocean, 1 = non-ocean
	islandLayer(biomes, 1, configuration);
	if (configuration->startingLayerID == L_CONTINENT_4096) {
		free(tempBuffer);
		return 0;
	}

	// Fuzzy Zoom: 1:4096 for Beta 1.8, 1:2048 for 1.0+
	// 0 = ocean, 1 = non-ocean
	zoomLayer(biomes, tempBuffer, true, 2000, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_4096 : L_ZOOM_2048)) {
		free(tempBuffer);
		return 0;
	}

	// Add Island: 1:4096 for Beta 1.8, 1:2048 for 1.0+
	// 0 = ocean, 1 = non-ocean
	addIslandLayer(biomes, tempBuffer, 1, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_4096 : L_LAND_2048)) {
		free(tempBuffer);
		return 0;
	}

	// Zoom: 1:2048 for Beta 1.8, 1:1024 for 1.0+
	// 0 = ocean, 1 = non-ocean
	zoomLayer(biomes, tempBuffer, false, 2001, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_2048 : L_ZOOM_1024)) {
		free(tempBuffer);
		return 0;
	}

	// Add Island: 1:2048 for Beta 1.8, 1:1024 for 1.0+
	// 0 = ocean, 1 = non-ocean
	addIslandLayer(biomes, tempBuffer, 2, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_2048 : L_LAND_1024_A)) {
		free(tempBuffer);
		return 0;
	}

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
	if (scale > 256) scale = 256;
	if (!scale) return 4;
	if (biomesCapacity < getMinCacheSize(&g, scale, configuration->width, 1, configuration->height)) return 5;

	if (genArea(&g.ls.layers[configuration->startingLayerID], biomes, configuration->minimumX, configuration->minimumZ, configuration->width, configuration->height)) return 6;
	return 0;
}

int main() {
	Configuration configuration = {
		0,
		8675309,
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
				if (scale > 256) scale = 256;
				size_t cacheSize = getMinCacheSize(&g, scale, configuration.width, 1, configuration.height);
				int *const emulatedBiomes = (int *const)calloc(cacheSize, sizeof(*emulatedBiomes));
				int *const trueBiomes = (int *const)calloc(cacheSize, sizeof(*trueBiomes));

				// Fill emulated array
				int errorCode;
				errorCode = emulateBiomes(&configuration, emulatedBiomes, cacheSize);
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
				for (int z = configuration.minimumZ + 1; z <= configuration.maximumZ - 1; ++z) {
					for (int x = configuration.minimumX + 1; x <= configuration.maximumX - 1; ++x) {
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
	return 0;
}