vvctre has a HTTP server running on port 47889 (can be changed with a option).  
Some endpoints can only be used when the emulation is running.

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
  "touch_device": String,
  "udp_input_address": String,
  "udp_input_port": Number,
  "udp_pad_index": Number
},
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

# GET/POST /audiodevice

Get or set the audio output device.

## Request/Reply

```json
{
  "value": String
}
```

# GET/POST /audiovolume

Get or set the HLE audio output volume.

## Request/Reply

```json
{
  "value": Number[0.0...1.0]
}
```

# GET/POST /audiospeed

Get or set the HLE audio output speed.

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

# GET /usevsync

Get whether VSync is enabled.

## Reply

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

# POST /boot

Boot a file.

## Request

```json
{
  "file": String
}
```

# POST /installciafile

Install a CIA file.

## Request

```json
{
  "file": String
}
```

# GET /cheats

Get the cheats.

## Reply

Array of:

```json
{
  "name": String,
  "type": String"can only be 'Gateway'.",
  "code": String,
  "comments": String,
  "enabled": Boolean,
  "index": Number
}
```

# GET /reloadcheats

Reloads cheats from the file.

# GET /savecheats

Saves the cheats to the file.

# POST /addcheat

Adds a cheat.

## Request

```json
{
  "name": String,
  "type": String"can only be 'Gateway'.",
  "code": String,
  "comments": String,
  "enabled": Boolean
}
```

# POST /removecheat

Removes a cheat.

## Request

```json
{
  "index": Number
}
```

# POST /updatecheat

## Request

```json
{
  "name": String,
  "type": String"can only be 'Gateway'.",
  "code": String,
  "comments": String,
  "enabled": Boolean,
  "index": Number
}
```

# GET /pause

Pauses emulation.

# GET /continue

Continues emulation.

# GET /registers/0-15

Get the ARM 0-15 register values.

## Reply

```json
[
  Number"0",
  Number"1",
  Number"2",
  Number"3",
  Number"4",
  Number"5",
  Number"6",
  Number"7",
  Number"8",
  Number"9",
  Number"10",
  Number"11",
  Number"12",
  Number"13",
  Number"14",
  Number"15"
]
```

# POST /registers/0-15

Set a ARM 0-15 register value.

## Request

```json
{
  "index": Number,
  "value": Number
}
```

# GET /registers/cpsr

Get the ARM CPSR register value.

## Reply

Value

# POST /registers/cpsr

Set the ARM CPSR register value.

## Request

```json
{
  "value": Number
}
```

# GET /registers/vfp

Get the ARM VFP register values.

## Reply

```json
[
  Number"0",
  Number"1",
  Number"2",
  Number"3",
  Number"4",
  Number"5",
  Number"6",
  Number"7",
  Number"8",
  Number"9",
  Number"10",
  Number"11",
  Number"12",
  Number"13",
  Number"14",
  Number"15",
  Number"16",
  Number"17",
  Number"18",
  Number"19",
  Number"20",
  Number"21",
  Number"22",
  Number"23",
  Number"24",
  Number"25",
  Number"26",
  Number"27",
  Number"28",
  Number"29",
  Number"30",
  Number"31"
]
```

# POST /registers/vfp

Set a ARM VFP register value.

## Request

```json
{
  "index": Number,
  "value": Number
}
```

# GET /registers/vfpsystem

Get the ARM VFP system register values.

## Reply

```json
[
  Number"0 (FPSID)",
  Number"1 (FPSCR)",
  Number"2 (FPEXC)",
  Number"3 (FPINST)",
  Number"4 (FPINST2)",
  Number"5 (MVFR0)",
  Number"6 (MVFR1)"
]
```

# POST /registers/vfpsystem

Set a ARM VFP system register value.

## Request

```json
{
  "index": Number,
  "value": Number
}
```

# GET /registers/cp15

Get the ARM CP15 register values.

## Reply

