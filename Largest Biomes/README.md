# Largest Biomes

These are programs to find various *Minecraft: Java Edition* 1.0 - 1.17.1 biomes spanning the most number of contiguous tiles. In general, such tile counts are related to, but unfortunately **not** an exact indicator of, the biome's final block-level area.

After generating a static archive of Cubiomes with `cmake` and a Makefile generator of your choice, you can generally compile these programs with
```bash
g++ <PROGRAM>.cpp ../reversal.c ../cubiomes/libcubiomes_static.a ../core/Backends/<YOUR BACKEND OF CHOICE> -o <YOUR DESIRED EXECUTABLE NAME>.exe -O3 -fwrapv -ffast-math -Wall -Wextra -pedantic
```

Thank you to
- [Kris](https://github.com/kludwisz/) for helping me optimize `mostMushroomTiles.cpp` and catching some bugs in it; and
- [Andrew](https://github.com/Gaider10/) for speeding up the quadratic PRNG reversal algorithm.