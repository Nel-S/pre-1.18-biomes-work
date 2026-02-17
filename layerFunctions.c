#include <string.h>
#include "layerFunctions.h"

// From Cubiomes
static inline int isAny4(int biomeID, int a, int b, int c, int d) {
	return biomeID == a || biomeID == b || biomeID == c || biomeID == d;
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
			if (!x && !z) *entry = Warm;
			// Otherwise we roll a 1/10 chance
			else {
				uint64_t random = getChunkSeed(startSeed, x, z);
				*entry = !quadraticNextInt(&random, 0, 10); // Last call, so start salt does not matter
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
			switch (quadraticNextInt(&random, 0, 4)) { // Last call, so start salt does not matter
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
					*entry = (quadraticNextInt(&random, 0, 3) == 2); // Last call, so start salt does not matter
					continue;
				}
				// Otherwise randomly choose a land corner to potentially replace it
				int landBiomesFoundCount = 0, potentialReplacement = Warm;
				if (!shallowOceanCheck(northwestValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = northwestValue;
				if (!shallowOceanCheck(northeastValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = northeastValue;
				if (!shallowOceanCheck(southwestValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = southwestValue;
				if (!shallowOceanCheck(southeastValue, configuration->version) && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = southeastValue;
				// Roll a 1/3 chance to replace it
				if (!quadraticNextInt(&random, 0, 3)) { // Last call, so start salt does not matter
					*entry = potentialReplacement;
					continue;
				}
				// Version-specific guaranteed replacements
				if (MC_1_0 <= configuration->version && configuration->version <= MC_1_6 && potentialReplacement == snowy_tundra) {
					*entry = frozen_ocean;
					continue;
				}
				if (configuration->version >= MC_1_7 && potentialReplacement == Freezing) {
					*entry = Freezing;
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
					*entry = (quadraticNextInt(&random, 0, 5) != 4); // Last call, so start salt does not matter
					continue;
				}
				// Otherwise (1.0+), roll 4/5 chance to ignore rest
				if (quadraticNextInt(&random, 0, 5)) { // Last call, so start salt does not matter
					*entry = centerValue;
					continue;
				}
				// Otherwise immediate change in 1.0-1.6
				if (MC_1_0 <= configuration->version && configuration->version <= MC_1_6) {
					*entry = (centerValue == snowy_tundra ? frozen_ocean : ocean);
					continue;
				}
				// In 1.7+, freezing biomes are preserved
				if (centerValue == Freezing) {
					*entry = Freezing;
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

void removeTooMuchOceanLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);
	
	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 1; z <= configuration->maximumZ - 1; ++z) {
		for (int64_t x = configuration->minimumX + 1; x <= configuration->maximumX - 1; ++x) {
			// Sampling
			// --------
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int northValue = biomes[flatten(x, z - 1, configuration)];
			int eastValue = biomes[flatten(x + 1, z, configuration)];
			int southValue = biomes[flatten(x, z + 1, configuration)];
			int westValue = biomes[flatten(x - 1, z, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];
			
			// If the coordinate is not ocean surrounded on all orthagonal sides by ocean, leave alone
			if (!shallowOceanCheck(centerValue, configuration->version) || !shallowOceanCheck(northValue, configuration->version) || !shallowOceanCheck(eastValue, configuration->version) || !shallowOceanCheck(westValue, configuration->version) || !shallowOceanCheck(southValue, configuration->version)) {
				*entry = centerValue;
				continue;
			}
			// Otherwise replace with non-ocean with 1/2 chance (otherwise leave alone)
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (!quadraticNextInt(&random, 0, 2)) *entry = Warm; // Last call, so start salt does not matter
			else *entry = centerValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

void addSnowLayer(int *const biomes, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);
	
	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];

			// Oceans are preserved
			if (shallowOceanCheck(*entry, configuration->version)) continue;
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (configuration->version <= MC_1_6) {
				// In 1.0-1.6, 1/5 chance is rolled
				if (!quadraticNextInt(&random, 0, 5)) *entry = snowy_tundra; // Last call, so start salt does not matter
				else *entry = plains;
			} else {
				// In 1.7+, 1/6 chance to set to Freezing, 1/6 chance to set to Cold; rest remain Warm
				switch (quadraticNextInt(&random, 0, 6)) { // Last call, so start salt does not matter
					case 0:
						*entry = Freezing;
						continue;
					case 1:
						*entry = Cold;
						continue;
					default:
						*entry = Warm;
						continue;
				}
			}
		}
	}			
}

void addEdgeLayerCoolWarm(int *const biomes, int *const tempBuffer, const Configuration *const configuration) {

	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 1; z <= configuration->maximumZ - 1; ++z) {
		for (int64_t x = configuration->minimumX + 1; x <= configuration->maximumX - 1; ++x) {
			// Sampling
			// --------
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int northValue = biomes[flatten(x, z - 1, configuration)];
			int eastValue = biomes[flatten(x + 1, z, configuration)];
			int southValue = biomes[flatten(x, z + 1, configuration)];
			int westValue = biomes[flatten(x - 1, z, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];
			
			// If the coordinate is Warm, and orthagonally bordered by any Cold or Freezing, switch to Lush. Otherwise leave alone
			if (centerValue == Warm && (isAny4(Cold, eastValue, westValue, southValue, northValue) || isAny4(Freezing, northValue, eastValue, westValue, southValue))) {
				*entry = Lush;
				continue;
			} else *entry = centerValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

void addEdgeLayerHeatIce(int *const biomes, int *const tempBuffer, const Configuration *const configuration) {

	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 1; z <= configuration->maximumZ - 1; ++z) {
		for (int64_t x = configuration->minimumX + 1; x <= configuration->maximumX - 1; ++x) {
			// Sampling
			// --------
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int northValue = biomes[flatten(x, z - 1, configuration)];
			int eastValue = biomes[flatten(x + 1, z, configuration)];
			int southValue = biomes[flatten(x, z + 1, configuration)];
			int westValue = biomes[flatten(x - 1, z, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];
			
			// If the coordinate is Freezing, and orthagonally bordered by any Warm or Lush, switch to Cold. Otherwise leave alone
			if (centerValue == Freezing && (isAny4(Warm, northValue, eastValue, westValue, southValue) || isAny4(Lush, northValue, eastValue, westValue, southValue))) {
				*entry = Cold;
				continue;
			} else *entry = centerValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

void addEdgeLayerIntroduceSpecial(int *const biomes, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSalt = getStartSalt(configuration->worldseed, layerSalt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];
			// Oceans are left alone
			if (shallowOceanCheck(*entry, configuration->version)) continue;

			// 12/13 chance of doing nothing
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (quadraticNextInt(&random, startSalt, 13)) continue;
			
			// TODO: The exact nextInt value may actually be unnecessary
			*entry |= (256*(1 + quadraticNextInt(&random, startSalt, 15))) & 0xf00; // Last call, so start salt does not matter
			
		}
	}
}

void addMushroomIslandLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
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
			
			// If the center value, or any of the immediate diagonals, are not shallow ocean, preserve center value
			if (!shallowOceanCheck(centerValue, configuration->version) || !shallowOceanCheck(southwestValue, configuration->version) || !shallowOceanCheck(southeastValue, configuration->version) || !shallowOceanCheck(northwestValue, configuration->version) || !shallowOceanCheck(northeastValue, configuration->version)) {
				*entry = centerValue;
				continue;
			}
			// Roll 1/100 chance of replacement
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (!quadraticNextInt(&random, 0, 100)) { // Last call, so start salt does not matter
				*entry = mushroom_fields;
				continue;
			}
			// Otherwise keep the center value unchanged
			*entry = centerValue;
			continue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

void addDeepOceanLayer(int *const biomes, int *const tempBuffer, const Configuration *const configuration) {
	
	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 1; z <= configuration->maximumZ - 1; ++z) {
		for (int64_t x = configuration->minimumX + 1; x <= configuration->maximumX - 1; ++x) {
			// Sampling
			// --------
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int northValue = biomes[flatten(x, z - 1, configuration)];
			int eastValue = biomes[flatten(x + 1, z, configuration)];
			int southValue = biomes[flatten(x, z + 1, configuration)];
			int westValue = biomes[flatten(x - 1, z, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];
			
			// If the coordinate is not shallow ocean orthagonally bordered by shallow oceans on all sides, preserve its value
			if (!shallowOceanCheck(centerValue, configuration->version) || !shallowOceanCheck(northValue, configuration->version) || !shallowOceanCheck(eastValue, configuration->version) || !shallowOceanCheck(westValue, configuration->version) || !shallowOceanCheck(southValue, configuration->version)) {
				*entry = centerValue;
				continue;
			}

			// Otherwise replace ocean with deep equivalent
			switch (centerValue) {
				case warm_ocean:
					*entry = deep_warm_ocean;
					continue;
				case lukewarm_ocean:
					*entry = deep_lukewarm_ocean;
					continue;
				case cold_ocean:
					*entry = deep_cold_ocean;
					continue;
				case frozen_ocean:
					*entry = deep_frozen_ocean;
					continue;
				default:
					*entry = deep_ocean;
					continue;
			}
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

void biomeInitLayer(int *const biomes, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];
			// Normal oceans in 1.6-, all oceans in 1.7+, and mushroom fields are left alone
			if (configuration->version <= MC_1_6 ? *entry == ocean : isOceanic(*entry)) continue;
			if (*entry == mushroom_fields) continue;
			
			uint64_t random = getChunkSeed(startSeed, x, z);
			// Depending on the biome temperature:
			switch (*entry & ~0xf00) {
				case Warm:
					// Warm special biomes (1.7+ only) are 1/3 badlands plateau, 2/3 wooded badlands plateau
					if (*entry & 0xf00) {
						if (!quadraticNextInt(&random, 0, 3)) *entry = badlands_plateau; // Last call, so start salt does not matter
						else *entry = wooded_badlands_plateau;
						continue;
					}
					// Beta 1.8 has all biomes here, 1.0-1.6 all non-freezing biomes, 1.7+ all warm normal biomes.
					switch (quadraticNextInt(&random, 0, 6 + (configuration->version >= MC_1_2 && configuration->version <= MC_1_6))) {
						case 0:
							*entry = desert;
							continue;
						case 1:
							if (configuration->version <= MC_1_6) *entry = forest;
							else *entry = desert;
							continue;
						case 2:
							if (configuration->version <= MC_1_6) *entry = mountains;
							else *entry = desert;
							continue;
						case 3:
							if (configuration->version <= MC_1_6) *entry = swamp;
							else *entry = savanna;
							continue;
						case 4:
							if (configuration->version <= MC_1_6) *entry = plains;
							else *entry = savanna;
							continue;
						case 5:
							if (configuration->version <= MC_1_6) *entry = taiga;
							else *entry = plains;
							continue;
						default:
							*entry = jungle;
							continue;	
					}
				case Lush: // 1.7+
					// Lush special biomes biomes are jungles
					if (*entry & 0xf00) {
						*entry = jungle;
						continue;
					}
					switch (quadraticNextInt(&random, 0, 6)) { // Last call, so start salt does not matter
						case 0:
							*entry = forest;
							continue;
						case 1:
							*entry = dark_forest;
							continue;
						case 2:
							*entry = mountains;
							continue;
						case 3:
							*entry = plains;
							continue;
						case 4:
							*entry = birch_forest;
							continue;
						default:
							*entry = swamp;
							continue;
					}
				case Cold: // 1.7+
					// All cold special biomes are giant tree taigas
					if (*entry & 0xf00) {
						*entry = giant_tree_taiga;
						continue;
					}
					switch (quadraticNextInt(&random, 0, 4)) { // Last call, so start salt does not matter
						case 0:
							*entry = forest;
							continue;
						case 1:
							*entry = mountains;
							continue;
						case 2:
							*entry = taiga;
							continue;
						default:
							*entry = plains;
							continue;
					}
				default: // These are IDs 10 (frozen ocean) or 12 (snowy tundra) for 1.6-, or ID 4 (Freezing) for 1.7+.
					// 1.3-1.6 has 1/7th chance of placing taigas
					if (configuration->version >= MC_1_3 && configuration->version <= MC_1_6 && quadraticNextInt(&random, 0, 7) == 5) { // Last call, so start salt does not matter
						*entry = taiga;
						continue;
					}
					// 1.7+ has 1/4th chance of placing snowy taigas
					if (configuration->version >= MC_1_7 && quadraticNextInt(&random, 0, 4) == 3) { // Last call, so start salt does not matter
						*entry = snowy_taiga;
						continue;
					}
					// All others are snowy tundras
					*entry = snowy_tundra;
					continue;
			}
		}
	}
}

void addBambooLayer(int *const biomes, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];
			// Everything besides jungles are left alone
			if (*entry != jungle) continue;

			// 9/10ths chance of doing nothing
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (quadraticNextInt(&random, 0, 10)) continue; // Last call, so start salt does not matter

			// Otherwise replace with bamboo jungle
			*entry = bamboo_jungle;	
		}
	}
}

void riverInitLayer(int *const rivers, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &rivers[flatten(x, z, configuration)];
			// Oceans are left alone
			if (shallowOceanCheck(*entry, configuration->version)) continue;

			uint64_t random = getChunkSeed(startSeed, x, z);
			// 1.6- rolls a 1/2 chance
			if (configuration->version <= MC_1_6) *entry = 2 + quadraticNextInt(&random, 0, 2); // Last call, so start salt does not matter
			// 1.7+ instead rolls a... 1/299999 chance?
			else *entry = 2 + quadraticNextInt(&random, 0, 299999); // Last call, so start salt does not matter
		}
	}
}