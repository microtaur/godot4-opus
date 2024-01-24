#!/usr/bin/env python
from glob import glob
from pathlib import Path
from SCons.Script import MSVSProject

env = SConscript("godot-cpp/SConstruct")

# Sources
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

# Opus (Windows x64)
env.Append(CPPPATH=['#3rdparty/opus/include', '#3rdparty/speex/include'])
env.Append(LIBPATH=['#3rdparty/opus/lib', '#3rdparty/speex/lib'])
env.Append(LIBS=['opus', 'libspeex', 'libspeexdsp'])

(extension_path,) = glob("export/addons/*/*.gdextension")
addon_path = Path(extension_path).parent
project_name = Path(extension_path).stem
debug_or_release = "release" if env["target"] == "template_release" else "debug"

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

Default(library)
