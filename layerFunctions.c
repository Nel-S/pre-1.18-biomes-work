#include <string.h>
#include "layerFunctions.h"

// From Cubiomes
static inline int isAny4(int biomeID, int a, int b, int c, int d) {
	return biomeID == a || biomeID == b || biomeID == c || biomeID == d;
}

// All versions. One-to-one.
// (0, 0) is set to Warm/Plains. Every other coordinate is set to Warm/Plains with 1/10th chance, or remains Ocean otherwise.
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

// 1.1-1.12 1:64 Hills and 1:128 Hills (saltless); all other versions/layers. Halves.
// - (Even, even) coordinates are set to (even/2, even/2).
// - (Even, odd) coordinates randomly choose between (even/2, odd//2) and (even/2, (odd + 1)//2).
// - (Odd, even) coordinates waste a roll, then randomly choose between (odd//2, even/2) and ((odd + 1)//2, even/2).
// - (Odd, odd) coordinates waste 2 rolls. Then
// 		- if not fuzzy, and any two of (odd//2, odd//2), (odd//2, (odd + 1)//2), ((odd + 1)//2, odd//2), and ((odd + 1)//2, (odd + 1)//2) match while the other two don't, pick that value.
// 		- otherwise randomly choose one between them.
void zoomLayer(int *const biomes, int *const tempBuffer, bool fuzzy, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	// Worldseed-independent for 1.1-1.12- 1:128 and 1:64 Hills...
	uint64_t startSalt = salt ? getStartSalt(configuration->worldseed, layerSalt) : 0;
	uint64_t startSeed = salt ? getStartSeed(configuration->worldseed, layerSalt) : 0;
	
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
				// If two points agree, and the other two disagree, pick the values in agreement
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

// Beta 1.8; 1.0-1.6; 1.7+. Bishop.
//	- Beta 1.8:
// 		- If the coordinate is Ocean, and any neighboring diagonal isn't, roll 1/3rd chance to replace with Plains.
//		- If the coordinate is Plains, and any neighboring diagonal isn't, roll 1/5th chance to replace with Ocean.
//	- 1.0-1.6:
//		- If the coordinate is Ocean, and any neighboring diagonal isn't, randomly select a non-Ocean diagonal and roll 1/3rd chance to replace it. If the roll failed but the replacement would have been a Snowy Tundra, replace with Frozen Ocean.
//		- If the coordinate is not Ocean, any neighboring diagonal is, and a 1/5th chance roll succeeds, replace with Frozen Ocean (if originally a Snowy Tundra) or Ocean otherwise.
//	- 1.7+:
//		- If the coordinate is shallow ocean, and any neighboring diagonal isn't, randomly select a non-shallow-ocean diagonal and roll 1/3rd chance to replace it. If the roll failed but the replacement would have been Freezing/Forest, replace with Freezing/Forest.
//		- If the coordinate is not shallow ocean or Freezing/Forest, any neighboring diagonal is shallow ocean, and a 1/5th chance roll succeeds, replace with whichever of Northwest, Southwest, Northeast, and Southeast is first shallow ocean.
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
			
			uint64_t random = getChunkSeed(startSeed, x, z);
			// If the center value is shallow ocean, and one of the four corner biomes isn't:
			if (shallowOceanCheck(centerValue, configuration->version) && (!shallowOceanCheck(southwestValue, configuration->version) || !shallowOceanCheck(southeastValue, configuration->version) || !shallowOceanCheck(northwestValue, configuration->version) || !shallowOceanCheck(northeastValue, configuration->version))) {
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
			// In Beta 1.8, if the center value is plains and one of the four corner biomes is not plains:
			if (configuration->version == MC_B1_8 && (centerValue == plains) && (
				(southwestValue != plains) || (southeastValue != plains) || (northwestValue != plains) || (northeastValue != plains)
			)) {
				// For Beta 1.8, roll 4/5th chance to preserve plains, otherwise change to ocean
				*entry = (quadraticNextInt(&random, 0, 5) != 4); // Last call, so start salt does not matter
					continue;
			}
			// In 1.0+, if the center value isn't shallow ocean, one of the four corner biomes is shallow ocean, and a 1/5 chance occurs:
			if (configuration->version >= MC_1_0 && !shallowOceanCheck(centerValue, configuration->version) && (shallowOceanCheck(southwestValue, configuration->version) || shallowOceanCheck(southeastValue, configuration->version) || shallowOceanCheck(northwestValue, configuration->version) || shallowOceanCheck(northeastValue, configuration->version)) && !quadraticNextInt(&random, 0, 5)) { // Last call, so start salt does not matter
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
					*entry = southeastValue;
					continue;
			}
			// Otherwise keep the center value unchanged
			*entry = centerValue;
			continue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// 1.7+. Castle.
// If coordinate is Ocean surrounded orthagonally by Ocean, and a 1/2th chance succeeeds, replace with Warm.
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
			if (centerValue != ocean || northValue != ocean || eastValue != ocean || westValue != ocean || southValue != ocean) {
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

// 1.0-1.6; 1.7+. One-to-one.
//	- 1.0-1.6:
//		- If coordinate is not Ocean, and a 1/5th chance succeeds, replace with Snowy Tundra.
//	- 1.7+:
//		- If coordinate is not Ocean, roll a (1 + 1)/6th chance to replace with Freezing or Cold, respectively.
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
			if (*entry == ocean) continue;
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (configuration->version <= MC_1_6) {
				// In 1.0-1.6, 1/5 chance is rolled
				if (!quadraticNextInt(&random, 0, 5)) *entry = snowy_tundra; // Last call, so start salt does not matter
			} else {
				// In 1.7+, 1/6 chance to set to Freezing, 1/6 chance to set to Cold
				switch (quadraticNextInt(&random, 0, 6)) { // Last call, so start salt does not matter
					case 0:
						*entry = Freezing;
						continue;
					case 1:
						*entry = Cold;
						continue;
				}
			}
		}
	}			
}

