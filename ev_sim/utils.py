from datetime import datetime, timezone
import subprocess
import sys
import os


def run_command(command, cwd=None):
    print(f">>> {command}")
    result = subprocess.run(command, shell=True, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        print("Error:\n", result.stderr)
        sys.exit(result.returncode)
    return result.stdout.strip()


def stream_subprocess(command, cwd=None):
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)

    for line in process.stdout:
        print(line, end="")

    process.wait()
    return process.returncode


def build_and_run(target: str, *args, build_dir: str = "cmake-build-release", output_folder: str = ".",
                  cmake: bool = False):
    """
    Builds a CMake target `target` and runs it with arguments `args`.
    """

    os.makedirs(build_dir, exist_ok=True)
    if cmake:
        run_command("cmake ..", cwd=build_dir)

    print(">>> Building...")
    ret = stream_subprocess(["cmake", "--build", build_dir, "--target", target, "-j", "8"])

    if ret != 0:
        print("Build failed.")
        return

    print(">>> Running...")
    ret = stream_subprocess([f"{output_folder}/{target}", *tuple(map(str, args))])
    if ret != 0:
        print("Execution failed.")


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


__all__ = ["utc_now_iso", "run_command", "stream_subprocess", "build_and_run"]
