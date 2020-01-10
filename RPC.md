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

## Reply

```json
{
  "hex": Number,
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

# POST /padstate

Set the pad state.

## Request for setting a custom pad state

```json
{
  "hex": Number
}
```

## Request for using the real pad state

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

# GET /circlepadstate

Get the circle pad state.

## Reply

```json
{
  "x": Number[0.0...1.0],
  "y": Number[0.0...1.0]
}
```

# POST /circlepadstate

Set the circle pad state.

## Request

```json
{
  "x": Number[0.0...1.0],
  "y": Number[0.0...1.0]
}
```

# GET /touchstate

Get the touch screen state.

## Reply

```json
{
  "x": Number[0.0...1.0],
  "y": Number[0.0...1.0],
  "pressed": Boolean
}
```

# POST /touchstate

Set the touch screen state.

## Request

```json
{
  "x": Number[0.0...1.0],
  "y": Number[0.0...1.0],
  "pressed": Boolean
}
```

# GET /screenshot

Replies with a screenshot in PNG format.

# GET /layout

Get the current layout.

## Reply

```json
{
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
  "enabled": Boolean,
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
