
# Cosmo's Cosmic Adventure
A port of Cosmo's Cosmic Adventure to the Nintendo 64.

## Controls
* Jump - A
* Bomb - B
* Main Menu - Start
* Movement - Dpad or Analog stick 

## Download
You can download a precompiled binary from the [Release section](https://github.com/Ryzee119/Cosmo64/releases). This include the shareware version of the first episode ready to play.
The other 2 episodes are supported, however need files from the original game:
* To play episode two, copy `COSMO2.VOL` into `filesystem` and recompile with 'EP=2' flag.
* To play episode three, copy `COSMO3.VOL` into `filesystem` and recompile with 'EP=3' flag.

## Build
This was developed using the opensource N64 toolchain [libdragon](https://github.com/DragonMinded/libdragon). I developed it using the official docker container. The build process is something like this (Fixme. The project relies on unmerged PRs so this wont work):
```
apt-get install npm docker.io
git clone --recursive https://github.com/Ryzee119/Cosmo64.git
cd Cosmo64
npm install -g libdragon
libdragon download
libdragon start

#Build Episode 1 (The shareware game files are already setup ready to go)
libdragon make EP=1

#Build Episode 2 (Make sure the game files are in the filesystem folder)
libdragon make EP=2

#Build Episode 3 (Make sure the game files are in the filesystem folder)
libdragon make EP=3
```
This should produce a `cosmo64_epX.z64` rom file.

<img src="https://github.com/yuv422/cosmo-engine/raw/master/img/cosmo-engine.png" alt="Cosmo" width="100%"/>

## Credits
* [libdragon](https://github.com/DragonMinded/libdragon) : Licensed under the ["The Unlicense"](https://github.com/DragonMinded/libdragon/blob/trunk/LICENSE.md)
* [cosmo-engine](https://github.com/yuv422/cosmo-engine.gitk) : Licensed under [GNU General Public License v2.0](https://github.com/yuv422/cosmo-engine/blob/master/LICENSE)
* [Raskys mvs64 port](https://github.com/rasky/mvs64) : His RDP rendering work was extremely helpful.
* [3D Realms](https://3drealms.com/catalog/cosmos-cosmic-adventure_37/). Buy the game to support the developers. [GoG](https://www.gog.com/game/cosmos_cosmic_adventure) or [Steam](https://store.steampowered.com/app/358250/Cosmos_Cosmic_Adventure/).
