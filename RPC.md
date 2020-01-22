# GET /version

Get version information.

## Reply

`vvctre`: vvctre's version  
`movie`: movie file version  
`shader_cache`: shader disk cache version

# POST /memory/read

Read memory.

## Request

```json
{
  "address": Number,
  "size": Number
}
```

## Reply

Array

# POST /memory/write

Write memory.

## Request

```json
{
  "address": Number,
  "data": Array
}
```

# GET /padstate

Get the pad state.

## Example reply

```json
{
  "a": false,
  "b": false,
  "circle_down": false,
  "circle_left": false,
  "circle_right": false,
  "circle_up": false,
  "debug": false,
  "down": false,
  "gpio14": false,
  "hex": 0,
  "l": false,
  "left": false,
  "r": false,
  "right": false,
  "select": false,
  "start": false,
  "up": false,
  "x": false,
  "y": false
}
```

# POST /padstate

Set the pad state.

## Body for setting a custom pad state

```json
{
  "hex": Number
}
```

## Body for using the real state

```json
{}
```

## Reply

`hex`'s bits.

```json
{
  "a": Boolean,
  "b": Boolean,
  "select": Boolean,
  "start": Boolean,
  "right": Boolean,
  "left": Boolean,
  "up": Boolean,
  "down": Boolean,
  "r": Boolean,
  "l": Boolean,
  "x": Boolean,
  "y": Boolean,
  "debug": Boolean,
  "gpio14": Boolean,
  "circle_right": Boolean,
  "circle_left": Boolean,
  "circle_up": Boolean,
  "circle_down": Boolean
}
```

# GET/POST /circlepadstate

Get or set the circle pad state.

## Example body or reply

```json
{
  "x": 0.0,
  "y": 0.0
}
```

## Body for using the real state

```json
{}
```

# GET/POST /touchstate

Get or set the touch screen state.

## Example body or reply

```json
{
  "pressed": false,
  "x": 0.0,
  "y": 0.0
}
```

## Body for using the real state

```json
{}
```

# GET/POST /motionstate

Get or set the motion state.

## Example body or reply

```json
{
  "accel": {
    "x": 0.0,
    "y": -1.0,
    "z": 0.0
  },
  "gyro": {
    "x": 0.0,
    "y": 0.0,
    "z": 0.0
  }
}
```

## Body for using the real state

```json
{}
```

# GET /screenshot

Replies with a screenshot in PNG format.

# GET /layout

Get the current layout.

## Reply

```json
{
  "swap_screens": Boolean,
  "is_rotated": Boolean,
  "width": Number,
  "height": Number,
  "top_screen": {
    "width": Number,
    "height": Number,
    "left": Number,
    "top": Number,
    "right": Number,
    "bottom": Number
  },
  "bottom_screen": {
    "width": Number,
    "height": Number,
    "left": Number,
    "top": Number,
    "right": Number,
    "bottom": Number
  }
}
```

# POST /layout/custom

Sets the layout to a custom layout.

## Request

```json
{
  "top_screen": {
    "left": Number,
    "top": Number,
    "right": Number,
    "bottom": Number
  },
  "bottom_screen": {
    "left": Number,
    "top": Number,
    "right": Number,
    "bottom": Number
  }
}
```

# GET /layout/default

Sets the layout to Default Top Bottom Screen.

# GET /layout/singlescreen

Sets the layout to Single Screen Only.

# GET /layout/largescreen

Sets the layout to Large Screen Small Screen.

# GET /layout/sidebyside

Sets the layout to Side by Side.

# GET /layout/mediumscreen

Sets the layout to Large Screen Medium Screen.

# POST /layout/swapscreens

Sets swap screens enabled.

## Request

```json
{
  "enabled": Boolean
}
```

# POST /layout/upright

Sets upright orientation enabled.

## Request

```json
{
  "upright": Boolean
}
```

# GET/POST /backgroundcolor

Get/set background color.

