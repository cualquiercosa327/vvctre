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
