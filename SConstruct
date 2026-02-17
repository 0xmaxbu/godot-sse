#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# Add our source directory to include path
env.Append(CPPPATH=["src/"])

# Collect all source files
sources = Glob("src/*.cpp")

# Build the shared library with platform-specific naming
platform = env["platform"]
target = env["target"]

if platform == "macos":
    # macOS uses .framework bundle structure
    library = env.SharedLibrary(
        "demo/bin/libsse_client.{}.{}.framework/libsse_client.{}.{}".format(
            platform, target, platform, target
        ),
        source=sources,
    )
elif platform == "linux":
    # Linux uses .so with architecture suffix
    arch = env["arch"] if "arch" in env else "x86_64"
    library = env.SharedLibrary(
        "demo/bin/libsse_client.{}.{}.{}.so".format(platform, target, arch),
        source=sources,
    )
elif platform == "windows":
    # Windows uses .dll with architecture suffix
    arch = env["arch"] if "arch" in env else "x86_64"
    library = env.SharedLibrary(
        "demo/bin/libsse_client.{}.{}.{}.dll".format(platform, target, arch),
        source=sources,
    )
else:
    # Fallback for unsupported platforms
    library = env.SharedLibrary(
        "demo/bin/libsse_client{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
