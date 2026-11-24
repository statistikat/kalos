"""Exercise find_package(kalos) and reconfigure the same downstream build tree."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(*args, expect_failure=False):
    result = subprocess.run(args, capture_output=True, text=True)
    if expect_failure and result.returncode and "KalosOpenBLAS" in result.stdout + result.stderr:
        return
    if result.returncode or expect_failure:
        raise RuntimeError(f"{args}\n{result.stdout}\n{result.stderr}")


def check(build, provider, work):
    install = work / "installed"
    run("cmake", "--install", build, "--prefix", install)
    source = work / "consumer"
    source.mkdir()
    (source / "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.20)
project(consumer LANGUAGES CXX)
find_package(kalos CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE kalos::kalos)
""")
    (source / "main.cpp").write_text("""#include <kalos/detail/seats_ucarima_model.hpp>
#include <kalos/detail/lapack.hpp>
int main() { return kalos::lapack::provider_version() ? 0 : 1; }
""")
    tree = work / "build"
    configure = ["cmake", "-S", source, "-B", tree, f"-DCMAKE_PREFIX_PATH={install}"]
    run(*configure, f"-DOpenBLAS_ROOT={provider}")
    run("cmake", "--build", tree)
    run(tree / "consumer")
    other = work / "other-provider"
    shutil.copytree(provider / "include", other / "include")
    shutil.copytree(provider / "lib", other / "lib", symlinks=True)
    run(*configure, f"-DOpenBLAS_ROOT={other}")
    run("cmake", "--build", tree)
    run(tree / "consumer")
    run(*configure, f"-DOpenBLAS_ROOT={work / 'absent'}", expect_failure=True)

    # Provide valid headers and a real library exporting only the config getter.
    fake = work / "incomplete"
    shutil.copytree(provider / "include", fake / "include")
    (fake / "stub.cpp").write_text('extern "C" char* openblas_get_config() { return nullptr; }')
    (fake / "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.20)
project(stub LANGUAGES CXX)
add_library(openblas STATIC stub.cpp)
install(TARGETS openblas ARCHIVE DESTINATION lib)
""")
    run("cmake", "-S", fake, "-B", fake / "build")
    run("cmake", "--build", fake / "build")
    run("cmake", "--install", fake / "build", "--prefix", fake)
    run(*configure, f"-DOpenBLAS_ROOT={fake}", expect_failure=True)
    run(*configure, f"-DKalosOpenBLAS_INCLUDE_DIR={provider / 'include'}", expect_failure=True)
    run(*configure, "-UKalosOpenBLAS_INCLUDE_DIR", f"-DOpenBLAS_ROOT={provider}")
    run("cmake", "--build", tree)
    run(tree / "consumer")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("provider", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temp:
        check(args.build.resolve(), args.provider.resolve(), Path(temp))
