#include <stdbool.h>
#include <stdio.h>
#include "cubiomes/generator.h"
#include "cubiomes/util.h"

const char *layer2str(ptrdiff_t layer) {
	switch (layer) {
		case L_CONTINENT_4096: return "Continent 4096";
		case L_ZOOM_4096: return "Zoom 4096";
		case L_LAND_4096: return "Land 4096";
		case L_ZOOM_2048: return "Zoom 2048";
		case L_LAND_2048: return "Land 2048";
		case L_ZOOM_1024: return "Zoom 1024";
		case L_LAND_1024_A: return "Land 1024 A";
		case L_LAND_1024_B: return "Land 1024 B";
		case L_LAND_1024_C: return "Land 1024 C";
		case L_ISLAND_1024: return "Island 1024";
		case L_SNOW_1024: return "Snow 1024";
		case L_LAND_1024_D: return "Land 1024 D";
		case L_COOL_1024: return "Cool 1024";
		case L_HEAT_1024: return "Heat 1024";
		case L_SPECIAL_1024: return "Special 1024";
		case L_ZOOM_512: return "Zoom 512";
		case L_LAND_512: return "Land 512";
		case L_ZOOM_256: return "Zoom 256";
		case L_LAND_256: return "Land 256";
		case L_MUSHROOM_256: return "Mushroom 256";
		case L_DEEP_OCEAN_256: return "Deep Ocean 256";
		case L_BIOME_256: return "Biome 256";
		case L_BAMBOO_256: return "Bamboo 256";
		case L_ZOOM_128: return "Zoom 128";
		case L_ZOOM_64: return "Zoom 64";
		case L_BIOME_EDGE_64: return "Biome Edge 64";
		case L_NOISE_256: return "Noise 256";
		case L_ZOOM_128_HILLS: return "Zoom 128 Hills";
		case L_ZOOM_64_HILLS: return "Zoom 64 Hills";
		case L_HILLS_64: return "Hills 64";
		case L_SUNFLOWER_64: return "Sunflower 64";
		case L_ZOOM_32: return "Zoom 32";
		case L_LAND_32: return "Land 32";
		case L_ZOOM_16: return "Zoom 16";
		case L_SHORE_16: return "Shore 16";
		case L_SWAMP_RIVER_16: return "Swamp River 16";
		case L_ZOOM_8: return "Zoom 8";
		case L_ZOOM_4: return "Zoom 4";
		case L_SMOOTH_4: return "Smooth 4";
		case L_ZOOM_128_RIVER: return "Zoom 128 River";
		case L_ZOOM_64_RIVER: return "Zoom 64 River";
		case L_ZOOM_32_RIVER: return "Zoom 32 River";
		case L_ZOOM_16_RIVER: return "Zoom 16 River";
		case L_ZOOM_8_RIVER: return "Zoom 8 River";
		case L_ZOOM_4_RIVER: return "Zoom 4 River";
		case L_RIVER_4: return "River 4";
		case L_SMOOTH_4_RIVER: return "Smooth 4 River";
		case L_RIVER_MIX_4: return "River Mix 4";
		case L_OCEAN_TEMP_256: return "Ocean Temperature 256";
		case L_ZOOM_128_OCEAN: return "Zoom 128 Ocean";
		case L_ZOOM_64_OCEAN: return "Zoom 64 Ocean";
		case L_ZOOM_32_OCEAN: return "Zoom 32 Ocean";
		case L_ZOOM_16_OCEAN: return "Zoom 16 Ocean";
		case L_ZOOM_8_OCEAN: return "Zoom 8 Ocean";
		case L_ZOOM_4_OCEAN: return "Zoom 4 Ocean";
		case L_OCEAN_MIX_4: return "Ocean Mix 4";
		case L_VORONOI_1: return "Voronoi 1";
		case L_ZOOM_LARGE_A: return "Zoom Large Biomes A";
		case L_ZOOM_LARGE_B: return "Zoom Large Biomes B";
		case L_ZOOM_L_RIVER_A: return "Zoom Large Biomes River A";
		case L_ZOOM_L_RIVER_B: return "Zoom Large Biomes River B";
		default: return "None";
	}
}

