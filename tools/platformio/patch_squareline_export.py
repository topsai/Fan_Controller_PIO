from pathlib import Path
import re

Import("env")

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
GENERATED_UI_DIR = PROJECT_DIR / "src" / "transmitter_s3" / "ui" / "generated"
GENERATED_UI_C = GENERATED_UI_DIR / "ui.c"

SWAP_CHECK = """#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif"""

PATCHED_SWAP_CHECK = """#if 0 && LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif"""

IMAGE_SWAP_MARKER = "patch_squareline_export: byte-swapped for LV_COLOR_16_SWAP=1"
BYTE_PATTERN = re.compile(r"0x[0-9a-fA-F]{2}")
TRUE_COLOR_PATTERN = re.compile(r"\.header\.cf\s*=\s*LV_IMG_CF_TRUE_COLOR\s*,")


def patch_swap_check():
    if not GENERATED_UI_C.exists():
        return False

    text = GENERATED_UI_C.read_text(encoding="utf-8")
    if SWAP_CHECK not in text:
        return False

    GENERATED_UI_C.write_text(text.replace(SWAP_CHECK, PATCHED_SWAP_CHECK), encoding="utf-8")
    return True


def patch_true_color_images():
    if not GENERATED_UI_DIR.exists():
        return False

    patched = False
    for image_file in GENERATED_UI_DIR.glob("ui_img_*.c"):
        text = image_file.read_text(encoding="utf-8")
        if IMAGE_SWAP_MARKER in text or not TRUE_COLOR_PATTERN.search(text):
            continue

        array_start = text.find("const LV_ATTRIBUTE_MEM_ALIGN uint8_t")
        if array_start < 0:
            continue
        brace_start = text.find("{", array_start)
        brace_end = text.find("};", brace_start)
        if brace_start < 0 or brace_end < 0:
            continue

        array_body = text[brace_start + 1:brace_end]
        byte_values = BYTE_PATTERN.findall(array_body)
        if len(byte_values) < 2 or len(byte_values) % 2 != 0:
            continue

        swapped_values = []
        for index in range(0, len(byte_values), 2):
            swapped_values.extend((byte_values[index + 1], byte_values[index]))

        lines = []
        for index in range(0, len(swapped_values), 16):
            lines.append("    " + ",".join(swapped_values[index:index + 16]) + ",")

        new_array_body = "\n" + "\n".join(lines) + "\n"
        patched_text = (
            text[:brace_start + 1]
            + new_array_body
            + text[brace_end:]
            + f"\n// {IMAGE_SWAP_MARKER}\n"
        )
        image_file.write_text(patched_text, encoding="utf-8")
        patched = True

    return patched


def patch_squareline_export(source, target, env):
    patch_swap_check()
    patch_true_color_images()


patch_squareline_export(None, None, env)
env.AddPreAction("buildprog", patch_squareline_export)
