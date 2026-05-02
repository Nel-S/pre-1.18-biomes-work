#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../cubiomes/generator.h"
#include "../cubiomes/util.h"
#include "layerFunctions.h"

const uint64_t WORLDSEEDS_TO_CHECK[] = {
	8675309,
	-246117,
	111111111111111111,
	0,
	1,
	2,
};

const uint64_t SUPPORTED_LAYERS =
	  (UINT64_C(1) << L_CONTINENT_4096)
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
	| (UINT64_C(1) << L_BIOME_256)
	| (UINT64_C(1) << L_BAMBOO_256)
	| (UINT64_C(1) << L_ZOOM_128)
	| (UINT64_C(1) << L_ZOOM_64)
	| (UINT64_C(1) << L_BIOME_EDGE_64)
	| (UINT64_C(1) << L_NOISE_256)
	| (UINT64_C(1) << L_ZOOM_128_HILLS)
	| (UINT64_C(1) << L_ZOOM_64_HILLS)
	| (UINT64_C(1) << L_HILLS_64)
	| (UINT64_C(1) << L_SUNFLOWER_64)
	| (UINT64_C(1) << L_ZOOM_32)
	| (UINT64_C(1) << L_LAND_32)
	| (UINT64_C(1) << L_SHORE_16)
	| (UINT64_C(1) << L_ZOOM_16)
	| (UINT64_C(1) << L_SWAMP_RIVER_16)
	| (UINT64_C(1) << L_ZOOM_8)
	| (UINT64_C(1) << L_ZOOM_4)
	| (UINT64_C(1) << L_ZOOM_LARGE_A)
	| (UINT64_C(1) << L_ZOOM_LARGE_B)
	| (UINT64_C(1) << L_ZOOM_128_RIVER)
	| (UINT64_C(1) << L_ZOOM_64_RIVER)
	| (UINT64_C(1) << L_ZOOM_32_RIVER)
	| (UINT64_C(1) << L_ZOOM_16_RIVER)
	| (UINT64_C(1) << L_ZOOM_8_RIVER)
	| (UINT64_C(1) << L_ZOOM_4_RIVER)
	| (UINT64_C(1) << L_ZOOM_L_RIVER_A)
	| (UINT64_C(1) << L_ZOOM_L_RIVER_B)
	| (UINT64_C(1) << L_RIVER_4)
	| (UINT64_C(1) << L_SMOOTH_4_RIVER)
	| (UINT64_C(1) << L_RIVER_MIX_4)
	| (UINT64_C(1) << L_OCEAN_TEMP_256)
	| (UINT64_C(1) << L_ZOOM_128_OCEAN)
	| (UINT64_C(1) << L_ZOOM_64_OCEAN)
	| (UINT64_C(1) << L_ZOOM_32_OCEAN)
	| (UINT64_C(1) << L_ZOOM_16_OCEAN)
	| (UINT64_C(1) << L_ZOOM_8_OCEAN)
	| (UINT64_C(1) << L_ZOOM_4_OCEAN)
	| (UINT64_C(1) << L_OCEAN_MIX_4)
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

