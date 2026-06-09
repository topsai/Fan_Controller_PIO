from pathlib import Path

Import("env")

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
GENERATED_UI_C = PROJECT_DIR / "src" / "transmitter_s3" / "ui" / "generated" / "ui.c"

SWAP_CHECK = """#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif"""

PATCHED_SWAP_CHECK = """#if 0 && LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif"""


def patch_squareline_export(source, target, env):
    if not GENERATED_UI_C.exists():
        return

    text = GENERATED_UI_C.read_text(encoding="utf-8")
    if SWAP_CHECK not in text:
        return

    GENERATED_UI_C.write_text(text.replace(SWAP_CHECK, PATCHED_SWAP_CHECK), encoding="utf-8")


patch_squareline_export(None, None, env)
env.AddPreAction("buildprog", patch_squareline_export)