## Request/Reply

```json
{
  "red": Number[0.0...1.0],
  "green": Number[0.0...1.0],
  "blue": Number[0.0...1.0]
}
```

# GET /customticks

Get custom tick settings.

## Reply

```json
{
  "enabled": Boolean,
  "ticks": Number
}
```

# POST /customticks

Set custom tick settings.

## Request

```json
{
  "enabled": Boolean,
  "ticks": Number
}
```

# GET /speedlimit

Get speed limit settings.

## Reply

```json
{
  "enabled": Boolean,
  "percentage": Number
}
```

# POST /speedlimit

Set speed limit settings.

## Request

```json
{
  "enabled": Boolean,
  "percentage": Number
}
```

# POST /amiibo

Load a amiibo or remove the current amiibo.

## Request

Empty body to remove the current amiibo, or amiibo file contents to load a amiibo.

# GET/POST /3d

Get or set 3D settings.

## Request/Reply

```json
{
  "mode": Number,
  "intensity": Number[0...100]
}
```

# GET/POST /microphone

Get or set microphone settings.

## Request/Reply

type = 0: none, type = 1: real, type = 2: static

```json
{
  "type": Number,
  "device": String
}
```

# GET/POST /resolution

Get or set the resolution.

## Request/Reply

0 is a special value for `resolution` to scale resolution to window size.

```json
{
  "resolution": Number
}
```

# GET/POST /frameadvancing

Get or set whether frame advancing is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET /frameadvancing/advance

Advances a frame. enables frame advancing if it isn't enabled.

# GET/POST /controls

Get or set input settings.

## Request/Reply

Profile:
```json
{
  "analogs": [
    String"CirclePad",
    String"CStick"
  ],
  "buttons": [
    String"A",
    String"B",
    String"X",
    String"Y",
    String"Up",
    String"Down",
    String"Left",
    String"Right",
    String"L",
    String"R",
    String"START",
    String"SELECT",
    String"Debug",
    String"GPIO14",
    String"ZL",
    String"ZR",
    String"Home"
  ],
  "motion_device": String,
  "name": String,
  "touch_device": String,
  "udp_input_address": String,
  "udp_input_port": Number,
  "udp_pad_index": Number
},
```

```json
{
  "current_profile": Profile,
  "current_profile_index": Number,
  "profiles": Array<Profile>
}
```

# GET/POST /cpuclockpercentage

Get or set CPU clock percentage.

## Request/Reply

```json
{
  "value": Number
}
```

# GET/POST /multiplayerurl

Get or set the multiplayer server URL.

## Request/Reply

```json
{
  "value": String
}
```

# GET/POST /usehardwarerenderer

Get or set whether the hardware renderer is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /usehardwareshader

Get or set whether the hardware shader is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /usediskshadercache

Get or set whether disk shader caching is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /shaderaccuratemultiplication

Get or set whether shader accurate multiplication is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /useshaderjit

Get or set whether the shader JIT is used instead of the interpreter for software shader.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET /filtermode

Get the filtering mode.

## Reply

```json
{
  "mode": String"nearest|linear"
}
```

# GET /filtermode/nearest

Sets the filtering mode to nearest.

# GET /filtermode/linear

Sets the filtering mode to linear.

# GET/POST /postprocessingshader

Get or set the post processing shader name.

Builtin shaders:
- Anaglyph 3D only: dubois (builtin)
- Interlaced 3D only: horizontal (builtin)
- 3D off only: none (builtin)

## Request/Reply

```json
{
  "name": String
}
```

# GET/POST /customscreenrefreshrate

Get or set the 3DS screen refresh rate.

## Request/Reply

```json
{
  "enabled": Boolean,
  "value": Number
}
```

# GET/POST /minverticesperthread

Get or set the minimum vertices per thread (software shader only).

## Request/Reply

```json
{
  "value": Number
}
```

# GET/POST /dumptextures

Get or set whether texture dumping is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /customtextures

