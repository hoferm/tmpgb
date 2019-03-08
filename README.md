# tmpgb
Game Boy emulator written in C. WIP!

## Building

### Prerequisites
- SDL2

### Windows
Put the SDL2.dll into project root and build with `make`.

### Linux
Install SDL2 and build with `make`.

## Usage
```
tmpgb [options] <rom>
Options:
  -b <bootrom>  Start with executing the bootrom
  -d            Start in debug mode
```

## Progress
#### CPU
- [x] Instructions
- [x] Timing

#### Video
- [x] Background
- [x] Sprites
- [ ] Window

#### Audio
Not implemented yet.

#### Input
Not implemented yet.

## License
This project is licensed under the MIT License - see [LICENSE](LICENSE) for details.
