#!/usr/bin/env python3
# Run from any directory: python3 firmware/tests/display-sensors/run_tests.py
# Compile actual production translation units; never link the simulator stub.
# The temporary directory is removed on success, failure, and timeout.

import argparse
from pathlib import Path
import shlex
import subprocess
import sys
from tempfile import TemporaryDirectory


COMPILE_TIMEOUT_S = 60
RUN_TIMEOUT_S = 10


def run(command, timeout, stage):
    print(f"[{stage}] {shlex.join(map(str, command))}", flush=True)
    try:
        result = subprocess.run(command, timeout=timeout, check=False)
    except subprocess.TimeoutExpired:
        print(f"[{stage}] TIMEOUT after {timeout}s", file=sys.stderr)
        return 124
    except OSError as error:
        print(f"[{stage}] ERROR: {error}", file=sys.stderr)
        return 1
    return result.returncode


def main():
    parser = argparse.ArgumentParser(
        description="Dependency-free host tests for display sensors, link, and USB commands."
    )
    parser.add_argument("--cxx", default="c++", help="C++ compiler executable")
    args = parser.parse_args()
    test_dir = Path(__file__).resolve().parent
    display_dir = test_dir.parent.parent / "display-v9"

    with TemporaryDirectory(prefix=".build-", dir=test_dir) as temporary:
        executable = Path(temporary) / "display-sensors"
        command = [
            args.cxx,
            "-std=c++11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I", str(test_dir),
            "-I", str(display_dir),
            str(test_dir / "test_display_sensors.cpp"),
            str(display_dir / "fake_data.cpp"),
            str(display_dir / "link_serial.cpp"),
            str(display_dir / "serial_cmd.cpp"),
            "-o", str(executable),
        ]
        result = run(command, COMPILE_TIMEOUT_S, "build")
        if result != 0:
            return result
        print("[build] PASS", flush=True)
        return run([str(executable)], RUN_TIMEOUT_S, "test")


if __name__ == "__main__":
    sys.exit(main())
