vvctre is a New Nintendo 3DS emulator based on Citra for Windows and Linux.

vvctre is licensed under the GPLv2 (or any later version).  
Refer to the license.txt file included.

|                                  |       Citra       |                                                                                                                                                                                                                                  vvctre |
|----------------------------------|:-----------------:|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|
| Multiplayer works out of the box |         No        |                                                                                                                                                                                                                                     Yes |
| Scripting requests               | Memory read/write | Memory read/write<br>Input overriding<br>Cheat management<br>Pausing/continuing/restarting emulation<br>Getting/setting settings<br>Booting executables<br>Installing CIAs<br>Amiibo loading/unloading<br>Getting/setting CPU registers |
| Versions                         | Sequential number |                                                                                                                                                                                                                                  SemVer |
| CLI usage                        | -                 | -                                                                                                                                                                                                                                       |

# Definitions

## user folder

vvctre uses the first existing option.

- On Windows:
  - `vvctre folder\user`
  - `%AppData%\vvctre`
- On Linux:
  - `vvctre folder/user`
  - `~/.local/share/vvctre`

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
- Different folder for mods, put mods (Luma3DS folder structure) in `user folder/sdmc/luma/titles`
- Different RPC server protocol
