# Large Mushroom Islands

This is a program to find *Minecraft: Java Edition* 1.0 - 1.17.1 mushroom islands with the most contiguous tiles. Such tile counts are related to, but unfortunately **not** an exact indicator of, the island's final block-level area.

After generating a static archive of Cubiomes with `cmake` and a Makefile generator of your choice, you can compile this program with
```bash
g++ mostMushroomTiles.cpp ../reversal.c ../cubiomes/libcubiomes_static.a ../core/Backends/<YOUR BACKEND OF CHOICE> -o mostMushroomTiles.exe -O3 -fwrapv -Wall -Wextra -pedantic
```

Thank you to [Kris](https://github.com/kludwisz/) for helping me optimize this program, and for catching some bugs.