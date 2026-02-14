#ifndef LAYERUTIL_H
#define LAYERUTIL_H

#include "cubiomes/layers.h"

// Maps a layer enum to its corresponding name string.
static const char *layer2str(ptrdiff_t layer) {
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

#endif