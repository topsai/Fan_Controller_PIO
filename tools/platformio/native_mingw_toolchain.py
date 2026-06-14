import os

Import("env")

if os.name == "nt":
    toolchain_dir = env.PioPlatform().get_package_dir("toolchain-gccmingw32")
    if toolchain_dir:
        bin_dir = os.path.join(toolchain_dir, "bin")
        env.PrependENVPath("PATH", bin_dir)
        env.Replace(
            CC=os.path.join(bin_dir, "gcc.exe"),
            CXX=os.path.join(bin_dir, "g++.exe"),
            AR=os.path.join(bin_dir, "gcc-ar.exe"),
            RANLIB=os.path.join(bin_dir, "gcc-ranlib.exe"),
        )
        env.Append(LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"])
