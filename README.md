# GameBoy Emulator and Pongus

A simple Game Boy emulator written in C, using SDL2 for rendering and input.

As well as a simple pong game written in gameboy assembly.

## Features

- Emulates core CPU instructions (Z80-like)
- Renders 160x144 display via SDL
- Supports background tilemaps
- Handles D-pad and button input
- Renders tiles from VRAM
- Simulates basic VBlank timing

## Controls

| Key      | Action      |
|----------|-------------|
| W        | Up          |
| A        | Left        |
| S        | Down        |
| D        | Right       |
| J        | B Button    |
| K        | A Button    |
| L        | Select      |
| ;        | Start       |

## Build

Make sure you have SDL2 installed. Then compile with make.


## Run

./gbemu path/to/rom.gb

## Files

    cpu.c/.h: Core CPU emulation (instructions, memory)

    display.c: SDL rendering, VRAM decoding, input handling

## TODO

    Sound emulation

    Cartridge MBC support

    Save states

## License

MIT

