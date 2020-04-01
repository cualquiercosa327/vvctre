vvctre is a New Nintendo 3DS emulator based on Citra for Windows and Linux.

vvctre is licensed under the GPLv2 (or any later version).  
Refer to the license.txt file included.

|  | Citra | vvctre |
|---------------------------------------|:------------------------------------------------------------------------------:|------------------------------------------------------------------:|
| Can use the menu in fullscreen | No | Yes |
| Can copy screenshots to the clipboard | No | Yes |
| Out of the box multiplayer | No | Yes |
| Scripting requests | Read/write memory | [Here](https://github.com/vvanelslande/vvctre/blob/master/RPC.md) |
| Versions | Sequential number | [SemVer](https://semver.org/) |

# Portable Mode

Create a `user` folder where vvctre is.

# Requirements

- GPU with OpenGL >= 3.3
- OS:
  - 64-bit Windows >= 7
    - [Visual C++ 2019 Redistributable](https://aka.ms/vs/16/release/vc_redist.x64.exe)
    - Windows N and KN only: Media Feature Pack for your Windows version (for playing AAC audio, optional)
  - 64-bit Ubuntu 18.04 with packages:
    - For running releases
      - `libsdl2-2.0-0`
      - `libpng16-16`
    - For building
      - `libsdl2-dev`
      - `libpng-dev`
      - `libfdk-aac-dev` (for playing AAC audio, optional)

# GUI Features

- Middle click pause menu
- Disk shader cache loading progress bar
- FPS indicator
  - You can move it while the left mouse button is held.
  - You can change its color while the right mouse button is held.
- Keyboard applet
- Mii selector applet

# Removed Features

- Dump RomFS
- System camera support
- FFmpeg AAC decoder (WMF and FDK are easier to use)
- Video dumper (very slow)
- Audio stretching setting (if disabled, audio will not work properly if audio speed is changed)
- CPU Clock Speed setting (https://github.com/citra-emu/citra/pull/5025#issuecomment-590103178)

# Incompatibilities

- Different movie file format
- Different folder for mods, put mods (Luma3DS folder structure) in `sdmc/luma/titles` in vvctre's data folder (you can open it from the GUI with File -> Open Data Folder)
- Different RPC server protocol
