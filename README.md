vvctre is a Nintendo 3DS emulator based on Citra for Windows and Linux.  
vvctre is licensed under the GPLv2 (or any later version).
Refer to the license.txt file included.

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
  - 64-bit Windows >= 8.1
    - [Visual C++ 2019 redistributable](https://aka.ms/vs/16/release/vc_redist.x64.exe)
  - 64-bit Ubuntu >= 18.04 with packages:
    - `libsdl2-dev`
    - `libswscale-dev`
    - `libavformat-dev`
    - `libavcodec-dev`
    - `libavdevice-dev`

# Usage

<details>
  <summary>Easy</summary>

Ways to use:

- Open vvctre
- Drop a file on vvctre

</details>

<details>
  <summary>Advanced</summary>

Run `vvctre` in a terminal.  
For a list of commands and options, run `vvctre usage`.

</details>

# Unique Features

- [SemVer](https://semver.org/) versions instead of a simple numbers and random strings
- Hidden mode (can be controlled with RPC)
- Supported apps
  - Everything Citra supports
  - [3DSident](https://github.com/joel16/3DSident.git) <= 0.7.9
- Error messages in vvctre and Citra's GUI, but not Citra's CLI:
  - You are running default Windows drivers for your GPU. You need to install the proper drivers for your graphics card from the manufacturer's website.
  - Your GPU may not support OpenGL 3.3, or you do not have the latest graphics driver.
- More options
- [Better RPC](RPC.md)

# Removed Features

- GUI
- System camera support

# Incompatibilities

- No multiplayer rooms, multiplayer will work out of the box, and [anyone can host the multiplayer server](https://github.com/vvanelslande/vvctre-multiplayer.glitch.me).
- Movies created with any Citra build can't be played.
- Movies created with vvctre can't be played on any Citra build.
- Different folder for mods. put mods (Luma3DS folder structure) in `user folder/sdmc/luma/titles`.
- Different RPC server.
