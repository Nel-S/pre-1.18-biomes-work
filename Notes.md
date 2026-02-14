# Notes

Biomes are mostly selected using a 64-bit quadratic update function: `New = Next(State, Salt) = A*State^2 + B*State + Salt`.
- `New` will always have the same parity as `Salt`.
- Given `State` and `Salt`, one possibility for `New` exists; given `State` and `New`, one possibility for `Salt` exists.
- Given `New` and `Salt`, two possibilities for State exist, one odd and one even.
- The possibilities can be converted to one another via `possibility2 = -7379792620528906219LL - possibility1`.
  - Across multiple advancements, whichever State possibility does not have parity matching Salt is impossible and is discarded; only during the very first advancement are both possibilites preserved. (This is where shadow seeds come from and why there are exactly two.)
Ocean Temperature 256 additionally uses a single Perlin noisemap initialized with the structure seed.

Each Layer has a layer salt that is version + generation mode-dependent.
The worldseed is then used alongside the layer salt to generate the layer's start salt.
The layer's start seed is `Next(start salt, 0)`. This will always be even, but only a multiple of `2^(n + 1)` if the start salt is a multiple of `2^n`.
- Layers with identical layer salts should have identical start salts, start seeds, and subsequent PRNG streams.

## Versions where Biomes Changed:
Beta 1.8, 1.0, 1.1-1.6.4, 1.7-1.12.2, 1.7-1.7.9 Large Biomes, 1.13-1.13.2, 1.14-1.14.4, 1.15-1.17.1

## Duplicate 1.16.1 Layer Salts (Unsigned):
- 229918546094678885   (2001): Zoom 1024, Zoom 128 Ocean
- 558114146894579477   (2005): Zoom 8 Ocean
- 837738509879401688   (2002): Zoom 512, Zoom 64 Ocean
- 1827289100522298840  (1002): Zoom 8, Zoom 8 River
- 3006835321906069877  (2003): Zoom 256, Zoom 32 Ocean
- 3038466749335869312  ( 200): Biome 256
- 3107951898966440229  (   1): Continent 4096, Land 2048, River 4
- 5360640171528462240  (   4): Land 256, Deep Ocean 256
- 5692911206796425088  (1000): Zoom 128, Biome Edge 64, Zoom 128 Hills, Hills 64, Zoom 32, Shore 16, Smooth 4, Zoom 128 River, Zoom 32 River, Smooth 4 River
- 5723240131506253216  ( 100): Noise 256, River Mix 4, Ocean Mix 4
- 5852781679691581125  (1001): Bamboo 256, Zoom 64, Zoom 64 Hills, Sunflower 64, Zoom 16, Zoom 64 River, Zoom 16 River
- 7231908362866731896  (  70): Land 1024 C
- 7590731853067264053  (   3): Land 1024 D, Special 1024, Land 32
- 9672642253349399552  (2000): Zoom 2048
- 10938390704639815544 (2006): Zoom 4 Ocean
- 10967462438749070293 (   5): Mushroom 256
- 12314713599595444213 (1005): Zoom Large Biomes B
- 13432066074785117656 (   2): Land 1024 A, Island 1024, Snow 1024, Cool 1024, Ocean Temperature 256
- 14406777830260091477 (1003): Zoom 4, Zoom 4 River
- 16630052651815956128 (1004): Zoom Large Biomes A
- 16973349028156721880 (  50): Land 1024 B
- 17944835642018066080 (2004): Zoom 16 Ocean
- None (uses worldseed SHA)  : Voronoi 1

## Next Steps:
- Recreate Cubiomes' biome emulation without function chaining, to properly understand what each layer does + how they build off of their predecessors
- What PRNG calls do layers with the same layer salt make? Do e.g. the salt 1000 ones give enough information to identify/significantly narrow down the possible original start salt?
- Determine why rivers only depend on lowest 26 bits