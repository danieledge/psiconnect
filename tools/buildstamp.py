# buildstamp.py - a PlatformIO pre-script (platformio.ini, extra_scripts):
# compiles src/buildstamp.cpp again on every build, so the date and time it
# keeps (shown on the screen at start-up and on the System page) are this
# build's, not those of the last time the file changed.
# MIT licence (see LICENSE at the top of the repository).
import os

Import("env")  # noqa: F821 (PlatformIO's SCons environment)

obj = os.path.join(env.subst("$BUILD_DIR"), "src", "buildstamp.cpp.o")  # noqa: F821
if os.path.exists(obj):
    os.remove(obj)
