vvctre is a Nintendo 3DS emulator based on Citra for Windows and Linux.

# Requirements

- OS:
  - 64-bit Ubuntu >= 18.04 with packages:
    - `libsdl2-dev`
    - `libswscale-dev`
    - `libavformat-dev`
    - `libavcodec-dev`
    - `libavdevice-dev`
  - 64-bit Windows >= 8.1
    - [Visual C++ 2019 redistributable](https://aka.ms/vs/16/release/vc_redist.x64.exe)
- GPU with OpenGL >= 3.3

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
- Headless mode (doesn't create a window, can be controlled with RPC)
- Supported apps
  - Everything Citra supports
  - [3DSident](https://github.com/joel16/3DSident.git) <= 0.7.9
- RPC endpoints
  - Get version
  - Get screenshot in PNG format
  - Amiibo
    - Load
    - Remove
  - Input
    - Get state
    - Set state
  - Settings
    - Get
    - Set
  - Movie
    - Get state
    - Play
    - Record
  - Frame advancing
    - Get enabled
    - Set enabled
    - Advance frame
  - Files
    - Boot executable
    - Install CIA
- Error messages in vvctre and Citra's GUI, but not Citra's CLI:
  - You are running default Windows drivers for your GPU. You need to install the proper drivers for your graphics card from the manufacturer's website.
  - Your GPU may not support OpenGL 3.3, or you do not have the latest graphics driver.

# FAQ

Q: Where's the ini?  
A: On Windows: `AppData\Roaming\vvctre\config\sdl2-config.ini`

Q: Does this support cameras?  
A: Not yet.

Q: Does this support multiplayer?  
A: Yes. it will work out of the box in most games.

Q: Where's the source code of the multiplayer server?  
A: https://github.com/vvanelslande/vvctre-multiplayer.glitch.me

Q: Can I host the multiplayer server myself?  
A: Yes. the default port is 3000 and can be changed by setting the `PORT` environment variable.
`vvctre` has a option and setting to change the multiplayer URL.

Q: Where is the multiplayer server hosted and what protocol does it use?  
A: It's hosted on [Glitch](https://glitch.com/) and uses WebSocket.

Q: What compilers are supported?  
A: GCC and MSVC
