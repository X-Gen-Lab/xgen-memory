"""Build actual source and installed C consumers without network access."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--generator", required=True)
    parser.add_argument("--status-source", default="")
    parser.add_argument("--prefix", default="")
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="package ", dir=args.work)).resolve()

    def run(command, expected=True):
        result = subprocess.run(command, capture_output=True, text=True)
        if (result.returncode == 0) != expected:
            raise RuntimeError(f"Unexpected result: {command}\n{result.stdout}\n{result.stderr}")
        return result

    def configure(source, build, extra=(), expected=True):
        return run(["cmake", "-S", str(source), "-B", str(build), "-G", args.generator,
                    f"-DCMAKE_C_COMPILER={args.compiler}", "-DCMAKE_BUILD_TYPE=Release", *extra], expected)

    def build(path):
        run(["cmake", "--build", str(path), "--config", "Release"])

    def consumer(name, body, main_source, expected=True):
        source = root / name
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.24)\nproject(consumer LANGUAGES C)\n'
            + body + '\nadd_executable(consumer main.c)\ntarget_link_libraries(consumer PRIVATE ${consume})\n', encoding="utf-8")
        (source / "main.c").write_text(main_source, encoding="utf-8")
        configure(source, source / "build", expected=expected)
        if expected:
            build(source / "build")
            candidates = [source / "build/consumer", source / "build/consumer.exe",
                          source / "build/Release/consumer.exe"]
            executable = next(path for path in candidates if path.is_file())
            run([str(executable)])

    allocator_main = '#include <xgen/memory/allocator.h>\nint main(void) { return xgm_allocator_is_valid(0); }\n'
    all_main = '''#include <xgen/memory/arena.h>
#include <xgen/memory/pool.h>
#include <xgen/memory/size_class_allocator.h>
#include <xgen/memory/tracking_allocator.h>
#include <xgen/memory/libc_allocator.h>
#include <xgen/memory/version.h>
int main(void) {
    _Alignas(max_align_t) unsigned char storage[128];
    xgm_arena_t arena;
    xgm_pool_t pools[1];
    xgm_size_class_allocator_t classes;
    const xgm_size_class_spec_t spec = {32, 2};
    xgm_tracking_allocator_t tracker;
    xgm_allocator_stats_t stats;
    if (XGM_ABI_VERSION != 1 || xgm_arena_init(&arena, storage, 128) != XGS_OK) return 1;
    if (xgm_arena_alloc(&arena, 5, 16) == 0) return 2;
    if (xgm_size_class_init(&classes, pools, 1, &spec, 1, storage, 128) != XGS_OK) return 3;
    void* block = xgm_alloc(&classes.service, 5);
    if (block == 0) return 4;
    xgm_free(&classes.service, block);
    if (xgm_size_class_deinit(&classes) != XGS_OK) return 5;
    if (xgm_tracking_allocator_init(&tracker, xgm_allocator_libc(), &stats, 0, 0) != XGS_OK) return 6;
    block = xgm_tracking_alloc(&tracker, 4);
    if (block == 0) return 7;
    xgm_tracking_free(&tracker, block);
    return xgm_tracking_allocator_deinit(&tracker) != XGS_OK;
}
'''
    for label, components in [("minimal", "allocator"),
                               ("full", "allocator;pool;size_class;arena;tracking;libc_allocator")]:
        build_dir = root / f"producer-{label}"
        prefix = root / f"install-{label}"
        extra = [f"-DXGM_COMPONENTS={components}", f"-DCMAKE_INSTALL_PREFIX={prefix}"]
        if label == "minimal":
            extra.append("-DCMAKE_DISABLE_FIND_PACKAGE_xgen_status=TRUE")
        else:
            extra += [f"-DXGM_STATUS_SOURCE_DIR={args.status_source}", f"-DCMAKE_PREFIX_PATH={args.prefix}"]
        configure(args.source, build_dir, extra)
        build(build_dir)
        run(["cmake", "--install", str(build_dir), "--config", "Release"])
        setup = f'set(CMAKE_PREFIX_PATH "{prefix.as_posix()};{args.prefix}")\n'
        if label == "minimal":
            setup += 'set(CMAKE_DISABLE_FIND_PACKAGE_xgen_status TRUE)\n'
            checks = '\nif(TARGET xgs::status OR TARGET xgm::pool)\nmessage(FATAL_ERROR "Unexpected dependency")\nendif()\n'
            consumer("installed-minimal", setup + 'find_package(xgen_memory 0.1.0 EXACT REQUIRED COMPONENTS allocator)\nset(consume xgm::allocator)' + checks, allocator_main)
            consumer("installed-default", setup + 'find_package(xgen_memory REQUIRED)\nset(consume xgm::allocator)' + checks, allocator_main)
            consumer("installed-optional", setup + 'find_package(xgen_memory REQUIRED COMPONENTS allocator OPTIONAL_COMPONENTS unavailable)\nset(consume xgm::allocator)' + checks, allocator_main)
            consumer("installed-missing", setup + 'find_package(xgen_memory REQUIRED COMPONENTS pool)', allocator_main, False)
            consumer("installed-unknown", setup + 'find_package(xgen_memory REQUIRED COMPONENTS unavailable)', allocator_main, False)
            consumer("installed-version", setup + 'find_package(xgen_memory 0.2.0 EXACT REQUIRED)', allocator_main, False)
            for identity in ("missing", "version", "abi", "type"):
                properties = {"missing": "", "version": "XGM_VERSION 9.0.0 XGM_ABI_VERSION 1",
                              "abi": "XGM_VERSION 0.1.0 XGM_ABI_VERSION 9",
                              "type": "XGM_VERSION 0.1.0 XGM_ABI_VERSION 1"}[identity]
                declaration = 'add_library(xgm::allocator INTERFACE IMPORTED)\n'
                if properties:
                    declaration += f'set_target_properties(xgm::allocator PROPERTIES {properties})\n'
                consumer(f"installed-identity-{identity}", setup + declaration + 'find_package(xgen_memory REQUIRED COMPONENTS allocator)', allocator_main, False)
        else:
            consumer("installed-full", setup + 'find_package(xgen_memory 0.1.0 EXACT REQUIRED COMPONENTS allocator pool size_class arena tracking libc_allocator)\nset(consume xgm::size_class xgm::arena xgm::tracking xgm::libc_allocator)', all_main)
    source_setup = f'set(XGM_COMPONENTS allocator CACHE STRING "" FORCE)\nset(CMAKE_DISABLE_FIND_PACKAGE_xgen_status TRUE)\nadd_subdirectory("{args.source.as_posix()}" memory)\nset(consume xgm::allocator)\n'
    consumer("source-minimal", source_setup, allocator_main)
    print("12 installed package cases and minimal source consumption passed.")


if __name__ == "__main__":
    main()
