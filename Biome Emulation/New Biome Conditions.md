# New Biome Conditions

These are tables of all layers and conditions under which a biome can be newly inserted into a world's biome map, *without the biome having already been present before*. (This therefore excludes cases like the Zoom layers, which for odd coordinates at a given scale copies one of the neighboring biomes, etc.)

<u>Underlined</u> entries are guaranteed to never persist to the final 1:1 biome map.

## Beta 1.8
<table>
    <thead>
        <tr> <th>Biome</th>           <th>Scale</th>             <th>Layer Name</th>          <th>Conditions</th> </tr>
    </thead>
    <tbody>
        <!-- Desert -->
        <tr> <td>Desert</td>          <td>1:256</td>             <td>BiomeInit</td>           <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Forest -->
        <tr> <td>Forest</td>          <td>1:256</td>             <td>BiomeInit</td>           <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Mountains -->
        <tr> <td>Mountains</td>       <td>1:256</td>             <td>BiomeInit</td>           <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Ocean -->
        <tr> <td rowspan=7>Ocean</td> <td>1:8192</td>            <td>Island</td>              <td>Initialized at tiles other than (0, 0), with 9/10th chance</td> </tr>
        <tr>                          <td>1:4096</td>            <td rowspan=6>AddIsland</td> <td rowspan=3>Replaces Plains diagonally bordered by Ocean, with 1/5th chance</td> </tr>
        <tr>                          <td>1:2048</td> </tr>
        <tr>                          <td>1:1024</td> </tr>
        <tr>                          <td>1:512</td>                                          <td rowspan=3>Replaces Plains diagonally bordered by any non-Plains biome, with 1/5th chance</td> </tr>
        <tr>                          <td>1:256</td> </tr>
        <tr>                          <td>1:32</td> </tr>
        <!-- Plains -->
        <tr> <td rowspan=8>Plains</td> <td rowspan=2>1:8192</td> <td rowspan=2>Island</td>    <td>Hardcoded biome at (0,0)</td> </tr>
        <tr>                                                                                  <td>Initialized at tiles other than (0,0), with 1/10th chance</td> </tr>
        <tr>                          <td>1:4096</td>            <td rowspan=6>AddIsland</td> <td rowspan=3>Replaces Ocean diagonally bordered by Plains, with 1/3rd chance</td> </tr>
        <tr>                          <td>1:2048</td> </tr>
        <tr>                          <td>1:1024</td> </tr>
        <tr>                          <td>1:512</td>                                          <td rowspan=3>Replaces Ocean diagonally bordered by any non-Ocean biome, with 1/3rd chance</td> </tr>
        <tr>                          <td>1:256</td> </tr>
        <tr>                          <td>1:32</td> </tr>
        <!-- River -->
        <tr> <td>River</td>           <td>1:4</td>               <td>RiverMixer</td>          <td>Replaces non-Oceans, dependent on borders/Ocean-bleedthrough in zoomed river noisemap</td> </tr>
        <!-- Swamp -->
        <tr> <td>Swamp</td>           <td>1:256</td>             <td>BiomeInit</td>           <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Taiga -->
        <tr> <td>Taiga</td>           <td>1:256</td>             <td>BiomeInit</td>           <td>Replaces Plains, with 1/6th chance</td> </tr>
    </tbody>
</table>