int emulateBiomes(const Configuration *const configuration, int *const biomes, size_t biomesCapacity, int *const biomesRequiredMargin) {
	if (!configuration) return 1;
	if (!biomes) return 2;
	if (configuration->startingLayerID >= L_NUM) return 3;
	if (biomesCapacity < (size_t)configuration->width * (size_t)configuration->height) return 4;

	int *const tempBuffer = (int *const)calloc(configuration->width * configuration->height, sizeof(*tempBuffer));

	// 1:8192 for Beta 1.8, 1:4096 for 1.0+, Large Biomes 1:16384
	// 0 = Ocean, 1 = Warm/Plains
	// Allows Ocean -> Warm/Plains
	if (biomesRequiredMargin) *biomesRequiredMargin = 0;
	islandLayer(biomes, 1, configuration);
	if (configuration->startingLayerID == L_CONTINENT_4096) {
		free(tempBuffer);
		return 0;
	}

	// (Fuzzy)
	// 1:4096 for Beta 1.8, 1:2048 for 1.0+, Large Biomes 1:8192
	// 0 = Ocean, 1 = Warm/Plains
	// Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, true, 2000, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_4096 : L_ZOOM_2048)) {
		free(tempBuffer);
		return 0;
	}

	// 1:4096 for Beta 1.8, 1:2048 for 1.0+, Large Biomes 1:8192
	// 0 = Ocean, 1 = Warm/Plains
	// Allows Ocean -> Warm/Plains, Warm/Plains -> Ocean
	if (biomesRequiredMargin) *biomesRequiredMargin += 1;
	addIslandLayer(biomes, tempBuffer, 1, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_4096 : L_LAND_2048)) {
		free(tempBuffer);
		return 0;
	}

	// 1:2048 for Beta 1.8, 1:1024 for 1.0+, Large Biomes 1:4096
	// 0 = Ocean, 1 = Warm/Plains
	// Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 2001, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_2048 : L_ZOOM_1024)) {
		free(tempBuffer);
		return 0;
	}

	// 1:2048 for Beta 1.8, 1:1024 for 1.0+, Large Biomes 1:4096
	// 0 = Ocean, 1 = Warm/Plains
	// Allows Ocean -> Warm/Plains, Warm/Plains -> Ocean
	if (biomesRequiredMargin) *biomesRequiredMargin += 1;
	addIslandLayer(biomes, tempBuffer, 2, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_2048 : L_LAND_1024_A)) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_7) {
		// 1:1024, Large Biomes 1:4096
		// 0 = Ocean, 1 = Warm
		// Allows Ocean -> Warm, Warm -> Ocean
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 50, configuration);
		if (configuration->startingLayerID == L_LAND_1024_B) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024, Large Biomes 1:4096
		// 0 = Ocean, 1 = Warm
		// Allows Ocean -> Warm, Warm -> Ocean
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 70, configuration);
		if (configuration->startingLayerID == L_LAND_1024_C) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024, Large Biomes 1:4096
		// 0 = Ocean, 1 = Warm
		// Allows Ocean -> Warm
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		removeTooMuchOceanLayer(biomes, tempBuffer, 2, configuration);
		if (configuration->startingLayerID == L_ISLAND_1024) {
			free(tempBuffer);
			return 0;
		}
	}

	if (configuration->version >= MC_1_0) {
		// 1:1024, Large Biomes 1:4096
		// 1.6-: 0 = Ocean, 1 = Plains, 12 = Snowy Tundra
		// 		Allows Plains -> Snowy Tundra
		// 1.7+: 0 = Ocean, 1 = Warm, 3 = Cold, 4 = Freezing
		// 		Allows Warm -> Cold, Warm -> Freezing
		addSnowLayer(biomes, 2, configuration);
		if (configuration->startingLayerID == L_SNOW_1024) {
			free(tempBuffer);
			return 0;
		}
	}

	if (configuration->version >= MC_1_7) {
		// 1:1024, Large Biomes 1:4096
		// 0 = Ocean, 1 = Warm, 3 = Cold, 4 = Freezing
		// 		Allows Ocean -> Warm, Ocean -> Cold, Ocean -> Freezing, Warm -> Ocean, Cold -> Ocean
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 3, configuration);
		if (configuration->startingLayerID == L_LAND_1024_D) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024, Large Biomes 1:4096
		// 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing
		// 		Allows Warm -> Lush
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addEdgeLayerCoolWarm(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_COOL_1024) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024, Large Biomes 1:4096
		// 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing
		// 		Allows Freezing -> Cold
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addEdgeLayerHeatIce(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_HEAT_1024) {
			free(tempBuffer);
			return 0;
		}

		// 1:1024, Large Biomes 1:4096
		// 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing, [256*[1...15] + 1] = Warm Special,
		// [256*[1...15] + 2] = Lush Special, [256*[1...15] + 3] = Cold Special,
		// [256*[1...15] + 4] = Freezing Special
		// 		Allows Warm -> Warm Special, Lush -> Lush Special, Cold -> Cold Special,
		// 		Freezing -> Freezing Special
		addEdgeLayerIntroduceSpecial(biomes, 3, configuration);
		if (configuration->startingLayerID == L_SPECIAL_1024) {
			free(tempBuffer);
			return 0;
		}
	}

	// 1:1024 for Beta 1.8, 1:512 for 1.0+, Large Biomes 1:2048
	// Beta 1.8: 0 = Ocean, 1 = Plains
	//		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0-1.6: 0 = Ocean, 1 = Plains, 12 = Snowy Tundra
	//		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7+: 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing, [256*[1...15] + 1] = Warm Special,
	// [256*[1...15] + 2] = Lush Special, [256*[1...15] + 3] = Cold Special, [256*[1...15] + 4] = Freezing Special
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 2002, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_1024 : L_ZOOM_512)) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version <= MC_1_6) {
		// 1:1024 for Beta 1.8, 1:512 for 1.0+, Large Biomes 1:2048
		// Beta 1.8: 0 = Ocean, 1 = Plains
		//		Allows Ocean -> Plains, Plains -> Ocean
		// 1.0-1.6: 0 = Ocean, 1 = Plains, 10 = Frozen Ocean, 12 = Snowy Tundra
		//		Allows Ocean -> Plains, Ocean -> Frozen Ocean, Ocean -> Snowy Tundra, Plains -> Ocean,
		// 		Frozen Ocean -> Ocean, Snowy Tundra -> Frozen Ocean
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 3, configuration);
		if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_LAND_1024_A : L_LAND_512)) {
			free(tempBuffer);
			return 0;
		}
	}

	// 1:512 for Beta 1.8, 1:256 for 1.0+, Large Biomes 1:1024
	// Beta 1.8: 0 = Ocean, 1 = Plains
	//		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0-1.6: 0 = Ocean, 1 = Plains, 10 = Frozen Ocean, 12 = Snowy Tundra
	//		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7+: 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing, [256*[1...15] + 1] = Warm Special,
	// [256*[1...15] + 2] = Lush Special, [256*[1...15] + 3] = Cold Special, [256*[1...15] + 4] = Freezing Special
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 2003, configuration);
	if (configuration->startingLayerID == (configuration->version == MC_B1_8 ? L_ZOOM_512 : L_ZOOM_256)) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version == MC_B1_8) {
		// 1:512
		// 0 = Ocean, 1 = Plains
		//		Allows Ocean -> Plains, Plains -> Ocean
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addIslandLayer(biomes, tempBuffer, 3, configuration);
		if (configuration->startingLayerID == L_LAND_512) {
			free(tempBuffer);
			return 0;
		}

		// 1:256
		// 0 = Ocean, 1 = Plains
		//		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
		zoomLayer(biomes, tempBuffer, false, 2004, configuration);
		if (configuration->startingLayerID == L_ZOOM_256) {
			free(tempBuffer);
			return 0;
		}
	}

	// 1:256, Large Biomes 1:1024
	// Beta 1.8: 0 = Ocean, 1 = Plains
	//		Allows Ocean -> Plains, Plains -> Ocean
	// 1.0-1.6: 0 = Ocean, 1 = Plains, 10 = Frozen Ocean, 12 = Snowy Tundra
	//		Allows Ocean -> Plains, Ocean -> Frozen Ocean, Ocean -> Snowy Tundra, Plains -> Ocean,
	// 		Frozen Ocean -> Ocean, Snowy Tundra -> Frozen Ocean
	// 1.7+: 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing, [256*[1...15] + 1] = Warm Special,
	// [256*[1...15] + 2] = Lush Special, [256*[1...15] + 3] = Cold Special, [256*[1...15] + 4] = Freezing Special
	//		Allows Ocean -> Warm, Ocean -> Lush, Ocean -> Cold, Ocean -> Freezing, Ocean -> Warm Special,
	// 		Ocean -> Lush Special, Ocean -> Cold Special, Ocean -> Freezing Special, Warm -> Ocean,
	// 		Lush -> Ocean, Cold -> Ocean, Warm Special -> Ocean, Lush Special -> Ocean, Cold Special -> Ocean,
	// 		Freezing Special -> Ocean
	if (biomesRequiredMargin) *biomesRequiredMargin += 1;
	addIslandLayer(biomes, tempBuffer, configuration->version == MC_B1_8 ? 3 : 4, configuration);
	if (configuration->startingLayerID == L_LAND_256) {
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_0) {
		// 1:256, Large Biomes 1:1024
		// 1.0-1.6: 0 = Ocean, 1 = Plains, 10 = Frozen Ocean, 12 = Snowy Tundra, 14 = Mushroom Fields
		//		Allows Ocean -> Mushroom Fields
		// 1.7+: 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing, 14 = Mushroom Fields,
		// [256*[1...15] + 1] = Warm Special, [256*[1...15] + 2] = Lush Special,
		// [256*[1...15] + 3] = Cold Special, [256*[1...15] + 4] = Freezing Special
		//		Allows Ocean -> Mushroom Fields
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addMushroomIslandLayer(biomes, tempBuffer, 5, configuration);
		if (configuration->startingLayerID == L_MUSHROOM_256) {
			free(tempBuffer);
			return 0;
		}
	}

	if (configuration->version >= MC_1_7) {
		// 1:256, Large Biomes 1:1024
		// 0 = Ocean, 1 = Warm, 2 = Lush, 3 = Cold, 4 = Freezing, 14 = Mushroom Fields, 24 = Deep Ocean,
		// [256*[1...15] + 1] = Warm Special, [256*[1...15] + 2] = Lush Special,
		// [256*[1...15] + 3] = Cold Special, [256*[1...15] + 4] = Freezing Special
		//		Allows Ocean -> Deep Ocean
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		addDeepOceanLayer(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_DEEP_OCEAN_256) {
			free(tempBuffer);
			return 0;
		}
	}

	// Copy current biomes layer to river noisemap, for generation later
	int *const riverNoise = (int *const)calloc(configuration->width * configuration->height, sizeof(*riverNoise));
	memcpy(riverNoise, biomes, configuration->width*configuration->height*sizeof(*biomes));
	int riverNoiseRequiredMargin = *biomesRequiredMargin;

	// 1:256, Large Biomes 1:1024
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	//		Allows Plains -> Desert, Plains -> Mountains, Plains -> Forest, Plains -> Taiga, Plains -> Swamp
	// 1.0-1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// {10 = Frozen Ocean}, 12 = Snowy Tundra, 14 = Mushroom Fields
	//		Allows Plains -> Desert, Plains -> Mountains, Plains -> Forest, Plains -> Taiga, Plains -> Swamp,
	// 		Frozen Ocean -> Snowy Tundra
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// {10 = Frozen Ocean}, 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle
	//		Allows Plains -> Desert, Plains -> Mountains, Plains -> Forest, Plains -> Taiga, Plains -> Swamp,
	// 		Plains -> Jungle, Frozen Ocean -> Snowy Tundra
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// {10 = Frozen Ocean}, 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle
	//		Allows Plains -> Desert, Plains -> Mountains, Plains -> Forest, Plains -> Taiga, Plains -> Swamp,
	// 		Plains -> Jungle, Frozen Ocean -> Taiga, Frozen Ocean -> Snowy Tundra, Snowy Tundra -> Taiga
	// 1.7+: 0 = Ocean, 1 = Warm/Plains, 2 = Lush/Desert, 3 = Cold/Mountains, 4 = Freezing/Forest, 5 = Taiga,
	// 6 = Swamp, 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 24 = Deep Ocean, 27 = Birch Forest, 
	// 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga, 35 = Savanna, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, {[256*[1...15] + 1] = Warm Special}, {[256*[1...15] + 2] = Lush Special},
	// {[256*[1...15] + 3] = Cold Special}, [256*[1...15] + 4] = Freezing Special}
	//		Allows Warm -> Plains, Warm -> Desert, Warm -> Savanna, Warm Special -> Wooded Badlands Plateau,
	// 		Warm Special -> Badlands Plateau, Lush -> Plains, Lush -> Mountains, Lush -> Forest, Lush -> Swamp,
	// 		Lush -> Birch Forest, Lush -> Dark Forest, Lush Special -> Jungle, Cold -> Plains,
	// 		Cold -> Mountains, Cold -> Forest, Cold -> Taiga, Cold Special -> Giant Tree Taiga,
	// 		Freezing -> Snowy Tundra, Freezing -> Snowy Taiga, Freezing Special -> Snowy Tundra,
	// 		Freezing Special -> Snowy Taiga
	biomeInitLayer(biomes, 200, configuration);
	if (configuration->startingLayerID == L_BIOME_256) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_14) {
		// 1:256, Large Biomes 1:1024
		// 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 24 = Deep Ocean, 27 = Birch Forest,
		// 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga, 35 = Savanna,
		// 38 = Wooded Badlands Plateau, 39 = Badlands Plateau, 168 = Bamboo Jungle
		//		Allows Jungle -> Bamboo Jungle
		addBambooLayer(biomes, 1001, configuration);
		if (configuration->startingLayerID == L_BAMBOO_256) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:128, Large Biomes 1:512
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0-1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 24 = Deep Ocean, 27 = Birch Forest,
	// 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga, 35 = Savanna, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 24 = Deep Ocean, 27 = Birch Forest,
	// 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga, 35 = Savanna, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 168 = Bamboo Jungle
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1000, configuration);
	if (configuration->startingLayerID == L_ZOOM_128) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:64, Large Biomes 1:256
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0-1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 24 = Deep Ocean, 27 = Birch Forest,
	// 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga, 35 = Savanna, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 24 = Deep Ocean, 27 = Birch Forest,
	// 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga, 35 = Savanna, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 168 = Bamboo Jungle
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1001, configuration);
	if (configuration->startingLayerID == L_ZOOM_64) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_7) {
		// 1:64, Large Biomes 1:256
		// 1.7-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 23 = Jungle Edge, 24 = Deep Ocean,
		// 27 = Birch Forest, 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga,
		// 34 = Wooded Mountains, 35 = Savanna, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau
		// 		Allows Desert -> Wooded Mountains, Swamp -> Plains, Swamp -> Jungle Edge,
		// 		Giant Tree Taiga -> Taiga, Wooded Badlands Plateau -> Badlands, Badlands Plateau -> Badlands
		// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 14 = Mushroom Fields, 21 = Jungle, 23 = Jungle Edge, 24 = Deep Ocean,
		// 27 = Birch Forest, 29 = Dark Forest, 30 = Snowy Taiga, 32 = Giant Tree Taiga,
		// 34 = Wooded Mountains, 35 = Savanna, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 168 = Bamboo Jungle
		// 		Allows Desert -> Wooded Mountains, Swamp -> Plains, Swamp -> Jungle Edge,
		// 		Giant Tree Taiga -> Taiga, Wooded Badlands Plateau -> Badlands, Badlands Plateau -> Badlands
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		biomeEdgeLayer(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_BIOME_EDGE_64) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// ------------------------------------
	// RIVERS AND HILLS
	// ------------------------------------

	// 1:256, Large Biomes 1:1024
	// Beta 1.8 - 1.6: 0 = Ocean, 2/3 = River Noise
	// 		Allows all non-Ocean -> River Noise
	// 1.7+: 0 = Ocean, [2...300001] = River Noise
	// 		Allows all non-Ocean -> River Noise
	riverInitLayer(riverNoise, 100, configuration);
	if (configuration->startingLayerID == L_NOISE_256) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	int *hillsNoise;
	int *hillsNoiseRequiredMargin, _riverMarginCopy = riverNoiseRequiredMargin;
	// Hills noise zoom's salt differs from river noise zoom's salt in 1.1-1.12, so a distinct copy must be made then. Otherwise, they are identical and we can reuse the same noisemap + margin counter.
	if (configuration->version >= MC_1_1 && configuration->version <= MC_1_12) { // (This would be 1.7-1.12 if not for Cubiomes)
		// Copy current rivers layer to hills noisemap
		hillsNoise = (int *)calloc(configuration->width * configuration->height, sizeof(*hillsNoise));
		memcpy(hillsNoise, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		hillsNoiseRequiredMargin = &_riverMarginCopy;
	} else {
		hillsNoise = riverNoise;
		hillsNoiseRequiredMargin = &riverNoiseRequiredMargin;
	}

	if (configuration->version >= MC_1_1 && configuration->version <= MC_1_12) { // (This would be 1.7-1.12 if not for Cubiomes)
		// 1:128, Large Biomes 1:512
		// 1.1-1.6: 0 = Ocean, 2/3 = Hills Noise
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.7-1.12: 0 = Ocean, [2...300001] = Hills Noise
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		*hillsNoiseRequiredMargin = ceil(*hillsNoiseRequiredMargin/2.);
		// Worldseed-independent for 1.1-1.12
		zoomLayer(hillsNoise, tempBuffer, false, 0, configuration);
		if (configuration->startingLayerID == L_ZOOM_128_HILLS) {
			memcpy(biomes, hillsNoise, configuration->width*configuration->height*sizeof(*hillsNoise));
			*biomesRequiredMargin = *hillsNoiseRequiredMargin;
			free(hillsNoise);
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:64, Large Biomes 1:256
		// 1.1-1.6: 0 = Ocean, 2/3 = Hills Noise
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.7-1.12: 0 = Ocean, [2...300001] = Hills Noise
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		*hillsNoiseRequiredMargin = ceil(*hillsNoiseRequiredMargin/2.);
		// Worldseed-independent for 1.1-1.12
		zoomLayer(hillsNoise, tempBuffer, false, 0, configuration);
		if (configuration->startingLayerID == L_ZOOM_64_HILLS) {
			memcpy(biomes, hillsNoise, configuration->width*configuration->height*sizeof(*hillsNoise));
			*biomesRequiredMargin = *hillsNoiseRequiredMargin;
			free(hillsNoise);
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:128, 1.7- Large Biomes 1:512
	// Beta 1.8 - 1.0: 0 = Ocean, 2/3 = River/Hills Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.1-1.6: 0 = Ocean, 2/3 = River Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.12: 0 = Ocean, [2...300001] = River Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.13+: 0 = Ocean, [2...300001] = River/Hills Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
	zoomLayer(riverNoise, tempBuffer, false, 1000, configuration);
	// 1.1-1.12 Zoom 128 Hills case already returned above
	if (configuration->startingLayerID == L_ZOOM_128_HILLS || configuration->startingLayerID == L_ZOOM_128_RIVER) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		if (configuration->version >= MC_1_1 && configuration->version <= MC_1_12) free(hillsNoise); // (This would be 1.7-1.12 if not for Cubiomes)
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:64, 1.7- Large Biomes 1:256
	// Beta 1.8 - 1.0: 0 = Ocean, 2/3 = River/Hills Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.1-1.6: 0 = Ocean, 2/3 = River Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.12: 0 = Ocean, [2...300001] = River Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.13+: 0 = Ocean, [2...300001] = River/Hills Noise
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
	zoomLayer(riverNoise, tempBuffer, false, 1001, configuration);
	// 1.1-1.12 Zoom 64 Hills case already returned above
	if (configuration->startingLayerID == L_ZOOM_64_HILLS || configuration->startingLayerID == L_ZOOM_64_RIVER) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		if (configuration->version >= MC_1_1 && configuration->version <= MC_1_12) free(hillsNoise); // (This would be 1.7-1.12 if not for Cubiomes)
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_1) {
		// 1:64, 1.7- Large Biomes 1:256
		// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills
		// 		Allows Plains -> Forest, Desert -> Desert Hills, Forest -> Wooded Hills,
		// 		Taiga -> Taiga Hills, Snowy Tundras -> Snowy Mountains
		// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
		// 		Allows Plains -> Forest, Desert -> Desert Hills, Forest -> Wooded Hills,
		// 		Taiga -> Taiga Hills, Snowy Tundras -> Snowy Mountains, Jungle -> Jungle Hills
		// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
		// 		Allows Plains -> Forest, Desert -> Desert Hills, Forest -> Wooded Hills,
		// 		Taiga -> Taiga Hills, Snowy Tundras -> Snowy Mountains, Jungle -> Jungle Hills
		// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
		// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
		// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest,
		// 156 = Tall Birch Hills, 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains,
		// 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains,
		// 163 = Shattered Savanna, 164 = Shattered Savanna Plateau, 165 = Eroded Badlands,
		// 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows Ocean -> Deep Ocean, Plains -> Forest, Plains -> Wooded Hills,
		// 		Plains -> Sunflower Plains, Plains -> Flower Forest, Desert -> Desert Hills,
		// 		Desert -> Desert Lakes, Mountains -> Wooded Mountains, Mountains -> Gravelly Mountains,
		// 		Mountains -> Modified Gravelly Mountains, Forest -> Wooded Hills, Forest -> Flower Forest, 
		// 		Taiga -> Taiga Hills, Taiga -> Taiga Mountains, Swamp -> Swamp Hills,
		// 		Snowy Tundra -> Snowy Mountains, Snowy Tundra -> Ice Spikes, Jungle -> Jungle Hills,
		// 		Jungle -> Modified Jungle, Jungle Edge -> Modified Jungle Edge, Deep Ocean -> Plains,
		// 		Deep Ocean -> Forest, Deep Ocean -> Sunflower Plains, Deep Ocean -> Flower Forest,
		// 		Birch Forest -> Birch Forest Hills, Birch Forest -> Tall Birch Forest,
		// 		Birch Forest -> Tall Birch Hills, Dark Forest -> Plains, Dark Forest -> Sunflower Plains, 
		// 		Dark Forest -> Dark Forest Hills, Snowy Taiga -> Snowy Taiga Hills,
		// 		Snowy Taiga -> Snowy Taiga Mountains, Giant Tree Taiga -> Giant Tree Taiga Hills,
		// 		Giant Tree Taiga -> Giant Spruce Taiga, Giant Tree Taiga -> Giant Spruce Taiga Hills,
		// 		Wooded Mountains -> Modified Gravelly Mountains, Savanna -> Savanna Plateau,
		// 		Savanna -> Shattered Savanna, Savanna -> Shattered Savanna Plateau,
		// 		Badlands -> Eroded Badlands, Wooded Badlands Plateau -> Badlands,
		// 		Wooded Badlands Plateau -> Eroded Badlands,
		// 		Wooded Badlands Plateau -> Modified Wooded Badlands Plateau, Badlands Plateau -> Badlands,
		// 		Badlands Plateau -> Eroded Badlands, Badlands Plateau -> Modified Badlands Plateau
		// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
		// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 156 = Tall Birch Hills,
		// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
		// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
		// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
		// 167 = Modified Badlands Plateau
		// 		Allows Ocean -> Deep Ocean, Plains -> Forest, Plains -> Wooded Hills,
		// 		Plains -> Sunflower Plains, Plains -> Flower Forest, Desert -> Desert Hills,
		// 		Desert -> Desert Lakes, Mountains -> Wooded Mountains, Mountains -> Gravelly Mountains,
		// 		Mountains -> Modified Gravelly Mountains, Forest -> Wooded Hills, Forest -> Flower Forest, 
		// 		Taiga -> Taiga Hills, Taiga -> Taiga Mountains, Swamp -> Swamp Hills,
		// 		Snowy Tundra -> Snowy Mountains, Snowy Tundra -> Ice Spikes, Jungle -> Jungle Hills,
		// 		Jungle -> Modified Jungle, Jungle Edge -> Modified Jungle Edge, Deep Ocean -> Plains,
		// 		Deep Ocean -> Forest, Deep Ocean -> Sunflower Plains, Deep Ocean -> Flower Forest,
		// 		Birch Forest -> Birch Forest Hills, Birch Forest -> Tall Birch Hills, Dark Forest -> Plains, 
		// 		Dark Forest -> Sunflower Plains, Dark Forest -> Dark Forest Hills,
		// 		Snowy Taiga -> Snowy Taiga Hills, Snowy Taiga -> Snowy Taiga Mountains,
		// 		Giant Tree Taiga -> Giant Tree Taiga Hills, Giant Tree Taiga -> Giant Spruce Taiga,
		// 		Giant Tree Taiga -> Giant Spruce Taiga Hills,
		// 		Wooded Mountains -> Modified Gravelly Mountains, Savanna -> Savanna Plateau,
		// 		Savanna -> Shattered Savanna, Savanna -> Shattered Savanna Plateau,
		// 		Badlands -> Eroded Badlands, Wooded Badlands Plateau -> Badlands,
		// 		Wooded Badlands Plateau -> Eroded Badlands,
		// 		Wooded Badlands Plateau -> Modified Wooded Badlands Plateau, Badlands Plateau -> Badlands,
		// 		Badlands Plateau -> Eroded Badlands, Badlands Plateau -> Modified Badlands Plateau
		// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
		// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest,
		// 156 = Tall Birch Hills, 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains,
		// 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains,
		// 163 = Shattered Savanna, 164 = Shattered Savanna Plateau, 165 = Eroded Badlands,
		// 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau, 168 = Bamboo Jungle,
		// 169 = Bamboo Jungle Hills
		// 		Allows Ocean -> Deep Ocean, Plains -> Forest, Plains -> Wooded Hills,
		// 		Plains -> Sunflower Plains, Plains -> Flower Forest, Desert -> Desert Hills,
		// 		Desert -> Desert Lakes, Mountains -> Wooded Mountains, Mountains -> Gravelly Mountains,
		// 		Mountains -> Modified Gravelly Mountains, Forest -> Wooded Hills, Forest -> Flower Forest, 
		// 		Taiga -> Taiga Hills, Taiga -> Taiga Mountains, Swamp -> Swamp Hills,
		// 		Snowy Tundra -> Snowy Mountains, Snowy Tundra -> Ice Spikes, Jungle -> Jungle Hills,
		// 		Jungle -> Modified Jungle, Jungle Edge -> Modified Jungle Edge, Deep Ocean -> Plains,
		// 		Deep Ocean -> Forest, Deep Ocean -> Sunflower Plains, Deep Ocean -> Flower Forest,
		// 		Birch Forest -> Birch Forest Hills, Birch Forest -> Tall Birch Forest,
		// 		Birch Forest -> Tall Birch Hills, Dark Forest -> Plains, Dark Forest -> Sunflower Plains, 
		// 		Dark Forest -> Dark Forest Hills, Snowy Taiga -> Snowy Taiga Hills,
		// 		Snowy Taiga -> Snowy Taiga Mountains, Giant Tree Taiga -> Giant Tree Taiga Hills,
		// 		Giant Tree Taiga -> Giant Spruce Taiga, Giant Tree Taiga -> Giant Spruce Taiga Hills,
		// 		Wooded Mountains -> Modified Gravelly Mountains, Savanna -> Savanna Plateau,
		// 		Savanna -> Shattered Savanna, Savanna -> Shattered Savanna Plateau,
		// 		Badlands -> Eroded Badlands, Wooded Badlands Plateau -> Badlands,
		// 		Wooded Badlands Plateau -> Eroded Badlands,
		// 		Wooded Badlands Plateau -> Modified Wooded Badlands Plateau, Badlands Plateau -> Badlands,
		// 		Badlands Plateau -> Eroded Badlands, Badlands Plateau -> Modified Badlands Plateau,
		// 		Bamboo Jungle -> Bamboo Jungle Hills
		if (biomesRequiredMargin) {
			if (*biomesRequiredMargin < *hillsNoiseRequiredMargin) *biomesRequiredMargin = *hillsNoiseRequiredMargin;
			*biomesRequiredMargin += 1;
		}
		regionHillsLayer(biomes, hillsNoise, tempBuffer, 1000, configuration);
		if (configuration->startingLayerID == L_HILLS_64) {
			if (configuration->version <= MC_1_12) free(hillsNoise); // (This would be 1.7-1.12 if not for Cubiomes)
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// Hills noise is never used again (river noise is though)
	if (configuration->version >= MC_1_1 && configuration->version <= MC_1_12) { // (This would be 1.7-1.12 if not for Cubiomes)
		free(hillsNoise);
	}

	// ======================================
	// BIOMES II
	// ======================================

	// 1:64, Large Biomes 1:256
	if (configuration->version >= MC_1_7) {
		// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
		// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
		// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest,
		// 156 = Tall Birch Hills, 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains,
		// 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains,
		// 163 = Shattered Savanna, 164 = Shattered Savanna Plateau, 165 = Eroded Badlands,
		// 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows Plains -> Sunflower Plains
		// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
		// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 156 = Tall Birch Hills,
		// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
		// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
		// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
		// 167 = Modified Badlands Plateau
		// 		Allows Plains -> Sunflower Plains
		// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
		// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
		// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest,
		// 156 = Tall Birch Hills, 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains,
		// 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains,
		// 163 = Shattered Savanna, 164 = Shattered Savanna Plateau, 165 = Eroded Badlands,
		// 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau, 168 = Bamboo Jungle,
		// 169 = Bamboo Jungle Hills
		// 		Allows Plains -> Sunflower Plains
		addSunflowerLayer(biomes, 1001, configuration);
		if (configuration->startingLayerID == L_SUNFLOWER_64) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:32, Large Biomes 1:128
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 14 = Mushroom Fields
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills, 
	// 19 = Taiga Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills,
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills, 
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
	// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
	// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
	// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills, 
	// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
	// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
	// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
	// 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills, 
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean,
	// 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills, 
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills,
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean,
	// 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills,
	// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
	// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
	// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
	// 167 = Modified Badlands Plateau, 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1000, configuration);
	if (configuration->startingLayerID == L_ZOOM_32) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:32, Large Biomes 1:128
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	// 		Allows Ocean -> Plains, Plains -> Ocean
	// 1.0: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 14 = Mushroom Fields
	// 		Allows Ocean -> Plains, Ocean -> Desert, Ocean -> Mountains, Ocean -> Forest, Ocean -> Taiga,
	// 		Ocean -> Swamp, Ocean -> Frozen Ocean, Ocean -> Snowy Tundra, Ocean -> Mushroom Fields,
	// 		Plains -> Ocean, Desert -> Ocean, Mountains -> Ocean, Forest -> Ocean, Taiga -> Ocean,
	// 		Swamp -> Ocean, Snowy Tundra -> Frozen Ocean, Mushroom Fields -> Ocean
	// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 
	// 18 = Wooded Hills, 19 = Taiga Hills
	// 		Allows Ocean -> Plains, Ocean -> Desert, Ocean -> Mountains, Ocean -> Forest, Ocean -> Taiga,
	// 		Ocean -> Swamp, Ocean -> Frozen Ocean, Ocean -> Snowy Tundra, Ocean -> Snowy Mountains,
	// 		Ocean -> Mushroom Fields, Ocean -> Desert Hills, Ocean -> Wooded Hills, Ocean -> Taiga Hills,
	// 		Plains -> Ocean, Desert -> Ocean, Mountains -> Ocean, Forest -> Ocean, Taiga -> Ocean,
	// 		Swamp -> Ocean, Snowy Tundra -> Frozen Ocean, Snowy Mountains -> Ocean, Mushroom Fields -> Ocean, 
	// 		Desert Hills -> Ocean, Wooded Hills -> Ocean, Taiga Hills -> Ocean
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
	// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
	// 		Allows Ocean -> Plains, Ocean -> Desert, Ocean -> Mountains, Ocean -> Forest, Ocean -> Taiga,
	// 		Ocean -> Swamp, Ocean -> Frozen Ocean, Ocean -> Snowy Tundra, Ocean -> Snowy Mountains,
	// 		Ocean -> Mushroom Fields, Ocean -> Desert Hills, Ocean -> Wooded Hills, Ocean -> Taiga Hills,
	// 		Ocean -> Jungle, Ocean -> Jungle Hills, Plains -> Ocean, Desert -> Ocean, Mountains -> Ocean,
	// 		Forest -> Ocean, Taiga -> Ocean, Swamp -> Ocean, Snowy Tundra -> Frozen Ocean,
	// 		Snowy Mountains -> Ocean, Mushroom Fields -> Ocean, Desert Hills -> Ocean, Wooded Hills -> Ocean, 
	// 		Taiga Hills -> Ocean, Jungle -> Ocean, Jungle Hills -> Ocean
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
	// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
	// 		Allows Ocean -> Plains, Ocean -> Desert, Ocean -> Mountains, Ocean -> Forest, Ocean -> Taiga,
	// 		Ocean -> Swamp, Ocean -> Frozen Ocean, Ocean -> Snowy Tundra, Ocean -> Snowy Mountains,
	// 		Ocean -> Mushroom Fields, Ocean -> Desert Hills, Ocean -> Wooded Hills, Ocean -> Taiga Hills,
	// 		Ocean -> Jungle, Ocean -> Jungle Hills, Plains -> Ocean, Desert -> Ocean, Mountains -> Ocean,
	// 		Forest -> Ocean, Taiga -> Ocean, Swamp -> Ocean, Snowy Tundra -> Frozen Ocean,
	// 		Snowy Mountains -> Ocean, Mushroom Fields -> Ocean, Desert Hills -> Ocean, Wooded Hills -> Ocean, 
	// 		Taiga Hills -> Ocean, Jungle -> Ocean, Jungle Hills -> Ocean
	// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
	// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
	// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
	// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills, 
	// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
	// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
	// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
	// 167 = Modified Badlands Plateau
	// 		Allows Ocean -> Plains, Ocean -> Desert, Ocean -> Mountains, Ocean -> Forest, Ocean -> Taiga,
	// 		Ocean -> Swamp, Ocean -> Snowy Tundra, Ocean -> Snowy Mountains, Ocean -> Mushroom Fields,
	// 		Ocean -> Desert Hills, Ocean -> Wooded Hills, Ocean -> Taiga Hills, Ocean -> Jungle,
	// 		Ocean -> Jungle Hills, Ocean -> Jungle Edge, Ocean -> Deep Ocean, Ocean -> Birch Forest,
	// 		Ocean -> Birch Forest Hills, Ocean -> Dark Forest, Ocean -> Snowy Taiga,
	// 		Ocean -> Snowy Taiga Hills, Ocean -> Giant Tree Taiga, Ocean -> Giant Tree Taiga Hills,
	// 		Ocean -> Wooded Mountains,  Ocean -> Savanna, Ocean -> Savanna Plateau, Ocean -> Badlands,
	// 		Ocean -> Wooded Badlands Plateau, Ocean -> Badlands Plateau, Ocean -> Sunflower Plains,
	// 		Ocean -> Desert Lakes, Ocean -> Gravelly Mountains, Ocean -> Flower Forest,
	// 		Ocean -> Taiga Mountains, Ocean -> Swamp Hills, Ocean -> Ice Spikes, Ocean -> Modified Jungle,
	// 		Ocean -> Modified Jungle Edge, Ocean -> Tall Birch Forest, Ocean -> Tall Birch Hills,
	// 		Ocean -> Dark Forest Hills, Ocean -> Snowy Taiga Mountains, Ocean -> Giant Spruce Taiga,
	// 		Ocean -> Giant Spruce Taiga Hills, Ocean -> Modified Gravelly Mountains,
	// 		Ocean -> Shattered Savanna, Ocean -> Shattered Savanna Plateau, Ocean -> Eroded Badlands,
	// 		Ocean -> Modified Wooded Badlands Plateau, Ocean -> Modified Badlands Plateau, Plains -> Ocean,
	// 		Desert -> Ocean, Mountains -> Ocean, Taiga -> Ocean, Swamp -> Ocean, Snowy Tundra -> Ocean,
	// 		Snowy Mountains -> Ocean, Mushroom Fields -> Ocean, Desert Hills -> Ocean, Wooded Hills -> Ocean,
	// 		Taiga Hills -> Ocean, Jungle -> Ocean, Jungle Hills -> Ocean, Jungle Edge -> Ocean,
	// 		Deep Ocean -> Ocean, Birch Forest -> Ocean, Birch Forest Hills -> Ocean, Dark Forest -> Ocean,
	// 		Snowy Taiga -> Ocean, Snowy Taiga Hills -> Ocean, Giant Tree Taiga -> Ocean,
	// 		Giant Tree Taiga Hills -> Ocean, Wooded Mountains -> Ocean,  Savanna -> Ocean,
	// 		Savanna Plateau -> Ocean, Badlands -> Ocean, Wooded Badlands Plateau -> Ocean,
	// 		Badlands Plateau -> Ocean, Sunflower Plains -> Ocean, Desert Lakes -> Ocean,
	// 		Gravelly Mountains -> Ocean, Flower Forest -> Ocean, Taiga Mountains -> Ocean,
	// 		Swamp Hills -> Ocean, Ice Spikes -> Ocean, Modified Jungle -> Ocean,
	// 		Modified Jungle Edge -> Ocean, Tall Birch Forest -> Ocean, Tall Birch Hills -> Ocean,
	// 		Dark Forest Hills -> Ocean, Snowy Taiga Mountains -> Ocean, Giant Spruce Taiga -> Ocean,
	// 		Giant Spruce Taiga Hills -> Ocean, Modified Gravelly Mountains -> Ocean,
	// 		Shattered Savanna -> Ocean, Shattered Savanna Plateau -> Ocean, Eroded Badlands -> Ocean,
	// 		Modified Wooded Badlands Plateau -> Ocean, Modified Badlands Plateau -> Ocean
	// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills, 
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean,
	// 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills, 
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
	// 		Allows Ocean -> Plains, Ocean -> Desert, Ocean -> Mountains, Ocean -> Forest, Ocean -> Taiga,
	// 		Ocean -> Swamp, Ocean -> Snowy Tundra, Ocean -> Snowy Mountains, Ocean -> Mushroom Fields,
	// 		Ocean -> Desert Hills, Ocean -> Wooded Hills, Ocean -> Taiga Hills, Ocean -> Jungle,
	// 		Ocean -> Jungle Hills, Ocean -> Jungle Edge, Ocean -> Deep Ocean, Ocean -> Birch Forest,
	// 		Ocean -> Birch Forest Hills, Ocean -> Dark Forest, Ocean -> Snowy Taiga,
	// 		Ocean -> Snowy Taiga Hills, Ocean -> Giant Tree Taiga, Ocean -> Giant Tree Taiga Hills,
	// 		Ocean -> Wooded Mountains,  Ocean -> Savanna, Ocean -> Savanna Plateau, Ocean -> Badlands,
	// 		Ocean -> Wooded Badlands Plateau, Ocean -> Badlands Plateau, Ocean -> Sunflower Plains,
	// 		Ocean -> Desert Lakes, Ocean -> Gravelly Mountains, Ocean -> Flower Forest,
	// 		Ocean -> Taiga Mountains, Ocean -> Swamp Hills, Ocean -> Ice Spikes, Ocean -> Modified Jungle,
	// 		Ocean -> Modified Jungle Edge, Ocean -> Tall Birch Hills, Ocean -> Dark Forest Hills,
	// 		Ocean -> Snowy Taiga Mountains, Ocean -> Giant Spruce Taiga, Ocean -> Giant Spruce Taiga Hills, 
	// 		Ocean -> Modified Gravelly Mountains, Ocean -> Shattered Savanna,
	// 		Ocean -> Shattered Savanna Plateau, Ocean -> Eroded Badlands,
	// 		Ocean -> Modified Wooded Badlands Plateau, Ocean -> Modified Badlands Plateau, Plains -> Ocean,
	// 		Desert -> Ocean, Mountains -> Ocean, Taiga -> Ocean, Swamp -> Ocean, Snowy Tundra -> Ocean,
	// 		Snowy Mountains -> Ocean, Mushroom Fields -> Ocean, Desert Hills -> Ocean, Wooded Hills -> Ocean,
	// 		Taiga Hills -> Ocean, Jungle -> Ocean, Jungle Hills -> Ocean, Jungle Edge -> Ocean,
	// 		Deep Ocean -> Ocean, Birch Forest -> Ocean, Birch Forest Hills -> Ocean, Dark Forest -> Ocean,
	// 		Snowy Taiga -> Ocean, Snowy Taiga Hills -> Ocean, Giant Tree Taiga -> Ocean,
	// 		Giant Tree Taiga Hills -> Ocean, Wooded Mountains -> Ocean,  Savanna -> Ocean,
	// 		Savanna Plateau -> Ocean, Badlands -> Ocean, Wooded Badlands Plateau -> Ocean,
	// 		Badlands Plateau -> Ocean, Sunflower Plains -> Ocean, Desert Lakes -> Ocean,
	// 		Gravelly Mountains -> Ocean, Flower Forest -> Ocean, Taiga Mountains -> Ocean,
	// 		Swamp Hills -> Ocean, Ice Spikes -> Ocean, Modified Jungle -> Ocean,
	// 		Modified Jungle Edge -> Ocean, Tall Birch Hills -> Ocean, Dark Forest Hills -> Ocean,
	// 		Snowy Taiga Mountains -> Ocean, Giant Spruce Taiga -> Ocean, Giant Spruce Taiga Hills -> Ocean,
	// 		Modified Gravelly Mountains -> Ocean, Shattered Savanna -> Ocean,
	// 		Shattered Savanna Plateau -> Ocean, Eroded Badlands -> Ocean,
	// 		Modified Wooded Badlands Plateau -> Ocean, Modified Badlands Plateau -> Ocean
	// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills,
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean,
	// 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills,
	// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
	// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
	// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
	// 167 = Modified Badlands Plateau, 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
	// 		Allows Ocean -> Plains, Ocean -> Desert, Ocean -> Mountains, Ocean -> Forest, Ocean -> Taiga,
	// 		Ocean -> Swamp, Ocean -> Snowy Tundra, Ocean -> Snowy Mountains, Ocean -> Mushroom Fields,
	// 		Ocean -> Desert Hills, Ocean -> Wooded Hills, Ocean -> Taiga Hills, Ocean -> Jungle,
	// 		Ocean -> Jungle Hills, Ocean -> Jungle Edge, Ocean -> Deep Ocean, Ocean -> Birch Forest,
	// 		Ocean -> Birch Forest Hills, Ocean -> Dark Forest, Ocean -> Snowy Taiga,
	// 		Ocean -> Snowy Taiga Hills, Ocean -> Giant Tree Taiga, Ocean -> Giant Tree Taiga Hills,
	// 		Ocean -> Wooded Mountains,  Ocean -> Savanna, Ocean -> Savanna Plateau, Ocean -> Badlands,
	// 		Ocean -> Wooded Badlands Plateau, Ocean -> Badlands Plateau, Ocean -> Sunflower Plains,
	// 		Ocean -> Desert Lakes, Ocean -> Gravelly Mountains, Ocean -> Flower Forest,
	// 		Ocean -> Taiga Mountains, Ocean -> Swamp Hills, Ocean -> Ice Spikes, Ocean -> Modified Jungle,
	// 		Ocean -> Modified Jungle Edge, Ocean -> Tall Birch Forest, Ocean -> Tall Birch Hills,
	// 		Ocean -> Dark Forest Hills, Ocean -> Snowy Taiga Mountains, Ocean -> Giant Spruce Taiga,
	// 		Ocean -> Giant Spruce Taiga Hills, Ocean -> Modified Gravelly Mountains,
	// 		Ocean -> Shattered Savanna, Ocean -> Shattered Savanna Plateau, Ocean -> Eroded Badlands,
	// 		Ocean -> Modified Wooded Badlands Plateau, Ocean -> Modified Badlands Plateau,
	// 		Ocean -> Bamboo Jungle, Ocean -> Bamboo Jungle Hills, Plains -> Ocean, Desert -> Ocean,
	// 		Mountains -> Ocean, Taiga -> Ocean, Swamp -> Ocean, Snowy Tundra -> Ocean,
	// 		Snowy Mountains -> Ocean, Mushroom Fields -> Ocean, Desert Hills -> Ocean, Wooded Hills -> Ocean,
	// 		Taiga Hills -> Ocean, Jungle -> Ocean, Jungle Hills -> Ocean, Jungle Edge -> Ocean,
	// 		Deep Ocean -> Ocean, Birch Forest -> Ocean, Birch Forest Hills -> Ocean, Dark Forest -> Ocean,
	// 		Snowy Taiga -> Ocean, Snowy Taiga Hills -> Ocean, Giant Tree Taiga -> Ocean,
	// 		Giant Tree Taiga Hills -> Ocean, Wooded Mountains -> Ocean,  Savanna -> Ocean,
	// 		Savanna Plateau -> Ocean, Badlands -> Ocean, Wooded Badlands Plateau -> Ocean,
	// 		Badlands Plateau -> Ocean, Sunflower Plains -> Ocean, Desert Lakes -> Ocean,
	// 		Gravelly Mountains -> Ocean, Flower Forest -> Ocean, Taiga Mountains -> Ocean,
	// 		Swamp Hills -> Ocean, Ice Spikes -> Ocean, Modified Jungle -> Ocean,
	// 		Modified Jungle Edge -> Ocean, Tall Birch Forest -> Ocean, Tall Birch Hills -> Ocean,
	// 		Dark Forest Hills -> Ocean, Snowy Taiga Mountains -> Ocean, Giant Spruce Taiga -> Ocean,
	// 		Giant Spruce Taiga Hills -> Ocean, Modified Gravelly Mountains -> Ocean,
	// 		Shattered Savanna -> Ocean, Shattered Savanna Plateau -> Ocean, Eroded Badlands -> Ocean,
	// 		Modified Wooded Badlands Plateau -> Ocean, Modified Badlands Plateau -> Ocean,
	// 		Bamboo Jungle -> Ocean, Bamboo Jungle Hills -> Ocean
	if (biomesRequiredMargin) *biomesRequiredMargin += 1;
	addIslandLayer(biomes, tempBuffer, 3, configuration);
	if (configuration->startingLayerID == L_LAND_32) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// Beta 1.8 doesn't actually have a Shore layer that changes anything...
	if (configuration->version == MC_B1_8 && configuration->startingLayerID == L_SHORE_16) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}
	if (configuration->version == MC_1_0) {
		// 1:32
		// 1.0: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 10 = Frozen Ocean, 12 = Snowy Tundra, 14 = Mushroom Fields, 15 = Mushroom Field Shore
		// 		Allows Mushroom Fields -> Mushroom Field Shore
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		shoreLayer(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_SHORE_16) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:16, Large Biomes 1:64
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 14 = Mushroom Fields, 15 = Mushroom Field Shore
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 
	// 18 = Wooded Hills, 19 = Taiga Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
	// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
	// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
	// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills,
	// 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge,
	// 24 = Deep Ocean, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills, 
	// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
	// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
	// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
	// 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills, 
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean,
	// 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills, 
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 17 = Desert Hills, 18 = Wooded Hills,
	// 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean,
	// 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
	// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 
	// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
	// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
	// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
	// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills,
	// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
	// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
	// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
	// 167 = Modified Badlands Plateau, 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1001, configuration);
	if (configuration->startingLayerID == L_ZOOM_16) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_1) {
		// 1:16, Large Biomes 1:64
		// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge
		// 		Allows Plains -> Beach, Desert -> Beach, Mountains -> Mountain Edge, Forest -> Beach, 
		// 		Taiga -> Beach, Frozen Ocean -> Beach, Snowy Tundra -> Beach, Snowy Mountains -> Beach, 
		// 		Mushroom Fields -> Mushroom Field Shore, Desert Hills -> Beach, Wooded Hills -> Beach,
		// 		Taiga Hills -> Beach
		// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
		// 		Allows Plains -> Beach, Desert -> Beach, Mountains -> Mountain Edge, Forest -> Beach, 
		// 		Taiga -> Beach, Frozen Ocean -> Beach, Snowy Tundra -> Beach, Snowy Mountains -> Beach, 
		// 		Mushroom Fields -> Mushroom Field Shore, Desert Hills -> Beach, Wooded Hills -> Beach,
		// 		Taiga Hills -> Beach, Jungle -> Beach, Jungle Hills -> Beach
		// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
		// 		Allows Plains -> Beach, Desert -> Beach, Mountains -> Mountain Edge, Forest -> Beach, 
		// 		Taiga -> Beach, Frozen Ocean -> Beach, Snowy Tundra -> Beach, Snowy Mountains -> Beach, 
		// 		Mushroom Fields -> Mushroom Field Shore, Desert Hills -> Beach, Wooded Hills -> Beach,
		// 		Taiga Hills -> Beach, Jungle -> Beach, Jungle Hills -> Beach
		// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
		// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore,
		// 26 = Snowy Beach, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains,
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest,
		// 156 = Tall Birch Hills, 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains,
		// 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains,
		// 163 = Shattered Savanna, 164 = Shattered Savanna Plateau, 165 = Eroded Badlands,
		// 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows Plains -> Beach, Desert -> Beach, Mountains -> Stone Shore, Forest -> Beach,
		// 		Taiga -> Beach, Snowy Tundra -> Snowy Beach, Snowy Mountains -> Snowy Beach,
		// 		Mushroom Fields -> Mushroom Field Shore, Desert Hills -> Beach, Wooded Hills -> Beach,
		// 		Taiga Hills -> Beach, Jungle -> Beach, Jungle -> Jungle Edge, Jungle Hills -> Beach,
		// 		Jungle Hills -> Jungle Edge, Jungle Edge -> Beach, Birch Forest -> Beach,
		// 		Birch Forest Hills -> Beach, Dark Forest -> Beach, Snowy Taiga -> Snowy Beach,
		// 		Snowy Taiga Hills -> Snowy Beach, Giant Tree Taiga -> Beach, Giant Tree Taiga Hills -> Beach,
		// 		Wooded Mountains -> Stone Shore, Savanna -> Beach, Savanna Plateau -> Beach,
		// 		Badlands -> Desert, Wooded Badlands Plateau -> Desert, Badlands Plateau -> Beach,
		// 		Sunflower Plains -> Beach, Desert Lakes -> Beach, Gravelly Mountains -> Beach,
		// 		Flower Forest -> Beach, Taiga Mountains -> Beach, Swamp Hills -> Beach,
		// 		Ice Spikes -> Snowy Beach, Modified Jungle -> Beach, Modified Jungle -> Jungle Edge,
		// 		Modified Jungle Edge -> Beach, Modified Jungle Edge -> Jungle Edge,
		// 		Tall Birch Forest -> Beach, Tall Birch Hills -> Beach, Dark Forest Hills -> Beach,
		// 		Snowy Taiga Mountains -> Snowy Beach, Giant Spruce Taiga -> Beach,
		// 		Giant Spruce Taiga Hills -> Beach, Modified Gravelly Mountains -> Beach,
		// 		Shattered Savanna -> Beach, Shattered Savanna Plateau -> Beach,
		// 		Eroded Badlands -> Beach, Modified Wooded Badlands Plateau -> Beach,
		// 		Modified Badlands Plateau -> Beach
		// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore,
		// 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
		// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
		// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
		// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
		// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
		// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
		// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
		// 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
		// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
		// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
		// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows Plains -> Beach, Desert -> Beach, Mountains -> Stone Shore, Forest -> Beach,
		// 		Taiga -> Beach, Snowy Tundra -> Snowy Beach, Snowy Mountains -> Snowy Beach,
		// 		Mushroom Fields -> Mushroom Field Shore, Desert Hills -> Beach, Wooded Hills -> Beach,
		// 		Taiga Hills -> Beach, Jungle -> Beach, Jungle -> Jungle Edge, Jungle Hills -> Beach,
		// 		Jungle Hills -> Jungle Edge, Jungle Edge -> Beach, Birch Forest -> Beach,
		// 		Birch Forest Hills -> Beach, Dark Forest -> Beach, Snowy Taiga -> Snowy Beach,
		// 		Snowy Taiga Hills -> Snowy Beach, Giant Tree Taiga -> Beach, Giant Tree Taiga Hills -> Beach,
		// 		Wooded Mountains -> Stone Shore, Savanna -> Beach, Savanna Plateau -> Beach,
		// 		Badlands -> Desert, Wooded Badlands Plateau -> Desert, Badlands Plateau -> Beach,
		// 		Sunflower Plains -> Beach, Desert Lakes -> Beach, Gravelly Mountains -> Beach,
		// 		Flower Forest -> Beach, Taiga Mountains -> Beach, Swamp Hills -> Beach,
		// 		Ice Spikes -> Snowy Beach, Modified Jungle -> Beach, Modified Jungle -> Jungle Edge,
		// 		Modified Jungle Edge -> Beach, Modified Jungle Edge -> Jungle Edge,
		// 		Tall Birch Hills -> Beach, Dark Forest Hills -> Beach, Snowy Taiga Mountains -> Snowy Beach,
		// 		Giant Spruce Taiga -> Beach, Giant Spruce Taiga Hills -> Beach,
		// 		Modified Gravelly Mountains -> Beach, Shattered Savanna -> Beach,
		// 		Shattered Savanna Plateau -> Beach, Eroded Badlands -> Beach,
		// 		Modified Wooded Badlands Plateau -> Beach, Modified Badlands Plateau -> Beach
		// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore,
		// 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
		// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
		// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
		// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
		// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
		// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
		// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
		// 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills,
		// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
		// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
		// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
		// 167 = Modified Badlands Plateau, 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
		// 		Allows Plains -> Beach, Desert -> Beach, Mountains -> Stone Shore, Forest -> Beach,
		// 		Taiga -> Beach, Snowy Tundra -> Snowy Beach, Snowy Mountains -> Snowy Beach,
		// 		Mushroom Fields -> Mushroom Field Shore, Desert Hills -> Beach, Wooded Hills -> Beach,
		// 		Taiga Hills -> Beach, Jungle -> Beach, Jungle -> Jungle Edge, Jungle Hills -> Beach,
		// 		Jungle Hills -> Jungle Edge, Jungle Edge -> Beach, Birch Forest -> Beach,
		// 		Birch Forest Hills -> Beach, Dark Forest -> Beach, Snowy Taiga -> Snowy Beach,
		// 		Snowy Taiga Hills -> Snowy Beach, Giant Tree Taiga -> Beach, Giant Tree Taiga Hills -> Beach,
		// 		Wooded Mountains -> Stone Shore, Savanna -> Beach, Savanna Plateau -> Beach,
		// 		Badlands -> Desert, Wooded Badlands Plateau -> Desert, Badlands Plateau -> Beach,
		// 		Sunflower Plains -> Beach, Desert Lakes -> Beach, Gravelly Mountains -> Beach,
		// 		Flower Forest -> Beach, Taiga Mountains -> Beach, Swamp Hills -> Beach,
		// 		Ice Spikes -> Snowy Beach, Modified Jungle -> Beach, Modified Jungle -> Jungle Edge,
		// 		Modified Jungle Edge -> Beach, Modified Jungle Edge -> Jungle Edge,
		// 		Tall Birch Forest -> Beach, Tall Birch Hills -> Beach, Dark Forest Hills -> Beach,
		// 		Snowy Taiga Mountains -> Snowy Beach, Giant Spruce Taiga -> Beach,
		// 		Giant Spruce Taiga Hills -> Beach, Modified Gravelly Mountains -> Beach,
		// 		Shattered Savanna -> Beach, Shattered Savanna Plateau -> Beach,
		// 		Eroded Badlands -> Beach, Modified Wooded Badlands Plateau -> Beach,
		// 		Modified Badlands Plateau -> Beach, Bamboo Jungle -> Beach, Bamboo Jungle -> Jungle Edge,
		// 		Bamboo Jungle Hills -> Beach, Bamboo Jungle Hills -> Jungle Edge
		if (biomesRequiredMargin) *biomesRequiredMargin += 1;
		shoreLayer(biomes, tempBuffer, configuration);
		if (configuration->startingLayerID == L_SHORE_16) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	if (configuration->version >= MC_1_1 && configuration->version <= MC_1_6) {
		// 1:16, Large Biomes 1:64
		// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 7 = River, 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge
		// 		Allows Swamp -> River
		// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 7 = River, 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
		// 		Allows Swamp -> River, Jungle -> River, Jungle Hills -> River
		// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 7 = River, 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
		// 		Allows Swamp -> River, Jungle -> River, Jungle Hills -> River
		addSwampRiverLayer(biomes, 1000, configuration);
		if (configuration->startingLayerID == L_SWAMP_RIVER_16) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:8, Large Biomes 1:32
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 14 = Mushroom Fields, 15 = Mushroom Field Shore
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp, 7 = River,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
	// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
	// 20 = Mountain Edge
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp, 7 = River,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
	// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
	// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp, 7 = River,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
	// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
	// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore, 16 = Beach,
	// 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
	// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
	// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
	// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
	// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
	// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
	// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
	// 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore, 16 = Beach,
	// 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
	// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
	// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
	// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
	// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
	// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
	// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
	// 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore, 16 = Beach,
	// 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
	// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
	// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
	// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
	// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
	// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
	// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
	// 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau,
	// 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1002, configuration);
	if (configuration->startingLayerID == L_ZOOM_8) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:4, Large Biomes 1:16
	// Beta 1.8: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.0: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 14 = Mushroom Fields, 15 = Mushroom Field Shore
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.1: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp, 7 = River,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
	// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
	// 20 = Mountain Edge
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.2: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp, 7 = River,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
	// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
	// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp, 7 = River,
	// 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
	// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
	// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore, 16 = Beach,
	// 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
	// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
	// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
	// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
	// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
	// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
	// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
	// 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore, 16 = Beach,
	// 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
	// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
	// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
	// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
	// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
	// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
	// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
	// 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
	// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore, 16 = Beach,
	// 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
	// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
	// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
	// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
	// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
	// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
	// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
	// 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
	// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
	// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
	// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau,
	// 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
	// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
	if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
	zoomLayer(biomes, tempBuffer, false, 1003, configuration);
	if (configuration->startingLayerID == L_ZOOM_4) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	if (configuration->version >= MC_1_3 && configuration->largeBiomes) {
		// 1:8
		// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 7 = River, 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
		// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore,
		// 26 = Snowy Beach, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains,
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest,
		// 156 = Tall Birch Hills, 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains,
		// 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains,
		// 163 = Shattered Savanna, 164 = Shattered Savanna Plateau, 165 = Eroded Badlands,
		// 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore,
		// 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
		// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
		// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
		// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
		// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
		// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
		// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
		// 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
		// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
		// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
		// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore,
		// 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
		// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
		// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
		// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
		// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
		// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
		// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
		// 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills,
		// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
		// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
		// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
		// 167 = Modified Badlands Plateau, 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
		zoomLayer(biomes, tempBuffer, false, 1004, configuration);
		if (configuration->startingLayerID == L_ZOOM_LARGE_A) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:4
		// 1.3-1.6: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 7 = River, 10 = Frozen Ocean, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 20 = Mountain Edge, 21 = Jungle, 22 = Jungle Hills
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.7-1.8, 1.11-1.13: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga,
		// 6 = Swamp, 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields,
		// 15 = Mushroom Field Shore, 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills,
		// 21 = Jungle, 22 = Jungle Hills, 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore,
		// 26 = Snowy Beach, 27 = Birch Forest, 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga,
		// 31 = Snowy Taiga Hills, 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains,
		// 35 = Savanna, 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau,
		// 39 = Badlands Plateau, 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains,
		// 132 = Flower Forest, 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes,
		// 149 = Modified Jungle, 151 = Modified Jungle Edge, 155 = Tall Birch Forest,
		// 156 = Tall Birch Hills, 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains,
		// 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains,
		// 163 = Shattered Savanna, 164 = Shattered Savanna Plateau, 165 = Eroded Badlands,
		// 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.9-1.10: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore,
		// 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
		// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
		// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
		// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
		// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
		// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
		// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
		// 151 = Modified Jungle Edge, 156 = Tall Birch Hills, 157 = Dark Forest Hills,
		// 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga, 161 = Giant Spruce Taiga Hills,
		// 162 = Modified Gravelly Mountains, 163 = Shattered Savanna, 164 = Shattered Savanna Plateau,
		// 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau, 167 = Modified Badlands Plateau
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		// 1.14+: 0 = Ocean, 1 = Plains, 2 = Desert, 3 = Mountains, 4 = Forest, 5 = Taiga, 6 = Swamp,
		// 12 = Snowy Tundra, 13 = Snowy Mountains, 14 = Mushroom Fields, 15 = Mushroom Field Shore,
		// 16 = Beach, 17 = Desert Hills, 18 = Wooded Hills, 19 = Taiga Hills, 21 = Jungle, 22 = Jungle Hills,
		// 23 = Jungle Edge, 24 = Deep Ocean, 25 = Stone Shore, 26 = Snowy Beach, 27 = Birch Forest,
		// 28 = Birch Forest Hills, 29 = Dark Forest, 30 = Snowy Taiga, 31 = Snowy Taiga Hills,
		// 32 = Giant Tree Taiga, 33 = Giant Tree Taiga Hills, 34 = Wooded Mountains, 35 = Savanna,
		// 36 = Savanna Plateau, 37 = Badlands, 38 = Wooded Badlands Plateau, 39 = Badlands Plateau,
		// 129 = Sunflower Plains, 130 = Desert Lakes, 131 = Gravelly Mountains, 132 = Flower Forest,
		// 133 = Taiga Mountains, 134 = Swamp Hills, 140 = Ice Spikes, 149 = Modified Jungle,
		// 151 = Modified Jungle Edge, 155 = Tall Birch Forest, 156 = Tall Birch Hills,
		// 157 = Dark Forest Hills, 158 = Snowy Taiga Mountains, 160 = Giant Spruce Taiga,
		// 161 = Giant Spruce Taiga Hills, 162 = Modified Gravelly Mountains, 163 = Shattered Savanna,
		// 164 = Shattered Savanna Plateau, 165 = Eroded Badlands, 166 = Modified Wooded Badlands Plateau,
		// 167 = Modified Badlands Plateau, 168 = Bamboo Jungle, 169 = Bamboo Jungle Hills
		// 		Allows anything to change to anything else (on coordinates odd on 1+ axes)
		if (biomesRequiredMargin) *biomesRequiredMargin = ceil(*biomesRequiredMargin/2.);
		zoomLayer(biomes, tempBuffer, false, 1005, configuration);
		if (configuration->startingLayerID == L_ZOOM_LARGE_B) {
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:4
	if (biomesRequiredMargin) *biomesRequiredMargin += 1;
	smoothLayer(biomes, tempBuffer, 1000, configuration);
	if (configuration->startingLayerID == L_SMOOTH_4) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:32, 1.7- Large Biomes 1:128
	riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
	zoomLayer(riverNoise, tempBuffer, false, configuration->version <= MC_1_6 ? 1002 : 1000, configuration);
	if (configuration->startingLayerID == L_ZOOM_32_RIVER) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:16, 1.7- Large Biomes 1:64
	riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
	zoomLayer(riverNoise, tempBuffer, false, configuration->version <= MC_1_6 ? 1003 : 1001, configuration);
	if (configuration->startingLayerID == L_ZOOM_16_RIVER) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:8, 1.7- Large Biomes 1:32
	riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
	zoomLayer(riverNoise, tempBuffer, false, configuration->version <= MC_1_6 ? 1004 : 1002, configuration);
	if (configuration->startingLayerID == L_ZOOM_8_RIVER) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:4, 1.7- Large Biomes 1:16
	riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
	zoomLayer(riverNoise, tempBuffer, false, configuration->version <= MC_1_6 ? 1005 : 1003, configuration);
	if (configuration->startingLayerID == L_ZOOM_4_RIVER) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	if (configuration->version <= MC_1_7 && configuration->largeBiomes) {
		// 1:8
		riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
		zoomLayer(riverNoise, tempBuffer, false, configuration->version <= MC_1_6 ? 1006 : 1004, configuration);
		if (configuration->startingLayerID == L_ZOOM_L_RIVER_A) {
			memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
			*biomesRequiredMargin = riverNoiseRequiredMargin;
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:4
		riverNoiseRequiredMargin = ceil(riverNoiseRequiredMargin/2.);
		zoomLayer(riverNoise, tempBuffer, false, configuration->version <= MC_1_6 ? 1007 : 1005, configuration);
		if (configuration->startingLayerID == L_ZOOM_L_RIVER_B) {
			memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
			*biomesRequiredMargin = riverNoiseRequiredMargin;
			free(riverNoise);
			free(tempBuffer);
			return 0;
		}
	}

	// 1:4
	riverNoiseRequiredMargin += 1;
	riverLayer(riverNoise, tempBuffer, configuration);
	if (configuration->startingLayerID == L_RIVER_4) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:4
	riverNoiseRequiredMargin += 1;
	smoothLayer(riverNoise, tempBuffer, 1000, configuration);
	if (configuration->startingLayerID == L_SMOOTH_4_RIVER) {
		memcpy(biomes, riverNoise, configuration->width*configuration->height*sizeof(*riverNoise));
		*biomesRequiredMargin = riverNoiseRequiredMargin;
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// 1:4
	if (biomesRequiredMargin && *biomesRequiredMargin < riverNoiseRequiredMargin) *biomesRequiredMargin = riverNoiseRequiredMargin;
	riverMixerLayer(biomes, riverNoise, configuration);
	if (configuration->startingLayerID == L_RIVER_MIX_4) {
		free(riverNoise);
		free(tempBuffer);
		return 0;
	}

	// River noise is no longer used
	free(riverNoise);

	// =============================
	// OCEAN VARIANTS
	// =============================

	if (configuration->version >= MC_1_13) {
		// Create ocean noisemap
		int *const oceanNoise = (int *const)calloc(configuration->width * configuration->height, sizeof(*oceanNoise));
		int oceanNoiseRequiredMargin = 0;

		// 1:256
		oceanLayer(oceanNoise, configuration);
		if (configuration->startingLayerID == L_OCEAN_TEMP_256) {
			memcpy(biomes, oceanNoise, configuration->width*configuration->height*sizeof(*oceanNoise));
			*biomesRequiredMargin = oceanNoiseRequiredMargin;
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:128
		oceanNoiseRequiredMargin = ceil(oceanNoiseRequiredMargin/2.);
		zoomLayer(oceanNoise, tempBuffer, false, 2001, configuration);
		if (configuration->startingLayerID == L_ZOOM_128_OCEAN) {
			memcpy(biomes, oceanNoise, configuration->width*configuration->height*sizeof(*oceanNoise));
			*biomesRequiredMargin = oceanNoiseRequiredMargin;
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:64
		oceanNoiseRequiredMargin = ceil(oceanNoiseRequiredMargin/2.);
		zoomLayer(oceanNoise, tempBuffer, false, 2002, configuration);
		if (configuration->startingLayerID == L_ZOOM_64_OCEAN) {
			memcpy(biomes, oceanNoise, configuration->width*configuration->height*sizeof(*oceanNoise));
			*biomesRequiredMargin = oceanNoiseRequiredMargin;
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:32
		oceanNoiseRequiredMargin = ceil(oceanNoiseRequiredMargin/2.);
		zoomLayer(oceanNoise, tempBuffer, false, 2003, configuration);
		if (configuration->startingLayerID == L_ZOOM_32_OCEAN) {
			memcpy(biomes, oceanNoise, configuration->width*configuration->height*sizeof(*oceanNoise));
			*biomesRequiredMargin = oceanNoiseRequiredMargin;
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:16
		oceanNoiseRequiredMargin = ceil(oceanNoiseRequiredMargin/2.);
		zoomLayer(oceanNoise, tempBuffer, false, 2004, configuration);
		if (configuration->startingLayerID == L_ZOOM_16_OCEAN) {
			memcpy(biomes, oceanNoise, configuration->width*configuration->height*sizeof(*oceanNoise));
			*biomesRequiredMargin = oceanNoiseRequiredMargin;
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:8
		oceanNoiseRequiredMargin = ceil(oceanNoiseRequiredMargin/2.);
		zoomLayer(oceanNoise, tempBuffer, false, 2005, configuration);
		if (configuration->startingLayerID == L_ZOOM_8_OCEAN) {
			memcpy(biomes, oceanNoise, configuration->width*configuration->height*sizeof(*oceanNoise));
			*biomesRequiredMargin = oceanNoiseRequiredMargin;
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:4
		oceanNoiseRequiredMargin = ceil(oceanNoiseRequiredMargin/2.);
		zoomLayer(oceanNoise, tempBuffer, false, 2006, configuration);
		if (configuration->startingLayerID == L_ZOOM_4_OCEAN) {
			memcpy(biomes, oceanNoise, configuration->width*configuration->height*sizeof(*oceanNoise));
			*biomesRequiredMargin = oceanNoiseRequiredMargin;
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// 1:4
		if (biomesRequiredMargin) {
			if (*biomesRequiredMargin < oceanNoiseRequiredMargin) *biomesRequiredMargin = oceanNoiseRequiredMargin;
			*biomesRequiredMargin += 8;
		}
		oceanMixerLayer(biomes, oceanNoise, tempBuffer, configuration);
		if (configuration->startingLayerID == L_OCEAN_MIX_4) {
			free(oceanNoise);
			free(tempBuffer);
			return 0;
		}

		// Ocean noise is no longer used
		free(oceanNoise);
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
		0,
		false,
		// -100, -100, 100, 100, 201, 201,
		// -7, -7, 7, 7, 15, 15,
		-240, -240, 239, 239, 15*32, 15*32,
		0
	};

	bool anyWorldseedInvalidated = false;
	// For each worldseed to check:
	for (size_t i = 0; i < sizeof(WORLDSEEDS_TO_CHECK)/sizeof(*WORLDSEEDS_TO_CHECK); ++i) {
		configuration.worldseed = WORLDSEEDS_TO_CHECK[i];
	// for (configuration.worldseed = 0; configuration.worldseed <= 200; ++configuration.worldseed) {
		printf("Checking worldseed %" PRId64 "...\n", configuration.worldseed);
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
						printf("\tError: emulateBiomes() under %s%s, layer %s returned nonzero error code %d.\n", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID), errorCode);
						free(emulatedBiomes);
						free(trueBiomes);
						continue;
					}
					// Fill true array
					errorCode = getTrueBiomes(&configuration, trueBiomes, cacheSize);
					if (errorCode) {
						printf("\tError: getTrueBiomes() under %s%s, layer %s returned nonzero error code %d.\n", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID), errorCode);
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
					printf("\t%s%s, layer %s: Emulated biomes DO NOT match true biomes.\n", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
					char filepath[500];
					// Save emulated biomes as image
					snprintf(filepath, sizeof(filepath)/sizeof(*filepath), "./Emulation Maps/%s%s %s Emulated Biomes.ppm", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
					errorCode = saveAsImage(&configuration, emulatedBiomes, filepath);
					if (errorCode) printf("\t\tError: saveAsImage(emulatedBiomes) returned non-zero error code %d.\n", errorCode);
					// Save true biomes as image
					snprintf(filepath, sizeof(filepath)/sizeof(*filepath), "./Emulation Maps/%s%s %s True Biomes.ppm", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
					errorCode = saveAsImage(&configuration, trueBiomes, filepath);
					if (errorCode) printf("\t\tError: saveAsImage(trueBiomes) returned non-zero error code %d.\n", errorCode);
					// Save difference as image
					for (size_t i = 0; i < cacheSize; ++i) emulatedBiomes[i] = (emulatedBiomes[i] == trueBiomes[i]);
					snprintf(filepath, sizeof(filepath)/sizeof(*filepath), "./Emulation Maps/%s%s %s Difference.ppm", mc2str(version), largeBiomes ? " Large Biomes" : "", layer2str(startingLayerID));
					errorCode = saveAsImage(&configuration, emulatedBiomes, filepath);
					if (errorCode) printf("\t\tError: saveAsImage(Difference) returned non-zero error code %d.\n", errorCode);

					free(emulatedBiomes);
					free(trueBiomes);
					anyWorldseedInvalidated = true;
					continue;
				}
			}
		}
		// Stop early if a worldseed is invalid; chances are high all others will be invalid as well
		if (anyWorldseedInvalidated) break;
	}
	if (!anyWorldseedInvalidated) printf("All checks completed.\n");
	return 0;
}