```json
[
  Number"0 (CP15_MAIN_ID)",
  Number"1 (CP15_CACHE_TYPE)",
  Number"2 (CP15_TCM_STATUS)",
  Number"3 (CP15_TLB_TYPE)",
  Number"4 (CP15_CPU_ID)",
  Number"5 (CP15_PROCESSOR_FEATURE_0)",
  Number"6 (CP15_PROCESSOR_FEATURE_1)",
  Number"7 (CP15_DEBUG_FEATURE_0)",
  Number"8 (CP15_AUXILIARY_FEATURE_0)",
  Number"9 (CP15_MEMORY_MODEL_FEATURE_0)",
  Number"10 (CP15_MEMORY_MODEL_FEATURE_1)",
  Number"11 (CP15_MEMORY_MODEL_FEATURE_2)",
  Number"12 (CP15_MEMORY_MODEL_FEATURE_3)",
  Number"13 (CP15_ISA_FEATURE_0)",
  Number"14 (CP15_ISA_FEATURE_1)",
  Number"15 (CP15_ISA_FEATURE_2)",
  Number"16 (CP15_ISA_FEATURE_3)",
  Number"17 (CP15_ISA_FEATURE_4)",
  Number"18 (CP15_CONTROL)",
  Number"19 (CP15_AUXILIARY_CONTROL)",
  Number"20 (CP15_COPROCESSOR_ACCESS_CONTROL)",
  Number"21 (CP15_TRANSLATION_BASE_TABLE_0)",
  Number"22 (CP15_TRANSLATION_BASE_TABLE_1)",
  Number"23 (CP15_TRANSLATION_BASE_CONTROL)",
  Number"24 (CP15_DOMAIN_ACCESS_CONTROL)",
  Number"25 (CP15_RESERVED)",
  Number"26 (CP15_FAULT_STATUS)",
  Number"27 (CP15_INSTR_FAULT_STATUS)",
  Number"28 (CP15_INST_FSR)",
  Number"29 (CP15_FAULT_ADDRESS)",
  Number"30 (CP15_WFAR)",
  Number"31 (CP15_IFAR)",
  Number"32 (CP15_WAIT_FOR_INTERRUPT)",
  Number"33 (CP15_PHYS_ADDRESS)",
  Number"34 (CP15_INVALIDATE_INSTR_CACHE)",
  Number"35 (CP15_INVALIDATE_INSTR_CACHE_USING_MVA)",
  Number"36 (CP15_INVALIDATE_INSTR_CACHE_USING_INDEX)",
  Number"37 (CP15_FLUSH_PREFETCH_BUFFER)",
  Number"38 (CP15_FLUSH_BRANCH_TARGET_CACHE)",
  Number"39 (CP15_FLUSH_BRANCH_TARGET_CACHE_ENTRY)",
  Number"40 (CP15_INVALIDATE_DATA_CACHE)",
  Number"41 (CP15_INVALIDATE_DATA_CACHE_LINE_USING_MVA)",
  Number"42 (CP15_INVALIDATE_DATA_CACHE_LINE_USING_INDEX)",
  Number"43 (CP15_INVALIDATE_DATA_AND_INSTR_CACHE)",
  Number"44 (CP15_CLEAN_DATA_CACHE)",
  Number"45 (CP15_CLEAN_DATA_CACHE_LINE_USING_MVA)",
  Number"46 (CP15_CLEAN_DATA_CACHE_LINE_USING_INDEX)",
  Number"47 (CP15_DATA_SYNC_BARRIER)",
  Number"48 (CP15_DATA_MEMORY_BARRIER)",
  Number"49 (CP15_CLEAN_AND_INVALIDATE_DATA_CACHE)",
  Number"50 (CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_MVA)",
  Number"51 (CP15_CLEAN_AND_INVALIDATE_DATA_CACHE_LINE_USING_INDEX)",
  Number"52 (CP15_INVALIDATE_ITLB)",
  Number"53 (CP15_INVALIDATE_ITLB_SINGLE_ENTRY)",
  Number"54 (CP15_INVALIDATE_ITLB_ENTRY_ON_ASID_MATCH)",
  Number"55 (CP15_INVALIDATE_ITLB_ENTRY_ON_MVA)",
  Number"56 (CP15_INVALIDATE_DTLB)",
  Number"57 (CP15_INVALIDATE_DTLB_SINGLE_ENTRY)",
  Number"58 (CP15_INVALIDATE_DTLB_ENTRY_ON_ASID_MATCH)",
  Number"59 (CP15_INVALIDATE_DTLB_ENTRY_ON_MVA)",
  Number"60 (CP15_INVALIDATE_UTLB)",
  Number"61 (CP15_INVALIDATE_UTLB_SINGLE_ENTRY)",
  Number"62 (CP15_INVALIDATE_UTLB_ENTRY_ON_ASID_MATCH)",
  Number"63 (CP15_INVALIDATE_UTLB_ENTRY_ON_MVA)",
  Number"64 (CP15_DATA_CACHE_LOCKDOWN)",
  Number"65 (CP15_TLB_LOCKDOWN)",
  Number"66 (CP15_PRIMARY_REGION_REMAP)",
  Number"67 (CP15_NORMAL_REGION_REMAP)",
  Number"68 (CP15_PID)",
  Number"69 (CP15_CONTEXT_ID)",
  Number"70 (CP15_THREAD_UPRW)",
  Number"71 (CP15_THREAD_URO)",
  Number"72 (CP15_THREAD_PRW)",
  Number"73 (CP15_PERFORMANCE_MONITOR_CONTROL)",
  Number"74 (CP15_CYCLE_COUNTER)",
  Number"75 (CP15_COUNT_0)",
  Number"76 (CP15_COUNT_1)",
  Number"77 (CP15_READ_MAIN_TLB_LOCKDOWN_ENTRY)",
  Number"78 (CP15_WRITE_MAIN_TLB_LOCKDOWN_ENTRY)",
  Number"79 (CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS)",
  Number"80 (CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS)",
  Number"81 (CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE)",
  Number"82 (CP15_TLB_DEBUG_CONTROL)",
  Number"83 (CP15_TLB_FAULT_ADDR)",
  Number"84 (CP15_TLB_FAULT_STATUS)"
]
```

# POST /registers/cp15

Set a ARM CP15 register value.

## Request

```json
{
  "index": Number,
  "value": Number
}
```

# GET /restart

Restarts emulation.

# GET /reloadcameras

Reloads cameras.

# GET/POST /texturefilter

Get or set texture filtering settings.

## Request/Reply

```json
{
  "name": String"can be none, xBRZ freescale, Anime4K Ultrafast, or Bicubic",
  "factor": Number
}
```
