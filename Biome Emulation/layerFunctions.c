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
//		- If the coordinate is ocean, and any neighboring diagonal isn't, randomly select a non-Ocean diagonal and roll 1/3rd chance to replace it. If the roll failed but the replacement would have been Freezing/Forest, replace with Freezing/Forest.
//		- If the coordinate is not Ocean or Freezing/Forest, any neighboring diagonal is Ocean, and a 1/5th chance roll succeeds, replace with Ocean.
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
			// If the center value is Ocean, and one of the four corner biomes isn't:
			if (centerValue == ocean && (southwestValue != ocean || southeastValue != ocean || northwestValue != ocean || northeastValue != ocean)) {
				// For Beta 1.8, roll 2/3rd chance to preserve ocean, otherwise change to land
				if (configuration->version == MC_B1_8) {
					*entry = (quadraticNextInt(&random, 0, 3) == 2); // Last call, so start salt does not matter
					continue;
				}
				// Otherwise randomly choose a land corner to potentially replace it
				int landBiomesFoundCount = 0, potentialReplacement = Warm;
				if (northwestValue != ocean && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = northwestValue;
				if (northeastValue != ocean && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = northeastValue;
				if (southwestValue != ocean && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = southwestValue;
				if (southeastValue != ocean && !quadraticNextInt(&random, startSalt, ++landBiomesFoundCount)) potentialReplacement = southeastValue;
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
			if (configuration->version == MC_B1_8 && centerValue == plains && (
				southwestValue != plains || southeastValue != plains || northwestValue != plains || northeastValue != plains
			)) {
				// For Beta 1.8, roll 4/5th chance to preserve plains, otherwise change to ocean
				*entry = (quadraticNextInt(&random, 0, 5) != 4); // Last call, so start salt does not matter
					continue;
			}
			// In 1.0+, if the center value isn't Ocean, one of the four corner biomes is Ocean, and a 1/5 chance occurs:
			if (configuration->version >= MC_1_0 && centerValue != ocean && (southwestValue == ocean || southeastValue == ocean || northwestValue == ocean || northeastValue == ocean) && !quadraticNextInt(&random, 0, 5)) { // Last call, so start salt does not matter
				// 1.0-1.6 snowy tundras switch to frozen ocean
				if (MC_1_0 <= configuration->version && configuration->version <= MC_1_6 && centerValue == snowy_tundra) *entry = frozen_ocean;
				// 1.7+ freezing biomes are preserved
				else if (MC_1_7 <= configuration->version && centerValue == Freezing) *entry = Freezing;
				// Otherwise replace with ocean
				else *entry = ocean;
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
			
			// TODO: Why is the 4-bit tag necessary?
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
// Badlands Plateau/Wooded Badlands Plateau not surrounded orthagonally by Badlands Plateaus/Wooded Badlands Plateaus is replaced with Badlands.
// Giant Tree Taiga not surrounded orthagonally by Taigas, Snowy Taigas, or Giant Tree Taigas is replaced with Taigas.
// Desert orthagonally bordering a Snowy Tundra is replaced with Wooded Mountains.
// Swamp orthagonally bordering a Desert, Snowy Taiga, or Snowy Tundra is replaced with Plains.
// Swamp orthagonally bordering a Jungle or Bamboo Jungle is replaced with Jungle Edge.
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

// Beta 1.8 - 1.6; 1.7+. One-to-one.
// - Beta 1.8 - 1.6:
//		- Non-Ocean coordinates are replaced with either 2 or 3
// - 1.7+:
//		- Non-Ocean coordinates are replaced with values in the range [2...300001].
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
			if (*entry == ocean) continue;

			uint64_t random = getChunkSeed(startSeed, x, z);
			// 1.6- rolls a 1/2 chance
			if (configuration->version <= MC_1_6) *entry = 2 + quadraticNextInt(&random, 0, 2); // Last call, so start salt does not matter
			// 1.7+ instead rolls a... 1/299999 chance?
			else *entry = 2 + quadraticNextInt(&random, 0, 299999); // Last call, so start salt does not matter
		}
	}
}

// 1.1 (hillsNoise unused); 1.2-1.6 (hillsNoise unused); 1.7-1.8/1.11+; 1.9-1.10. Castle.
// - 1.1:
//		- Plains orthagonally surrounded by plains are replaced with forests if a 1/3rd chance succeeds.
//		- Deserts orthagonally surrounded by deserts are replaced with desert hills if a 1/3rd chance succeeds.
//		- Forests orthagonally surrounded by forests are replaced with wooded hills if a 1/3rd chance succeeds.
//		- Taigas orthagonally surrounded by taigas are replaced with taiga hills if a 1/3rd chance succeeds.
//		- Snowy tundras orthagonally surrounded by snowy tundras are replaced with snowy mountains if a 1/3rd chance succeeds.
// - 1.2-1.6:
//		- Plains orthagonally surrounded by plains are replaced with forests if a 1/3rd chance succeeds.
//		- Deserts orthagonally surrounded by deserts are replaced with desert hills if a 1/3rd chance succeeds.
//		- Forests orthagonally surrounded by forests are replaced with wooded hills if a 1/3rd chance succeeds.
//		- Taigas orthagonally surrounded by taigas are replaced with taiga hills if a 1/3rd chance succeeds.
//		- Snowy tundras orthagonally surrounded by snowy tundras are replaced with snowy mountains if a 1/3rd chance succeeds.
//		- Jungles orthagonally surrounded by jungles are replaced with jungle hills if a 1/3rd chance succeeds.
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
			
			// In 1.6-, nothing happens if any orthogonal neighbors differ from the center, or a
			// 2/3rds chance succeeds
			if (configuration->version <= MC_1_6 && (
				northValue != centerValue || eastValue != centerValue || southValue != centerValue || westValue != centerValue || quadraticNextInt(&random, 0, 3)
			)) { // Last call (for 1.6-), so start salt does not matter
				*entry = centerValue;
				continue;
			}
			// In 1.7+, sample hills noise
			if (configuration->version >= MC_1_7) {
				int noise = (hillsNoise[flatten(x, z, configuration)] - 2) % 29;
				// 1/29 chance non-shallow-ocean biomes will later be mutated
				if (centerValue != ocean && noise == 1) guaranteeMutation = true;
				// Otherwise 2/3 + 28/29 chance of doing nothing
				else if (quadraticNextInt(&random, startSalt, 3) && noise) {
					*entry = centerValue;
					continue;
				}
				// Otherwise consider mutation if noise == 0
				else mutateIfReplacementDiffers = !noise;

				// If a mutation hasn't been guaranteed, and < 3 orthogonal neighbors are in the same
				// biome category as the center, skip
				if (!guaranteeMutation) {
					int similarNeighborsCount = (int)areSimilar(configuration->version, northValue, centerValue) + (int)areSimilar(configuration->version, eastValue, centerValue) + (int)areSimilar(configuration->version, westValue, centerValue) + (int)areSimilar(configuration->version, southValue, centerValue);
					if (similarNeighborsCount < 3) {
						*entry = centerValue;
						continue;
					}
				}
			}

			switch (centerValue) {
				case ocean:
					if (configuration->version >= MC_1_7 && !guaranteeMutation && !mutateIfReplacementDiffers) *entry = deep_ocean;
					else *entry = centerValue;
					continue;
				case plains:
					if (guaranteeMutation) *entry = sunflower_plains;
					else if (mutateIfReplacementDiffers) {
						// 2/3rds chance of replacing with flower forest; otherwise keep the same
						if (quadraticNextInt(&random, startSalt, 3)) *entry = flower_forest;
						else *entry = centerValue;
					// 1/3rd chance of replacing with wooded hills; otherwise replace with forest
					} else if (configuration->version >= MC_1_7 && !quadraticNextInt(&random, startSalt, 3)) *entry = wooded_hills;
					else *entry = forest;
					continue;
				case desert:
					if (guaranteeMutation) *entry = desert_lakes;
					else if (mutateIfReplacementDiffers) *entry = centerValue;
					else *entry = desert_hills;
					continue;
				case mountains:
					if (guaranteeMutation) *entry = gravelly_mountains;
					else if (mutateIfReplacementDiffers) *entry = modified_gravelly_mountains;
					else if (configuration->version >= MC_1_7) *entry = wooded_mountains;
					else *entry = centerValue;
					continue;
				case forest:
					if (guaranteeMutation) *entry = flower_forest;
					else if (mutateIfReplacementDiffers) *entry = centerValue;
					else *entry = wooded_hills;
					continue;
				case taiga:
					if (guaranteeMutation) *entry = taiga_mountains;
					else if (mutateIfReplacementDiffers) *entry = centerValue;
					else *entry = taiga_hills;
					continue;
				case swamp:
					if (guaranteeMutation) *entry = swamp_hills;
					else *entry = centerValue;
					continue;
				case snowy_tundra:
					if (guaranteeMutation) *entry = ice_spikes;
					else if (mutateIfReplacementDiffers) *entry = centerValue;
					else *entry = snowy_mountains;
					continue;
				case mushroom_fields:
					*entry = centerValue;
					continue;
				case jungle:
					if (guaranteeMutation) *entry = modified_jungle;
					else if (mutateIfReplacementDiffers) *entry = centerValue;
					else *entry = jungle_hills;
					continue;
				// 1.7+
				case jungle_edge:
					if (guaranteeMutation) *entry = modified_jungle_edge;
					else *entry = centerValue;
					continue;
				case deep_ocean:
					// If guaranteed to be mutated, or 2/3rd chance succeeds, keep the same
					if (guaranteeMutation || quadraticNextInt(&random, startSalt, 3)) *entry = centerValue;
					else if (mutateIfReplacementDiffers) {
						// 1/2th chance of replacing with sunflower plains; otherwise replace with
						// flower forest
						if (!quadraticNextInt(&random, startSalt, 2)) *entry = sunflower_plains;
						else *entry = flower_forest;
					// 1/2th chance of replacing with plains; otherwise replace with forest
					} else if (!quadraticNextInt(&random, startSalt, 2)) *entry = plains;
					else *entry = forest;
					continue;
				case birch_forest:
					if (guaranteeMutation) {
						if (configuration->version <= MC_1_8 || configuration->version >= MC_1_11) *entry = tall_birch_forest;
						else *entry = tall_birch_hills;
					}
					else if (mutateIfReplacementDiffers) {
						if (configuration->version <= MC_1_8 || configuration->version >= MC_1_11) *entry = tall_birch_hills;
						else *entry = centerValue;
					}
					else *entry = birch_forest_hills;
					continue;
				case dark_forest:
					if (guaranteeMutation) *entry = dark_forest_hills;
					else if (mutateIfReplacementDiffers) *entry = sunflower_plains;
					else *entry = plains;
					continue;
				case snowy_taiga:
					if (guaranteeMutation) *entry = snowy_taiga_mountains;
					else if (mutateIfReplacementDiffers) *entry = centerValue;
					else *entry = snowy_taiga_hills;
					continue;
				case giant_tree_taiga:
					if (guaranteeMutation) *entry = giant_spruce_taiga;
					else if (mutateIfReplacementDiffers) *entry = giant_spruce_taiga_hills;
					else *entry = giant_tree_taiga_hills;
					continue;
				case wooded_mountains:
					if (guaranteeMutation) *entry = modified_gravelly_mountains;
					else *entry = centerValue;
					continue;
				case savanna:
					if (guaranteeMutation) *entry = shattered_savanna;
					else if (mutateIfReplacementDiffers) *entry = shattered_savanna_plateau;
					else *entry = savanna_plateau;
					continue;
				case badlands:
					if (guaranteeMutation) *entry = eroded_badlands;
					else *entry = centerValue;
					continue;
				case wooded_badlands_plateau:
					if (guaranteeMutation) *entry = modified_wooded_badlands_plateau;
					else if (mutateIfReplacementDiffers) *entry = eroded_badlands;
					else *entry = badlands;
					continue;
				case badlands_plateau:
					if (guaranteeMutation) *entry = modified_badlands_plateau;
					else if (mutateIfReplacementDiffers) *entry = eroded_badlands;
					else *entry = badlands;
					continue;
				// 1.14+
				case bamboo_jungle:
					if (!guaranteeMutation && !mutateIfReplacementDiffers) *entry = bamboo_jungle_hills;
					else *entry = centerValue;
					continue;
			}
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// 1.7+. One-to-one.
// Plains have a 1/57th chance of being replaced with Sunflower Plains.
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

// 1.0; 1.1-1.6; 1.7-1.13; 1.14+. Castle.
// - 1.0:
// 		- Mushroom Fields orthagonally bordered by Ocean are replaced with Mushroom Field Shore.
// - 1.1-1.6:
//		- Mountains orthagonally bordered by non-Mountains are replaced with Mountain Edge.
// 		- Mushroom Fields orthagonally bordered by Ocean are replaced with Mushroom Field Shore.
//		- All other biomes except Oceans and Swamps, if bordered by Ocean or Deep Ocean, are replaced with
// 			Beach.
// - 1.7-1.13:
//		- Mountains and Wooded Mountains orthagonally bordered by Ocean or Deep Ocean are replaced with Stone
// 			Shore.
//		- Snowy Tundras, Snowy Mountains, Ice Spikes, Snowy Taigas, Snowy Taiga Hills, and Snowy Taiga
// 			Mountains orthagonally bordered by Ocean or Deep Ocean are replaced with Snowy Beach.
// 		- Mushroom Fields orthagonally bordered by Ocean are replaced with Mushroom Field Shore.
//		- Jungles, Jungle Hills, Jungle Edges, Modified Jungles, and Modified Jungle Edges orthagonally
// 			bordered by anything other than a Jungle-Category biome, Forest, Taiga, Ocean, or Deep Ocean are
// 			replaced with Jungle Edge. Those otherwise bordered by an Ocean or Deep Ocean are replaced with
// 			Beach.
//		- Badlands and Wooded Badlands Plateaus not orthagonally bordered by Ocean, Deep Ocean, or
// 			Mesa-category biomes are replaced with Deserts.
//		- All other biomes except Oceans, Deep Oceans, and Swamps, if bordered by Ocean or Deep Ocean,
// 			are replaced with Beach.
// - 1.14+:
//		- Mountains and Wooded Mountains orthagonally bordered by Ocean or Deep Ocean are replaced with Stone
// 			Shore.
//		- Snowy Tundras, Snowy Mountains, Ice Spikes, Snowy Taigas, Snowy Taiga Hills, and Snowy Taiga
// 			Mountains orthagonally bordered by Ocean or Deep Ocean are replaced with Snowy Beach.
// 		- Mushroom Fields orthagonally bordered by Ocean are replaced with Mushroom Field Shore.
//		- Jungles, Jungle Hills, Jungle Edges, Modified Jungles, Modified Jungle Edges, Bamboo Jungles, and
// 			Bamboo Jungle Hills orthagonally bordered by anything other than a Jungle-Category biome, Forest, 
// 			Taiga, Ocean, or Deep Ocean are replaced with Jungle Edge. Those otherwise bordered by an Ocean
// 			or Deep Ocean are replaced with Beach.
//		- Badlands and Wooded Badlands Plateaus not orthagonally bordered by Ocean, Deep Ocean, or
// 			Mesa-category biomes are replaced with Deserts.
//		- All other biomes except Oceans, Deep Oceans, and Swamps, if bordered by Ocean or Deep Ocean,
// 			are replaced with Beach.
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
			
			// Mushroom fields orthagonally bordering ocean become mushroom fields shores
			if (centerValue == mushroom_fields) {
				if (isAny4(ocean, northValue, eastValue, southValue, westValue)) *entry = mushroom_field_shore;
				else *entry = centerValue;
				continue;
			}
			// Everything else in 1.0 is preserved
			if (configuration->version == MC_1_0) {
				*entry = centerValue;
				continue;
			}

			// Special cases:
			switch (centerValue) {
				case mountains:
					// 1.1-1.6 mountains
					if (configuration->version <= MC_1_6) {
						if (northValue != mountains || eastValue != mountains || southValue != mountains || westValue != mountains) *entry = mountain_edge;
						else *entry = centerValue;
						continue;
					}
					break; // Continue to beach addition
				// 1.7+ Jungle-category biomes
				case bamboo_jungle:
				case bamboo_jungle_hills:
				case jungle:
				case jungle_hills:
				case jungle_edge:
				case modified_jungle:
				case modified_jungle_edge:
					if (configuration->version >= MC_1_7 && (
						(getCategory(configuration->version, northValue) != jungle && northValue != forest && northValue != taiga && northValue != ocean && northValue != deep_ocean) ||
						(getCategory(configuration->version, eastValue) != jungle && eastValue != forest && eastValue != taiga && eastValue != ocean && eastValue != deep_ocean) ||
						(getCategory(configuration->version, southValue) != jungle && southValue != forest && southValue != taiga && southValue != ocean && southValue != deep_ocean) ||
						(getCategory(configuration->version, westValue) != jungle && westValue != forest && westValue != taiga && westValue != ocean && westValue != deep_ocean)
					)) {
						*entry = jungle_edge;
						continue;
					}
					break; // Continue to beach addition
				// 1.7+ Badlands/Wooded Badlands Plateaus
				case badlands:
				case wooded_badlands_plateau:
					if (!isAny4(ocean, northValue, eastValue, southValue, westValue) && !isAny4(deep_ocean, northValue, eastValue, southValue, westValue) && (
						!isMesa(northValue) || !isMesa(eastValue) || !isMesa(southValue) || !isMesa(westValue)
					)) *entry = desert;
					else *entry = centerValue;
					continue;
			}

			// In all other cases, if any neighbors are oceans or deep oceans:
			if (isAny4(ocean, northValue, eastValue, southValue, westValue) || isAny4(deep_ocean, northValue, eastValue, southValue, westValue)) {
				switch (centerValue) {
					// Preserved
					case ocean:
					case deep_ocean:
					case swamp:
						*entry = centerValue;
						break;

					// Replaced with Stone Shores
					case mountains:
					case wooded_mountains:
						*entry = stone_shore;
						break;

					// Replaced with Beaches in 1.1-1.6, or Snowy Beaches in 1.7+
					case snowy_tundra:
					case snowy_mountains:
						*entry = (configuration->version <= MC_1_6 ? beach : snowy_beach);
						break;

					// Replaced with Snowy Beaches
					case snowy_taiga:
					case snowy_taiga_hills:
					case ice_spikes:
					case snowy_taiga_mountains:
						*entry = snowy_beach;
						break;

					// Replaced with Beaches
					default:
						*entry = beach;
				}
			} else *entry = centerValue;
		}
	}
	memmove(biomes, tempBuffer, configuration->width*configuration->height*sizeof(*tempBuffer));
}