## 1.0
<table>
    <thead>
        <tr> <th>Biome</th>                            <th>Scale</th>                  <th>Layer Name</th>                 <th>Conditions</th> </tr>
    </thead>
    <tbody>
        <!-- Desert -->
        <tr> <td>Desert</td>                           <td>1:256</td>                  <td>BiomeInit</td>                  <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Forest -->
        <tr> <td>Forest</td>                           <td>1:256</td>                  <td>BiomeInit</td>                  <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Frozen Ocean -->
        <tr> <td rowspan=6>Frozen Ocean</td>           <td rowspan=2><u>1:512</u></td> <td rowspan=4><u>AddIsland</u></td> <td><u>Replaces Ocean diagonally bordered by Snowy Tundra, with 2*[number of diagonal Snowy Tundra neighbors]/(3*[number of diagonal land neighbors])th chance</u></td> </tr>
        <tr>                                                                                                               <td><u>Replaces Snowy Tundra diagonally bordered by Ocean, with 1/5th chance</u></td> </tr>
        <tr>                                           <td rowspan=2><u>1:256</u></td>                                     <td><u>Replaces Ocean diagonally bordered by Snowy Tundra, with 2*[number of diagonal Snowy Tundra neighbors]/(3*[number of diagonal land neighbors])th chance</u></td> </tr>
        <tr>                                                                                                               <td><u>Replaces Snowy Tundra diagonally bordered by Ocean, with 1/5th chance</u></td> </tr>
        <tr>                                           <td rowspan=2>1:32</td>         <td rowspan=2>AddIsland</td>        <td>Replaces Ocean diagonally bordered by Snowy Tundra, with 2*[number of diagonal Snowy Tundra neighbors]/(3*[number of diagonal land neighbors])th chance</td> </tr>
        <tr>                                                                                                               <td>Replaces Snowy Tundra diagonally bordered by Ocean, with 1/5th chance</td> </tr>
        <!-- Frozen River -->
        <tr> <td>Frozen River</td>                     <td>1:4</td>                    <td>RiverMixer</td>                 <td>Replaces Snowy Tundra, dependent on borders/Ocean-bleedthrough in zoomed river noisemap</td> </tr>
        <!-- Mountains -->
        <tr> <td>Mountains</td>                        <td>1:256</td>                  <td>BiomeInit</td>                  <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Mushroom Fields -->
        <tr> <td>Mushroom Fields</td>                  <td>1:256</td>                  <td>AddMushroomIsland</td>          <td>Replaces Ocean diagonally surrounded by Ocean, with 1/100th chance</td> </tr>
        <!-- Mushroom Fields Shore -->
        <tr> <td rowspan=2>Mushroom Fields Shore</td>  <td>1:32</td>                   <td>Shore</td>                      <td>Replaces Mushroom Fields orthagonally bordered by Ocean</td> </tr>
        <tr>                                           <td>1:4</td>                    <td>RiverMixer</td>                 <td>Replaces Mushroom Fields, dependent on borders/Ocean-bleedthrough in zoomed river noisemap</td> </tr>
        <!-- Ocean -->
        <tr> <td rowspan=6>Ocean</td>                  <td>1:4096</td>                 <td>Island</td>                     <td>Initialized at tiles other than (0, 0), with 9/10th chance</td> </tr>
        <tr>                                           <td>1:2048</td>                 <td rowspan=5>AddIsland</td>        <td rowspan=2>Replaces Plains diagonally bordered by Ocean, with 1/5th chance</td> </tr>
        <tr>                                           <td>1:1024</td> </tr>
        <tr>                                           <td>1:512</td>                                                      <td rowspan=3>Replaces non-Snowy Tundras diagonally bordered by Ocean, with 1/5th chance</td> </tr>
        <tr>                                           <td>1:256</td> </tr>
        <tr>                                           <td>1:32</td> </tr>
        <!-- Plains -->
        <tr> <td rowspan=2>Plains</td>                 <td rowspan=2>1:4096</td>       <td rowspan=2>Island</td>           <td>Hardcoded biome at (0,0)</td> </tr>
        <tr>                                                                                                               <td>Initialized at tiles other than (0,0), with 1/10th chance</td> </tr>
        <!-- River -->
        <tr> <td>River</td>                            <td>1:4</td>                    <td>RiverMixer</td>                 <td>Replaces non-[Mushroom Fields/Mushroom Fields Shores/Oceans/Snowy Tundras], dependent on borders/Ocean-bleedthrough in zoomed river noisemap</td> </tr>
        <!-- Snowy Tundra -->
        <tr> <td rowspan=2>Snowy Tundra</td>           <td>1:1024</td>                 <td>AddSnow</td>                    <td>Replaces Plains, with 1/5th chance</td> </tr>
        <tr>                                           <td>1:256</td>                  <td>BiomeInit</td>                  <td>Replaces all Frozen Oceans</td> </tr>
        <!-- Swamp -->
        <tr> <td>Swamp</td>                            <td>1:256</td>                  <td>BiomeInit</td>                  <td>Replaces Plains, with 1/6th chance</td> </tr>
        <!-- Taiga -->
        <tr> <td>Taiga</td>                            <td>1:256</td>                  <td>BiomeInit</td>                  <td>Replaces Plains, with 1/6th chance</td> </tr>
    </tbody>
</table>