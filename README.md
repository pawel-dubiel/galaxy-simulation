# C-Galaxy Elite

Space simulation software written in C using SDL2.

## Dependencies

- SDL2 development libraries
- pkg-config
- GCC compiler

## Build

To compile the project, run:

```
make
```

## Usage

To start the application, run:

```
./galaxy
```

The application accepts an optional argument to force a specific number of stars:

```
./galaxy --stars=100
```

## Controls

### Galaxy Map Mode

- **Arrow Keys**: Pan camera
- **+/-**: Zoom in/out
- **Scroll Wheel**: Zoom in/out
- **Left Click**: Select star and enter system view
- **TAB**: Switch mode (if inside a system)

### Tactical View

Displays a top-down view of the star system with boundaries.

- **T**: Switch to Cockpit View
- **TAB**: Return to Galaxy Map

### Cockpit View

3D ship simulation mode.

- **W / S**: Pitch down / up
- **A / D**: Yaw left / right
- **Q / E**: Roll left / right
- **R / F**: Throttle increase / decrease
- **T**: Switch to Tactical View
- **TAB**: Return to Galaxy Map
- **SPACE**: Pause simulation