Get or set whether custom textures are used.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /preloadcustomtextures

Get or set whether custom texture preloading is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /usecpujit

Get or set whether the CPU JIT is used instead of the interpreter.
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /ignoreformatreinterpretation

Get or set whether ignore format reinterpretation is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /dspemulation

Get or set DSP emulation settings.  
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "emulation": String"hle|lle",
  "multithreaded (HLE only)": Boolean
}
```

# GET/POST /audioengine

Get or set the audio output engine.

## Request/Reply

```json
{
  "name": String"auto|cubeb|sdl2"
}
```

# GET/POST /audiostretching

Get or set whether audio output stretching is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /audiodevice

Get or set the audio output device.

## Request/Reply

```json
{
  "value": String
}
```

# GET/POST /audiovolume

Get or set the audio output volume.

## Request/Reply

```json
{
  "value": Number[0.0...1.0]
}
```

# GET/POST /audiospeed

Get or set the audio output speed.

## Request/Reply

```json
{
  "value": Number">0.0"
}
```

# GET/POST /usevirtualsdcard

Get or set whether a virtual SD card is used.  
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /isnew3ds

Get or set whether the console is a New 3DS.  
This doesn't fix New 3DS game crashes.  
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /region

Get or set the console's region.  
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "value": Number"-1=auto-select|0=Japan|1=USA|2=Europe|3=Australia|4=China|5=Korea|6=Taiwan"
}
```

# GET/POST /startclock

Get or set the time to use when emulation starts.  
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "clock": String"system|fixed",
  "unix_timestamp": String"optional"
}
```

# GET/POST /usevsync

Get or set whether VSync is enabled.  
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /logfilter

Get or set the log filter.

## Request/Reply

```json
{
  "value": String
}
```

# GET/POST /recordframetimes

Get or set the whether frametime recording is enabled.

## Request/Reply

```json
{
  "enabled": Boolean
}
```

# GET/POST /cameras

Get or set camera settings.

## Request/Reply

```json
{
  "name": Array<String"length: 3",
  "config": Array<String>"length: 3",
  "flip": Array<Number"length: 3"
}
```

# GET/POST /gdbstub

Get or set GDB stub enabled and port.

## Request/Reply

```json
{
  "enabled": Boolean,
  "port": Number
}
```

# GET/POST /llemodules

Get or set LLE modules.  
If setting and the emulation is running, the emulation will restart.

## Request/Reply

```json
{
  "AC": Boolean,
  "ACT": Boolean,
  "AM": Boolean,
  "BOSS": Boolean,
  "CAM": Boolean,
  "CDC": Boolean,
  "CECD": Boolean,
  "CFG": Boolean,
  "CSND": Boolean,
  "DLP": Boolean,
  "DSP": Boolean,
  "ERR": Boolean,
  "FRD": Boolean,
  "FS": Boolean,
  "GPIO": Boolean,
  "GSP": Boolean,
  "HID": Boolean,
  "HTTP": Boolean,
  "I2C": Boolean,
  "IR": Boolean,
  "LDR": Boolean,
  "MCU": Boolean,
  "MIC": Boolean,
  "MP": Boolean,
  "MVD": Boolean,
  "NDM": Boolean,
  "NEWS": Boolean,
  "NFC": Boolean,
  "NIM": Boolean,
  "NS": Boolean,
  "NWM": Boolean,
  "PDN": Boolean,
  "PM": Boolean,
  "PS": Boolean,
  "PTM": Boolean,
  "PXI": Boolean,
  "QTM": Boolean,
  "SOC": Boolean,
  "SPI": Boolean,
  "SSL": Boolean
}
```

# GET /movie

Get the movie state.

## Reply

```json
{
  "playing": Boolean,
  "recording": Boolean
}
```

# GET /movie/stop

Stop movie playback/recording.

# POST /movie/play and /movie/record

Play or record a movie.

## Request

```json
{
  "file": String
}
```