// 1.1; 1.2-1.6. One-to-one.
// - 1.1:
//		- Swamps have a 1/6th chance of being replaced with rivers.
// - 1.2-1.6:
//		- Swamps have a 1/6th chance of being replaced with rivers.
//		- Jungles and Jungle Hills have a 1/8th chance of being replaced with rivers.
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
			// Jungles and jungle hills have 1/8th chance to be replaced with rivers
			else if ((*entry == jungle || *entry == jungle_hills) && !quadraticNextInt(&random, 0, 8)) *entry = river;
		}
	}
}

// All versions. Castle.
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

// Beta 1.8-1.6; 1.7+. Castle.
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
				// NOTE: The parities can't be replaced with ([...]Value & 3) because all land cases must be completely separate from ocean cases. As is, 4 mod 4 == 0 == ocean.
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

// Beta 1.8-1.6; 1.7+. One-to-one.
void riverMixerLayer(int *const biomes, const int *const riverNoise, const Configuration *const configuration) {

	for (int64_t z = configuration->minimumZ; z <= configuration->maximumZ; ++z) {
		for (int64_t x = configuration->minimumX; x <= configuration->maximumX; ++x) {
			// Sampling
			// --------
			int *const entry = &biomes[flatten(x, z, configuration)];

			if (*entry == ocean || *entry == deep_ocean || *entry == mushroom_field_shore || riverNoise[flatten(x, z, configuration)] != river) continue;
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

// 1.13+. One-to-one.
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

// 1.13+. 8x8.
void oceanMixerLayer(int *const biomes, const int *const oceanNoise, int *const tempBuffer, const Configuration *const configuration) {

	// TODO: Figure out how to support coordinates outside the desired region
	for (int64_t z = configuration->minimumZ + 8; z <= configuration->maximumZ - 8; ++z) {
		for (int64_t x = configuration->minimumX + 8; x <= configuration->maximumX - 8; ++x) {
			int *const entry = &tempBuffer[flatten(x, z, configuration)];
			int centerValue = biomes[flatten(x, z, configuration)];

			// Non-oceanic biomes are ignored
			if (centerValue != ocean && centerValue != deep_ocean) {
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
						if (neighboringValue == ocean || neighboringValue == deep_ocean) continue;
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
