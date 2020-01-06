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

## Reply

```json
{
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

# GET /backgroundcolor

Get the background color.

## Reply

`red`, `green` and `blue` are floats in range 0.0-1.0.

```json
{
  "red": Number,
  "green": Number,
  "blue": Number
}
```

# POST /backgroundcolor

Sets the background color.

## Request

`red`, `green` and `blue` are floats in range 0.0-1.0.

```json
{
  "red": Number,
  "green": Number,
  "blue": Number
}
```
