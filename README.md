# Salis Kuwahara Filter for Adobe After Effects 2025

A painterly effect plugin that applies the classic Kuwahara filter algorithm to create oil painting-like effects.

This is a C++ project to build a high-quality, cross-platform Kuwahara filter plugin for Adobe After Effects.

## Current Status: Help Needed!

The plugin currently **compiles successfully** on macOS (Apple Silicon / arm64) for After Effects 2025 and loads without any startup errors.

However, the visual output is **severely corrupted**. Instead of a painterly effect, it produces an image with crushed blacks and intense RGB block noise.

We have tried numerous debugging steps, including multiple rewrites of the core algorithm (both brute-force and with integral images), but the visual bug persists. We are seeking help from the community to identify the root cause.

The full details of our problem are documented in this Adobe Community post: [Link to your future Adobe Community post]

---

## Features

- Classic Kuwahara filter implementation with 4-quadrant variance analysis
- Adjustable radius (1-50 pixels)
- Mix parameter for blending with original
- Support for 8-bit and 16-bit color depths
- Optimized for Apple Silicon (arm64)
- Thread-safe for multi-frame rendering (MFR)

## Build Instructions

### Prerequisites

1. Adobe After Effects 2025 SDK
2. Xcode 16.3 or later
3. macOS 13.0 or later

### Building the Plugin

1. Open the Xcode project:
   ```
   /Users/shionshimada/Desktop/Salis_KuwaharaFilter/Xcode/SalisKuwaharaFilter.xcodeproj
   ```

2. Select the appropriate build scheme (Debug or Release)

3. Build the project (Cmd+B)

4. The compiled plugin will be at:
   ```
   build/Debug/SalisKuwaharaFilter.plugin
   ```
   or
   ```
   build/Release/SalisKuwaharaFilter.plugin
   ```

### Installation

1. Copy the built `SalisKuwaharaFilter.plugin` to:
   ```
   /Applications/Adobe After Effects 2025/Plug-ins/
   ```

2. Restart After Effects

3. Find the effect under: **Effects > Salis Effects > Salis Kuwahara Filter**

## Parameters

- **Mode**: Currently only Classic mode is implemented
- **Radius**: The size of the filter kernel (1-50 pixels)
- **Mix**: Blend percentage with original image (0-100%)

## Algorithm Details

The Kuwahara filter divides the neighborhood around each pixel into four overlapping quadrants. It calculates the mean and variance for each quadrant, then sets the output pixel to the mean color of the quadrant with the lowest variance. This creates a painterly effect by preserving edges while smoothing areas of similar color.

## License

© 2025 Salis. All rights reserved.