bool printLayerStatistics() {
	FILE *file = fopen(".\\Layer Statistics.txt", "w");
	if (!file) return false;

	LayerStack layerStack;
	for (int version = MC_B1_8; version <= MC_1_17; ++version) {
		for (int largeBiomes = 0; largeBiomes <= (version >= MC_1_3); ++largeBiomes) {
			fprintf(file, "%s%s\n---------------------------\n", mc2str(version), largeBiomes ? " Large Biomes" : "");
			setupLayerStack(&layerStack, version, largeBiomes);
			for (size_t i = 0; i < sizeof(layerStack.layers)/sizeof(*layerStack.layers); ++i) {
				Layer *layer = &layerStack.layers[i];
				if (!layer->mc) continue;
				fprintf(file, "%s:\n- Zoom: %" PRId8 "\n- Edge: %" PRId8 "\n- Scale: %d\n- Layer Salt: %" PRIu64 "\n- First Parent: %s\n- Second Parent: %s\n\n", layer2str(i), layer->zoom, layer->edge, layer->scale, layer->layerSalt, layer2str(layer->p - layerStack.layers), layer2str(layer->p2 - layerStack.layers));
			}
			fprintf(file, "---------------------------\n");
		}
	}
	return !fclose(file);
}





bool testMcStepSeed(uint64_t internalState, uint64_t salt) {
	printf("(%" PRIu64 ", %" PRIu64 ") -> %" PRIu64 "\n", internalState, salt, mcStepSeed(internalState, salt));
	return true;
}

// Quadratic congruence reversal logic from S. M. Dehnavi et al: https://doi.org/10.7546/nntdm.2019.25.1.75-83

uint64_t solveWithEvenA(uint64_t evenA, uint64_t oddB, uint64_t c, size_t n) {
	uint64_t output = 0;
	for (size_t i = 0; i < n; ++i) {
		if (c & 1) {
			output |= UINT64_C(1) << i;
			c = evenA/2 + oddB/2 + c/2 + 1;
			oddB = 2*evenA + oddB;
		} else c /= 2;
		evenA *= 2;
	}
	return output;
}

uint64_t getEvenSolution(uint64_t oddA, uint64_t oddB, uint64_t evenC) {
	return 2*solveWithEvenA(2*oddA, oddB, evenC/2, 63);
}

uint64_t getOddSolution(uint64_t oddA, uint64_t oddB, uint64_t evenC) {
	return 2*solveWithEvenA(2*oddA, 2*oddA + oddB, (oddA + oddB + evenC)/2, 63) + 1;
}

bool testReverseMcStepSeed(uint64_t output, uint64_t salt) {
	if ((output & 1) != (salt & 1)) {
		printf("testReverseMcStepSeed(%" PRIu64 ", %" PRIu64 "): No solution exists.\n", output, salt);
		return false;
	}
	uint64_t oddSolution = getOddSolution(6364136223846793005, 1442695040888963407, salt - output);
	uint64_t evenSolution = getEvenSolution(6364136223846793005, 1442695040888963407, salt - output);
	printf("Odd solution: %" PRIu64 " (Verification: %" PRIu64 " = %" PRIu64 ")\n", oddSolution, mcStepSeed(oddSolution, salt), output);
	printf("Even solution: %" PRIu64 " (Verification: %" PRIu64 " = %" PRIu64 ")\n", evenSolution, mcStepSeed(evenSolution, salt), output);
	return true;
}

int main() {
	// printLayerStatistics();
	testMcStepSeed(8675309, 3107951898966440229);
	testReverseMcStepSeed(mcStepSeed(8675309, 3107951898966440229), 3107951898966440229);
	return 0;
}