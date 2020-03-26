vvctre is a New Nintendo 3DS emulator based on Citra for Windows and Linux.

vvctre is licensed under the GPLv2 (or any later version).  
Refer to the license.txt file included.

|                            |                                     Citra                                      |                                                            vvctre |
| -------------------------- | :----------------------------------------------------------------------------: | ----------------------------------------------------------------: |
| Out of the box multiplayer |                                       No                                       |                                                               Yes |
| Scripting requests         |                               Read/write memory                                | [Here](https://github.com/vvanelslande/vvctre/blob/master/RPC.md) |
| Versions                   |                               Sequential number                                |                                     [SemVer](https://semver.org/) |
| CLI usage                  | [Here](https://github.com/citra-emu/citra/blob/master/src/citra/citra.cpp#L62) |          [Here](https://github.com/vvanelslande/vvctre/issues/1) |

# Portable Mode

Create a `user` folder where vvctre is.

# Requirements

- GPU with OpenGL >= 3.3
- OS:
  - 64-bit Windows >= 7
    - [Visual C++ 2019 redistributable](https://aka.ms/vs/16/release/vc_redist.x64.exe)
  - 64-bit Ubuntu >= 18.04 with packages:
    - `libsdl2-dev`
    - `libswscale-dev`
    - `libavformat-dev`
    - `libavcodec-dev`
    - `libavdevice-dev`
    - `libpng-dev`

# GUI Features

- Middle click pause menu
- Disk shader cache loading progress bar
- FPS indicator
  - You can move it while the left mouse button is held.
  - You can change its color while the right mouse button is held.
- Keyboard applet
- Mii selector applet

# Removed Features

- System camera support
- FFmpeg AAC decoder (WMF and FDK are easier to use)
- Video dumper (very slow)
- Audio stretching setting (if disabled, audio will not work properly if audio speed is changed)

# Incompatibilities

- Different movie file format
- Different folder for mods, put mods (Luma3DS folder structure) in `sdmc/luma/titles` in vvctre's data folder (you can open it from the GUI with File -> Open Data Folder)
- Different RPC server protocol
