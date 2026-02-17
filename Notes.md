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
- Layers with identical layer salts have identical start salts, start seeds, and subsequent PRNG streams.

## Versions where Biomes Changed:
- Beta 1.8 - Beta 1.8.1
- 1.0 - 1.0.1
- 1.1
- 1.2.1 - 1.2.5
- 1.3.1 - 1.6.4
- 1.7.2 - 1.12.2
- 1.7.2 - 1.7.10 Large Biomes
- 1.13 - 1.13.2
- 1.14 - 1.14.4
- 1.15 - 1.17.1

## Duplicate 1.16.1 Layer Salts (Unsigned):
- 229918546094678885   (2001):
  - Zoom 1024: Next(2) at (even, odd) or (odd, even); Next(4) at (odd, odd) if tie occurs
  - Zoom 128 Ocean: ?
- 558114146894579477   (2005):
  - Zoom 8 Ocean: ?
- 837738509879401688   (2002):
  - Zoom 512: Next(2) at (even, odd) or (odd, even); Next(4) at (odd, odd) if tie occurs
  - Zoom 64 Ocean: ?
- 1827289100522298840  (1002):
  - Zoom 8: ?
  - Zoom 8 River: ?
- 3006835321906069877  (2003):
  - Zoom 256: Next(2) at (even, odd) or (odd, even); Next(4) at (odd, odd) if tie occurs
  - Zoom 32 Ocean: ?
- 3038466749335869312  ( 200):
  - Biome 256: Next(3) if 1.7+ Warm Special; Next(6) if 1.1- or 1.7+ Warm Normal/Next(7) if 1.2-1.6 Warm Normal; Next(6) if 1.7+ Lush Normal; Next(4) if 1.7+ Cold Normal; Next(7) if 1.3-1.6 frozen ocean/snowy tundra; Next(4) if 1.7+ Freezing 
- 3107951898966440229  (   1):
  - Continent 4096: Next(10)
  - Land 2048: Next(3) if Beta 1.8 ocean diagonal to land; Next(1-4) Next(3) for 1.0+ ocean diagonal to land; Next(5) for land diagonal to ocean
  - River 4: ?
- 5360640171528462240  (   4):
  - Land 256: Next(3) if Beta 1.8 ocean diagonal to land; Next(1-4) Next(3) for 1.0+ ocean diagonal to land; Next(5) for land diagonal to ocean
- 5692911206796425088  (1000):
  - Zoom 128: ?
  - Zoom 128 Hills: ?
  - Zoom 128 River: ?
  - Biome Edge 64: ?
  - Hills 64: ?
  - Zoom 32: ?
  - Zoom 32 River: ?
  - Shore 16: ?
  - Smooth 4: ?
  - Smooth 4 River: ?
- 5723240131506253216  ( 100):
  - Noise 256: Next(2) for 1.6-, Next(299999) (!) for 1.7+
  - River Mix 4: ?
  - Ocean Mix 4: ?
- 5852781679691581125  (1001):
  - Bamboo 256: Next(10) for 1.14+ jungles
  - Zoom 64: ?
  - Zoom 64 Hills: ?
  - Zoom 64 River: ?
  - Sunflower 64: ?
  - Zoom 16: ?
  - Zoom 16 River: ?
- 7231908362866731896  (  70):
  - Land 1024 C: Next(1-4) Next(3) for ocean diagonal to land; Next(5) for land diagonal to ocean
- 7590731853067264053  (   3):
  - Land 1024 D: Next(1-4) Next(3) for ocean diagonal to land; Next(5) for land diagonal to ocean
  - Special 1024: Next(13) Next(15) for 1.7+ non-oceans (result of latter possibly not used)
  - Land 32: ?
- 9672642253349399552  (2000):
  - Zoom 2048: Next(2) at (even, odd) or (odd, even); Next(4) at (odd, odd) if tie occurs
- 10938390704639815544 (2006):
  - Zoom 4 Ocean: ?
- 10967462438749070293 (   5):
  - Mushroom 256: Next(100) at 1.0+ oceans diagonal to exclusively oceans
- 12314713599595444213 (1005):
  - Zoom Large Biomes B: ?
- 13432066074785117656 (   2):
  - Land 1024 A: Next(3) if Beta 1.8 ocean diagonal to land; Next(1-4) Next(3) for 1.0+ ocean diagonal to land; Next(5) for land diagonal to ocean
  - Island 1024: Next(2) at oceans surrounded orthagonally by oceans
  - Snow 1024: Next(5) for 1.0-1.6 non-ocean, Next(6) for 1.7+ non-ocean
  - Ocean Temperature 256: ?
- 14406777830260091477 (1003):
  - Zoom 4: ?
  - Zoom 4 River: ?
- 16630052651815956128 (1004):
  - Zoom Large Biomes A: ?
- 16973349028156721880 (  50):
  - Land 1024 B: Next(3) for ocean diagonal to land; Next(5) for land diagonal to ocean
- 17944835642018066080 (2004):
  - Zoom 16 Ocean: ?
- None (uses worldseed SHA)  :
  - Voronoi 1: ?

## Next Steps:
- Recreate Cubiomes' biome emulation without function chaining, to properly understand what each layer does + how they build off of their predecessors
- What PRNG calls do layers with the same layer salt make? Do e.g. the salt 1000 ones give enough information to identify/significantly narrow down the possible original start salt?
- Determine why rivers only depend on lowest 26 bits