// 1.7+. Castle.
// If coordinate is Warm orthagonally bordered by any Cold or Freezing, replace with Lush.
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

// 1.7+. Castle.
// If coordinate is Freezing orthagonally bordered by any Warm or Lush, replace with Cold.
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

// 1.7+. One-to-one.
// If coordinate is not Ocean and a 1/13 chance succeeds, mark as Special (with randomized 4-bit tag; 1/15th chance of each).
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
			if (*entry == ocean) continue;

			// 12/13 chance of doing nothing
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (quadraticNextInt(&random, startSalt, 13)) continue;
			
			*entry |= (256*(1 + quadraticNextInt(&random, 0, 15))); // Last call, so start salt does not matter
		}
	}
}

// 1.0+. Bishop.
// If coordinate is Ocean surrounded diagonally by Ocean, roll 1/100th chance to replace with Mushroom Fields.
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
			
			// If the center value, or any of the immediate diagonals, are not Ocean, preserve center value
			if (centerValue != ocean || southwestValue != ocean ||southeastValue != ocean || northwestValue != ocean || northeastValue != ocean) {
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

// 1.7+. Castle.
// If coordinate is Ocean surrounded orthagonally by Ocean, replace with Deep Ocean.
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
			
			// If the coordinate is not Ocean orthagonally bordered by Oceans on all sides, preserve its value
			if (centerValue != ocean || northValue != ocean || eastValue != ocean || westValue != ocean || southValue != ocean) {
				*entry = centerValue;
				continue;
			}

			// Otherwise replace ocean with deep equivalent
					*entry = deep_ocean;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// Beta 1.8; 1.0-1.1; 1.2; 1.3-1.6; 1.7+. One-to-one.
// - Beta 1.8:
//		- Plains are (1+1+1+1+1+1)/6th replaced with Deserts, Forests, Mountains, Swamps, Plains, and Taigas, respectively.
// - 1.0-1.1:
//		- Plains are (1+1+1+1+1+1)/6th replaced with Deserts, Forests, Mountains, Swamps, Plains, and Taigas, respectively.
//		- Frozen Oceans are replaced with Snowy Tundras.
// - 1.2:
//		- Plains are (1+1+1+1+1+1+1)/7th replaced with Deserts, Forests, Mountains, Swamps, Plains, Taigas, and Jungles, respectively.
//		- Frozen Oceans are replaced with Snowy Tundras.
// - 1.3-1.6:
//		- Plains are (1+1+1+1+1+1+1)/7th replaced with Deserts, Forests, Mountains, Swamps, Plains, Taigas, and Jungles, respectively.
//		- Frozen Oceans and Snowy Tundras are 1/7th replaced with Taigas, otherwise Snowy Tundras.
// - 1.7+:
//		- Warm is (3+2+1)/6th replaced with Deserts, Savannas, and Plains, respectively.
//		- Warm Special is 1/3rd replaced with Badlands Plateau, otherwise Wooded Badlands Plateau.
//		- Lush is (1+1+1+1+1+1)/6th replaced with Forests, Dark Forests, Mountains, Plains, Birch Forests, and Swamps, respectively.
//		- Lush Special is replaced with Jungles.
//		- Cold is (1+1+1+1)/4th replaced with Forests, Mountains, Taigas, and Plains.
//		- Cold Special is replaced with Giant Tree Taigas.
//		- Freezing and Freezing Special are 1/4th replaced by Snowy Taigas, otherwise Snowy Tundras.
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
			// Oceans, Deep Oceans, and Mushroom Fields are left alone
			if (*entry == ocean || *entry == deep_ocean || *entry == mushroom_fields) continue;
			
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

// 1.14+. One-to-one.
// If coordinate is Jungle, roll 1/10th chance of replacing with Bamboo Jungle.
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

// 1.7+. Castle.
// Coordinates that are a Badlands Plateau/Wooded Badlands Plateau not surrounded orthagonally by Badlands Plateaus/Wooded Badlands Plateaus are replaced with Badlands.
// Coordinates that are a Giant Tree Taiga not surrounded orthagonally by Taigas, Snowy Taigas, or Giant Tree Taigas are replaced with Taigas.
// Coordinates that are a Desert orthagonally bordering a Snowy Tundra are replaced with Wooded Mountains.
// Coordinates that are a Swamp orthagonally bordering a Desert, Snowy Taiga, or Snowy Tundra are replaced with Plains.
// Coordinates that are a Swamp orthagonally bordering a Jungle or Bamboo Jungle are replaced with Jungle Edge.
void biomeEdgeLayer(int *const biomes, int *const tempBuffer, const Configuration *const configuration) {
	
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
			
			// If the coordinate is a badlands plateau/wooded badlands plateau that's not orthagonally surrounded by badlands plateaus/wooded badlands plateaus, replace with badlands
			if ((centerValue == badlands_plateau || centerValue == wooded_badlands_plateau) && (
				(northValue != badlands_plateau && northValue != wooded_badlands_plateau) ||
				(eastValue  != badlands_plateau && eastValue  != wooded_badlands_plateau) ||
				(westValue  != badlands_plateau && westValue  != wooded_badlands_plateau) ||
				(southValue != badlands_plateau && southValue != wooded_badlands_plateau)
			)) {
				*entry = badlands;
				continue;
			}
			// If the coordinate is a giant tree taiga that's not orthagonally surrounded by taiga-category biomes, replace with taiga
			if (centerValue == giant_tree_taiga && (
				getCategory(configuration->version, northValue) != taiga ||
				getCategory(configuration->version, eastValue ) != taiga ||
				getCategory(configuration->version, westValue ) != taiga ||
				getCategory(configuration->version, southValue) != taiga
			)) {
				*entry = taiga;
				continue;
			}
			// If the coordinate is a desert orthagonally bordering a snowy tundra, replace with wooded mountains
			if (centerValue == desert && isAny4(snowy_tundra, northValue, eastValue, westValue, southValue)) {
				*entry = wooded_mountains;
				continue;
			}
			// If the coordinate is a swamp...
			if (centerValue == swamp) {
				// ...orthagonally bordering a desert, snowy taiga, or snowy tundra, replace with plains
				if (isAny4(desert, northValue, eastValue, westValue, southValue) || isAny4(snowy_taiga, northValue, eastValue, westValue, southValue) || isAny4(snowy_tundra, northValue, eastValue, westValue, southValue)) {
					*entry = plains;
					continue;
				}
				// ...orthagonally bordering a jungle or bamboo jungle, replace with jungle edge
				if (isAny4(jungle, northValue, southValue, eastValue, westValue) || isAny4(bamboo_jungle, northValue, southValue, eastValue, westValue)) {
					*entry = jungle_edge;
					continue;
				} 
			}
			// Otherwise preserve the original value
			*entry = centerValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// Beta 1.8 - 1.6; 1.7+
// One-to-one
void riverInitLayer(int *const riverNoise, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &riverNoise[flatten(x, z, configuration)];
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

// 1.1-1.6 (hillsNoise unused); 1.7-1.8/1.11+; 1.9-1.10
// Castle
void regionHillsLayer(int *const biomes, const int *const hillsNoise, int *const tempBuffer, uint64_t salt, const Configuration *const configuration) {
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
			int northValue = biomes[flatten(x, z - 1, configuration)];
			int eastValue = biomes[flatten(x + 1, z, configuration)];
			int southValue = biomes[flatten(x, z + 1, configuration)];
			int westValue = biomes[flatten(x - 1, z, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];

			// 1.7+ has two possible conditions under which it can mutate the replacement biome:
			bool guaranteeMutation = false, mutateIfReplacementDiffers = false;
			uint64_t random = getChunkSeed(startSeed, x, z);
			// 1.6- has 2/3rds chance of doing nothing
			if (configuration->version <= MC_1_6 && quadraticNextInt(&random, 0, 3)) { // Last call (for 1.6-), so start salt does not matter
				*entry = centerValue;
				continue;
			}
			// In 1.7+, sample HillsAndRiver noise
			if (configuration->version >= MC_1_7) {
				int noise = (hillsNoise[flatten(x, z, configuration)] - 2) % 29;
				// 1/29 chance non-shallow-ocean biomes will later be mutated
				if (!shallowOceanCheck(centerValue, configuration->version) && noise == 1) guaranteeMutation = true;
				// Otherwise 2/3 + 28/29 chance of doing nothing
				else if (quadraticNextInt(&random, startSalt, 3) && noise) {
					*entry = centerValue;
					continue;
				}
				// Otherwise consider mutation if noise == 0
				else mutateIfReplacementDiffers = !noise;
			}

			int replacement = centerValue;
			// If mutation isn't already guaranteed, choose a potential replacement
			if (!guaranteeMutation) {
				switch (centerValue) {
					case desert:
						replacement = desert_hills;
						break;
					case forest:
						replacement = wooded_hills;
						break;
					case taiga:
						replacement = taiga_hills;
						break;
					case plains:
						// 1.7+ has 1/3 chance of replacing with wooded hills
						if (configuration->version >= MC_1_7 && !quadraticNextInt(&random, startSalt, 3)) replacement = wooded_hills;
						else replacement = forest;
						break;
					case snowy_tundra:
						replacement = snowy_mountains;
						break;
					case jungle:
						replacement = jungle_hills;
						break;
					case birch_forest:
						replacement = birch_forest_hills;
						break;
					case dark_forest:
						replacement = plains;
						break;
					case giant_tree_taiga:
						replacement = giant_tree_taiga_hills;
						break;
					case snowy_taiga:
						replacement = snowy_taiga_hills;
						break;
					case bamboo_jungle:
						replacement = bamboo_jungle_hills;
						break;
					case ocean:
						if (configuration->version >= MC_1_7) replacement = deep_ocean;
						break;
					case lukewarm_ocean:
						replacement = deep_lukewarm_ocean;
						break;
					case cold_ocean:
						replacement = deep_cold_ocean;
						break;
					case frozen_ocean:
						if (configuration->version >= MC_1_7) replacement = deep_frozen_ocean;
						break;
					case mountains:
						if (configuration->version >= MC_1_7) replacement = wooded_mountains;
						break;
					case savanna:
						replacement = savanna_plateau;
						break;
					case badlands_plateau:
					case wooded_badlands_plateau:
						replacement = badlands;
						break;
					case deep_ocean:
					case deep_lukewarm_ocean:
					case deep_cold_ocean:
					case deep_frozen_ocean:
						// 2/3 chance of doing nothing
						if (quadraticNextInt(&random, startSalt, 3)) break;
						// 1/2 chance of replacing with plains
						if (!quadraticNextInt(&random, startSalt, 2)) replacement = plains;
						// Otherwise replace with forest
						else replacement = forest;
						break;
				}
			}

			// If a mutation is guaranteed, or if it's guaranteed if the replacement differs and the replacement, well, differs, mutate the replacement
			if (guaranteeMutation || (mutateIfReplacementDiffers && centerValue != replacement)) {
				switch (replacement) {
					case plains:
						replacement = sunflower_plains;
						break;
					case desert:
						replacement = desert_lakes;
						break;
					case mountains:
						replacement = gravelly_mountains;
						break;
					case forest:
						replacement = flower_forest;
						break;
					case taiga:
						replacement = taiga_mountains;
						break;
					case swamp:
						replacement = swamp_hills;
						break;
					case snowy_tundra:
						replacement = ice_spikes;
						break;
					case jungle:
						replacement = modified_jungle;
						break;
					case jungle_edge:
						replacement = modified_jungle_edge;
						break;
					case birch_forest:
						// 1.9-1.10 accidentally overwrote birch forest hills' slot
						if (configuration->version >= MC_1_9 && configuration->version <= MC_1_10) replacement = tall_birch_hills;
						else replacement = tall_birch_forest;
						break;
					case birch_forest_hills:
						// 1.9-1.10 accidentally had the slot overwritten by birch forests
						if (configuration->version <= MC_1_8 || configuration->version >= MC_1_11) replacement = tall_birch_hills;
						else replacement = centerValue;
						break;
					case dark_forest:
						replacement = dark_forest_hills;
						break;
					case snowy_taiga:
						replacement = snowy_taiga_mountains;
						break;
					case giant_tree_taiga:
						replacement = giant_spruce_taiga;
						break;
					case giant_tree_taiga_hills:
						replacement = giant_spruce_taiga_hills;
						break;
					case wooded_mountains:
						replacement = modified_gravelly_mountains;
						break;
					case savanna:
						replacement = shattered_savanna;
						break;
					case savanna_plateau:
						replacement = shattered_savanna_plateau;
						break;
					case badlands:
						replacement = eroded_badlands;
						break;
					case wooded_badlands_plateau:
						replacement = modified_wooded_badlands_plateau;
						break;
					case badlands_plateau:
						replacement = modified_badlands_plateau;
						break;
					default:
						// If the replacement doesn't have a mutation, reset to the very original biome
						replacement = centerValue;
				}
			}
			// Guaranteed mutations are immediately replaced
			if (guaranteeMutation) {
				*entry = replacement;
				continue;
			}
			// Otherwise if the replacement would be different, replace if all neighbors match in 1.6-, or 3+ of the neighbors are in the same category in 1.7+
			if (centerValue != replacement) {
				int similarNeighborsCount = (int)similarLayerCheck(northValue, centerValue, configuration->version) + (int)similarLayerCheck(eastValue, centerValue, configuration->version) + (int)similarLayerCheck(westValue, centerValue, configuration->version) + (int)similarLayerCheck(southValue, centerValue, configuration->version);
				if (similarNeighborsCount >= (configuration->version <= MC_1_6 ? 4 : 3) ) {
					*entry = replacement;
					continue;
				}
			}

			// Otherwise, preserve the original value
			*entry = centerValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// 1.7+
// One-to-one
void addSunflowerLayer(int *const biomes, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];
			// Everything except plains are left alone
			if (*entry != plains) continue;

			uint64_t random = getChunkSeed(startSeed, x, z);
			// 1/57 chance to replace with sunflower plains
			if (!quadraticNextInt(&random, 0, 57)) *entry = sunflower_plains; // Last call, so start salt does not matter
		}
	}
}

// Beta 1.8 - 1.0; 1.1-1.6; 1.7+
// Castle
void shoreLayer(int *const biomes, int *const tempBuffer, const Configuration *const configuration) {
	
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
			
			switch (centerValue) {
				case mushroom_fields:
					if (shallowOceanCheck(northValue, configuration->version) || shallowOceanCheck(eastValue, configuration->version) || shallowOceanCheck(southValue, configuration->version) || shallowOceanCheck(westValue, configuration->version)) *entry = mushroom_field_shore;
					else *entry = centerValue;
					continue;
				case bamboo_jungle:
				case bamboo_jungle_hills:
				case jungle:
				case jungle_hills:
				case jungle_edge:
				case modified_jungle:
				case modified_jungle_edge:
					if (configuration->version >= MC_1_7 && (
						(getCategory(configuration->version, northValue) != jungle && northValue != forest && northValue != taiga && !isOceanic(northValue)) ||
						(getCategory(configuration->version, eastValue) != jungle && eastValue != forest && eastValue != taiga && !isOceanic(eastValue)) ||
						(getCategory(configuration->version, southValue) != jungle && southValue != forest && southValue != taiga && !isOceanic(southValue)) ||
						(getCategory(configuration->version, westValue) != jungle && westValue != forest && westValue != taiga && !isOceanic(westValue))
					)) {
						*entry = jungle_edge;
						continue;
					}
					break; // Jump to beach check at end
				case mountains:
					// 1.1-1.6 may change it
					if (configuration->version >= MC_1_1 && configuration->version <= MC_1_6) {
						if (northValue != mountains || eastValue != mountains || southValue != mountains || westValue != mountains) *entry = mountain_edge;
						else *entry = centerValue;
						continue;
					}
					// 1.0- and 1.7+ fall through
					#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L
						[[fallthrough]];
					#endif
				case wooded_mountains:
				case mountain_edge:
					if (configuration->version >= MC_1_7 && (
						isOceanic(northValue) || isOceanic(eastValue) || isOceanic(southValue) || isOceanic(westValue))
					) *entry = stone_shore;
					else *entry = centerValue;
					continue;
				case snowy_tundra:
				case snowy_mountains:
					if (configuration->version >= MC_1_1 && configuration->version <= MC_1_6) break; // Jump to beach check at end
					// 1.0- and 1.7+ fall through
					#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 202000L
						[[fallthrough]];
					#endif
				case snowy_beach:
				case frozen_river:
				case ice_spikes:
				case snowy_taiga:
				case snowy_taiga_hills:
				case snowy_taiga_mountains:
					if (configuration->version >= MC_1_7 && (
						oceanCheck(northValue, configuration->version) || oceanCheck(eastValue, configuration->version) || oceanCheck(southValue, configuration->version) || oceanCheck(westValue, configuration->version)
					)) *entry = snowy_beach;
					else *entry = centerValue;
					continue;
				case badlands:
				case wooded_badlands_plateau:
					if (!isOceanic(northValue) && !isOceanic(eastValue) && !isOceanic(southValue) && !isOceanic(westValue) && (
						!isMesa(northValue) || !isMesa(eastValue) || !isMesa(southValue) || !isMesa(westValue)
					)) *entry = desert;
					else *entry = centerValue;
					continue;
			}
			// Beach check
			if (configuration->version >= MC_1_1 && !oceanCheck(centerValue, configuration->version) && centerValue != river && centerValue != swamp && (
				oceanCheck(northValue, configuration->version) || oceanCheck(eastValue, configuration->version) || oceanCheck(southValue, configuration->version) || oceanCheck(westValue, configuration->version)
			)) *entry = beach;
			else *entry = centerValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// 1.1-1.6
// One-to-one
void addSwampRiverLayer(int *const biomes, uint64_t salt, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t layerSalt = getLayerSalt(salt);
	uint64_t startSeed = getStartSeed(configuration->worldseed, layerSalt);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];

			// Swamps have 1/6th chance to be replaced with rivers
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (*entry == swamp && !quadraticNextInt(&random, 0, 6)) *entry = river;
			// Jungles and jungle hills have 1/6th chance to be replaced with rivers
			else if ((*entry == jungle || *entry == jungle_hills) && !quadraticNextInt(&random, 0, 8)) *entry = river;
		}
	}
}

