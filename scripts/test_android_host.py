#!/usr/bin/env python3
"""Linux JVM/JNI acceptance independent of Android SDK or emulator."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SAMPLE = ROOT / "Samples/AndroidPlatformer"


def run(compiler):
    javac = shutil.which("javac")
    if not javac:
        raise RuntimeError("JDK required")
    jdk = Path(javac).resolve().parents[1]
    java = SAMPLE / "app/src/main/java/org/kairo/platformer"
    with tempfile.TemporaryDirectory(prefix="kairo-jni-") as tmp:
        work = Path(tmp)
        subprocess.run([javac, "-Xlint:all", "-Werror", "-d", str(work),
                        str(java/"NativeGame.java"), str(java/"TouchInput.java"),
                        str(SAMPLE/"host-tests/org/kairo/platformer/HostTest.java")], check=True, timeout=60)
        subprocess.run([compiler, "-std=c++23", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-shared", "-fPIC", "-I", str(jdk/"include"), "-I", str(jdk/"include/linux"),
                        "-I", str(ROOT/"Runtime/KairoPlayer"),
                        str(SAMPLE/"app/src/main/cpp/PlatformerJni.cpp"),
                        str(ROOT/"Samples/WebPlatformer/PlatformerWasm.cpp"),
                        "-o", str(work/"libkairo_mobile.so")], check=True, timeout=90)
        subprocess.run([str(jdk/"bin/java"), "-Djava.library.path="+str(work), "-cp", str(work),
                        "org.kairo.platformer.HostTest"], check=True, timeout=60)


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler",default="c++")
    run(parser.parse_args().compiler)
