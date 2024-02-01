#!/usr/bin/env python
from glob import glob
from pathlib import Path
from SCons.Script import *

# Initialize environment and SCons script for godot-cpp
env = SConscript("godot-cpp/SConstruct")
env.Tool('msvs')

# Sources and include paths
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")
sources += Glob("src/platform/win32/*.cpp")

includes = Glob("src/*.h")
includes += Glob("src/platform/win32/*.h")

# Append additional library paths and libraries for Opus, Speex, and vpx
env.Append(CPPPATH=['#3rdparty/opus/include', '#3rdparty/speex/include', '#3rdparty/libvpx/include'])
env.Append(LIBPATH=['#3rdparty/opus/lib', '#3rdparty/speex/lib', '#3rdparty/libvpx/lib/x64'])
env.Append(LIBS=['opus', 'libspeex', 'libspeexdsp', 'vpx'])

# Determine extension and addon path
(extension_path,) = glob("export/addons/*/*.gdextension")
addon_path = Path(extension_path).parent
project_name = Path(extension_path).stem
debug_or_release = "release" if env["target"] == "template_release" else "debug"

# Generate library based on platform
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "{0}/lib/lib{1}.{2}.{3}.framework/{1}.{2}.{3}".format(
            addon_path,
            project_name,
            env["platform"],
            debug_or_release,
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "{}/lib/lib{}.{}.{}.{}{}".format(
            addon_path,
            project_name,
            env["platform"],
            debug_or_release,
            env["arch"],
            env["SHLIBSUFFIX"],
        ),
        source=sources,
    )

srcs = []
for s in sources:
    srcs.append(s.abspath)

incs = []
for s in includes:
    incs.append(s.abspath)

msvs_project = env.MSVSProject(
    target = project_name + env['MSVSPROJECTSUFFIX'],
    srcs = srcs,
    incs = incs,
    include_dirs = env['CPPPATH'],
    lib_dirs = env['LIBPATH'],
    libs = env['LIBS'],
    variant = 'Debug|x64',
)

# Make sure the default build includes the Visual Studio project
Default(library, msvs_project)