// All versions
// Castle
void smoothLayer(int *const biomes, int *const tempBuffer, uint64_t salt, const Configuration *const configuration) {
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
			
			bool xAxisMatches = eastValue == westValue;
			bool zAxisMatches = northValue == southValue;

			// If neither x-axis nor z-axis are uniform, preserve original value
			if (!xAxisMatches && !zAxisMatches) {
				*entry = centerValue;
				continue;
			}
			// If x-axis is uniform and z-axis isn't, replace with x-axis value
			if (xAxisMatches && !zAxisMatches) {
				*entry = westValue;
				continue;
			}
			// If z-axis is uniform and x-axis isn't, replace with z-axis value
			if (!xAxisMatches && zAxisMatches) {
				*entry = northValue;
				continue;
			}
			// If both x-axis and z-axis are uniform, replace with z-axis with 1/2 chance, otherwise x-axis
			uint64_t random = getChunkSeed(startSeed, x, z);
			if (quadraticNextInt(&random, 0, 2)) *entry = northValue;
			else *entry = westValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// Beta 1.8-1.6; 1.7+
// Castle
void riverLayer(int *const riverNoise, int *const tempBuffer, const Configuration *const configuration) {

	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 1; z <= configuration->maximumZ - 1; ++z) {
		for (int64_t x = configuration->minimumX + 1; x <= configuration->maximumX - 1; ++x) {
			// Sampling
			// --------
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int northValue = riverNoise[flatten(x, z - 1, configuration)];
			int eastValue = riverNoise[flatten(x + 1, z, configuration)];
			int southValue = riverNoise[flatten(x, z + 1, configuration)];
			int westValue = riverNoise[flatten(x - 1, z, configuration)];
			int centerValue = riverNoise[flatten(x, z, configuration)];
			
			int centerParity = centerValue ? 2 + (centerValue & 1) : 0;
			// In 1.6-, if the center does not match all of its orthagonal neighbors, or is nonzero, it is a potential river
			if (configuration->version <= MC_1_6 && (
				centerValue != northValue || centerValue != eastValue || centerValue != southValue || centerValue != westValue || !centerValue
			)) *entry = river;
			// In 1.7+, if the parity of the center does not match the parities of all of its orthagonal neighbors, it is a potential river
			else if (configuration->version >= MC_1_7 && (
				// centerParity != (westValue & 1) || centerParity != (northValue & 1) || centerParity != (eastValue & 1) || centerParity != (southValue & 1)
				centerParity != (westValue ? 2 + (westValue & 1) : 0) ||
				centerParity != (northValue ? 2 + (northValue & 1) : 0) ||
				centerParity != (eastValue ? 2 + (eastValue & 1) : 0) ||
				centerParity != (southValue ? 2 + (southValue & 1) : 0)
			)) *entry = river;
			// Otherwise it is land
			else *entry = -1;
		}
	}
	memmove(riverNoise, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// Beta 1.8-1.6; 1.7+
// One-to-one
void riverMixerLayer(int *const biomes, const int *const riverNoise, const Configuration *const configuration) {

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];

			if (oceanCheck(*entry, configuration->version) || *entry == mushroom_field_shore || riverNoise[flatten(x, z, configuration)] != river) continue;
			switch (*entry) {
				case snowy_tundra:
					*entry = frozen_river;
					continue;
				case mushroom_fields:
					*entry = mushroom_field_shore;
					continue;
				default:
					*entry = river;
			}
		}
	}
}

