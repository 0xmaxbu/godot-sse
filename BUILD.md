# Building SSE Client GDExtension

This document describes how to build the SSE Client GDExtension for different platforms.

## Prerequisites

- **Godot 4.3+** (for the .gdextension format)
- **SCons 4.0+** (build system)
- **C++17 compatible compiler**
  - macOS: Xcode clang
  - Linux: GCC 9+ or Clang 10+
  - Windows: MSVC 2019+ or MinGW-w64

## Initial Setup

```bash
# Clone the repository
git clone https://github.com/your-org/godot-sse.git
cd godot-sse

# Initialize godot-cpp submodule
git submodule update --init --recursive
```

## Build Commands

### macOS

```bash
# Debug build
scons platform=macos target=template_debug -j4

# Release build (larger, optimized)
scons platform=macos target=template_release -j4
```

Output: `demo/bin/libsse_client.macos.template_{debug,release}.framework/`

### Linux

```bash
# Debug build
scons platform=linux target=template_debug -j4

# Release build
scons platform=linux target=template_release -j4
```

Output: `demo/bin/libsse_client.linux.template_{debug,release}.x86_64.so`

### Windows

```bat
rem Debug build
scons platform=windows target=template_debug -j4

rem Release build
scons platform=windows target=template_release -j4
```

Output: `demo/bin/libsse_client.windows.template_{debug,release}.x86_64.dll`

## Build Output

After building, verify the library files exist:

```bash
# macOS
ls -lh demo/bin/libsse_client.macos.template_debug.framework/

# Linux
ls -lh demo/bin/libsse_client.linux.template_debug.x86_64.so

# Windows
dir demo/bin\libsse_client.windows.template_debug.x86_64.dll
```

## Development Build Tips

### Parallel Compilation

Always use `-j4` (or higher) to speed up builds:

```bash
scons platform=macos target=template_debug -j8
```

### Clean Build

If you encounter build issues, clean and rebuild:

```bash
scons -c
scons platform=macos target=template_debug -j4
```

### Debugging

For debugging, use the debug build and check Godot's output console:

```bash
# macOS debug symbols are included in the .framework
scons platform=macos target=template_debug -j4

# Check Godot console for SSE client logs
# Look for messages prefixed with "[SSEClient]"
```

## Troubleshooting

### "godot_cpp submodule not initialized"

```bash
git submodule update --init --recursive
```

### "library not loaded" error in Godot

Ensure the library path in `demo/bin/sse_client.gdextension` matches the actual build output.

### Build fails with "symbol not found"

Make sure you're using the correct `target` (template_debug vs template_release) that matches your Godot editor.

## CI/CD Reference

Example GitHub Actions workflow (`.github/workflows/build.yml`):

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        platform: [macos, linux, windows]
        target: [debug, release]
    runs-on: ${{ matrix.platform == 'macos' && 'macos-latest' || (matrix.platform == 'linux' && 'ubuntu-latest' || 'windows-latest') }}
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Build
        run: scons platform=${{ matrix.platform }} target=template_${{ matrix.target }} -j4
      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: sse-client-${{ matrix.platform }}-${{ matrix.target }}
          path: demo/bin/
```
