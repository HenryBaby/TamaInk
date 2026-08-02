Import("env")

from pathlib import Path
import shutil
import subprocess

project = Path(env.subst("$PROJECT_DIR"))
source = project / "tamalib"
build_root = Path(env.subst("$BUILD_DIR"))
staging_root = Path(env.subst("$PROJECT_WORKSPACE_DIR")) / "tamaink-tamalib-source"
stage = staging_root / "tamalib"
expected_revision = "ce304d55f9a73c60232ce3f552e7983db3fa399c"

try:
    revision = subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
    ).strip()
except (OSError, subprocess.CalledProcessError) as exc:
    raise RuntimeError("unable to verify pinned tamalib revision") from exc
if revision != expected_revision:
    raise RuntimeError(f"unsupported tamalib revision: {revision}")

if staging_root.exists():
    shutil.rmtree(staging_root)
stage.mkdir(parents=True)
for name in ("cpu.c", "cpu.h", "hw.c", "hw.h", "tamalib.c", "tamalib.h", "hal.h"):
    shutil.copyfile(source / name, stage / name)
hal_types_source = source / "hal_types.h.template"
hal_types = hal_types_source.read_text(encoding="utf-8")
for declaration in ("typedef unsigned int u32_t;", "typedef unsigned int timestamp_t;"):
    if hal_types.count(declaration) != 1:
        raise RuntimeError(f"unexpected TamaLib type declaration: {declaration}")
hal_types = hal_types.replace(
    "#define _HAL_TYPES_H_\n", "#define _HAL_TYPES_H_\n\n#include <stdint.h>\n", 1
)
hal_types = hal_types.replace("typedef unsigned int u32_t;", "typedef uint32_t u32_t;")
hal_types = hal_types.replace(
    "typedef unsigned int timestamp_t;", "typedef uint32_t timestamp_t;"
)
(staging_root / "hal_types.h").write_text(hal_types, encoding="utf-8")

markers = ("previous_cycles", "tamalib_export_extended_state", "tamalib_import_extended_state")
combined = "".join((stage / name).read_text(encoding="utf-8") for name in ("cpu.c", "cpu.h", "tamalib.c", "tamalib.h"))
if not all(marker in combined for marker in markers):
    patch = project / "patches" / "tamalib-live-state.patch"
    check = subprocess.run(["git", "apply", "--check", str(patch)], cwd=stage)
    if check.returncode != 0:
        raise RuntimeError("staged tamalib is missing live-state patch and cannot be patched")
    apply = subprocess.run(["git", "apply", str(patch)], cwd=stage)
    if apply.returncode != 0:
        raise RuntimeError("failed to apply live-state patch to staged tamalib")
    combined = "".join((stage / name).read_text(encoding="utf-8") for name in ("cpu.c", "cpu.h", "tamalib.c", "tamalib.h"))
if not all(marker in combined for marker in markers):
    raise RuntimeError("staged tamalib live-state API markers are incomplete")

cpu = stage / "cpu.h"
text = cpu.read_text(encoding="utf-8")
if text.count("#define E0C6S48_SUPPORT") != 1:
    raise RuntimeError("tamalib cpu.h must contain exactly one E0C6S48_SUPPORT define")
text = text.replace("#define E0C6S48_SUPPORT\n", "")
if text.count("#define E0C6S48_SUPPORT") != 0:
    raise RuntimeError("failed to remove E0C6S48_SUPPORT define")
if text.count("#define E0C6S46_SUPPORT") != 1:
    raise RuntimeError("tamalib cpu.h must retain exactly one E0C6S46_SUPPORT define")
cpu.write_text(text, encoding="utf-8")

env.Append(CPPDEFINES=["TAMAINK_TAMALIB", "E0C6S46_SUPPORT"])
env.Append(CPPPATH=[str(stage), str(staging_root)])
env.BuildSources(str(build_root / "tamalib"), str(stage), src_filter=["+<*.c>"])