// 1.13+
// One-to-one
void oceanLayer(int *const oceans, const Configuration *const configuration) {
	// Initialization
	// --------------
	uint64_t random;
	setSeed(&random, configuration->worldseed);
	PerlinNoise oceanNoise;
	perlinInit(&oceanNoise, &random);

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &oceans[flatten(x, z, configuration)];

			double sample = samplePerlin(&oceanNoise, x/8., z/8., 0., 0., 0.);
			if (sample > 0.4) *entry = warm_ocean;
			else if (sample > 0.2) *entry = lukewarm_ocean;
			else if (sample >= -0.2) *entry = ocean;
			else if (sample >= -0.4) *entry = cold_ocean;
			else *entry = frozen_ocean;
		}
	}
}

// 1.13+
// 8x8
void oceanMixerLayer(int *const biomes, const int *const oceanNoise, int *const tempBuffer, const Configuration *const configuration) {

	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 8; z <= configuration->maximumZ - 8; ++z) {
		for (int64_t x = configuration->minimumX + 8; x <= configuration->maximumX - 8; ++x) {
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];

			// Non-oceanic biomes are ignored
			if (!isOceanic(centerValue)) {
				*entry = centerValue;
				continue;
			}
			
			int oceanSelection = oceanNoise[flatten(x, z, configuration)];
			// If the sampled noise equates to a warm or frozen ocean...
			if (oceanSelection == warm_ocean || oceanSelection == frozen_ocean) {
				// ...and any of the 25 coordinates surrounding the current one, spaced 4 blocks apart, is a land biome...
				for (int dz = -8; dz <= 8; dz += 4) {
					for (int dx = -8; dx <= 8; dx += 4) {
						int neighboringValue = biomes[flatten(x + dx, z + dz, configuration)];
						if (isOceanic(neighboringValue)) continue;
						// Moderate the ocean's temperature
						if (oceanSelection == warm_ocean) *entry = lukewarm_ocean;
						else *entry = cold_ocean;
						goto L_next_coordinate;
					}
				}
			}
			// Otherwise if the biome is a deep ocean, and the sampled noise doesn't equate to a warm ocean, replace with corresponding deep ocean variant
			if (centerValue == deep_ocean && oceanSelection != warm_ocean) {
				switch (oceanSelection) {
					case lukewarm_ocean:
						*entry = deep_lukewarm_ocean;
						continue;
					case ocean:
						*entry = deep_ocean;
						continue;
					case cold_ocean:
						*entry = deep_cold_ocean;
						continue;
					case frozen_ocean:
						*entry = deep_frozen_ocean;
						continue;
				}
			}
			// Otherwise set to original ocean selection
			*entry = oceanSelection;
			L_next_coordinate: continue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}
