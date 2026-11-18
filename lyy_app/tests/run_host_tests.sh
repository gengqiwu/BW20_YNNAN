#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:?Pass an absolute scratch output directory}
mkdir -p "$OUT"
CC=${CC:-gcc}
FLAGS=(-std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -g -fsanitize=address,undefined)
"$CC" "${FLAGS[@]}" -I "$ROOT/lyy_app/hw/lyy_device" \
    "$ROOT/lyy_app/hw/lyy_device/lyy_frame.c" "$ROOT/lyy_app/tests/test_frame.c" -o "$OUT/test_frame"
"$OUT/test_frame"
"$CC" "${FLAGS[@]}" -include stdint.h \
    -I "$ROOT/lyy_app/tests/stubs" -I "$ROOT/lyy_app/app" -I "$ROOT/lyy_app/cfg" \
    -I "$ROOT/lyy_app/hal" -I "$ROOT/lyy_app/hw/lyy_user" \
    "$ROOT/lyy_app/app/APP_Factory.c" "$ROOT/lyy_app/app/APP_Param.c" \
    "$ROOT/lyy_app/tests/test_factory.c" -o "$OUT/test_factory"
"$OUT/test_factory"
"$CC" "${FLAGS[@]}" -I "$ROOT/lyy_app/tests/stubs" -I "$ROOT/lyy_app/hal" \
    -I "$ROOT/lyy_app/hw/lyy_device" "$ROOT/lyy_app/hw/lyy_device/lyy_mcu_ota.c" \
    "$ROOT/lyy_app/tests/test_mcu_ota.c" -o "$OUT/test_mcu_ota"
"$OUT/test_mcu_ota"
"$CC" "${FLAGS[@]}" -include stdint.h -D__HAL_LOG_H__ -include "$ROOT/lyy_app/tests/stubs/HAL_Log.h" -I "$ROOT/lyy_app/tests/stubs" -I "$ROOT/lyy_app/hal" \
    "$ROOT/lyy_app/hal/HAL_Flash.c" "$ROOT/lyy_app/tests/test_flash.c" -o "$OUT/test_flash"
TEST_FLASH_PREFIX="$OUT/flash" "$OUT/test_flash"
"$CC" "${FLAGS[@]}" -I "$ROOT/lyy_app/tests/stubs" -I "$ROOT/lyy_app/hal" \
    -I "$ROOT/lyy_app/app" -I "$ROOT/lyy_app/cfg" -I "$ROOT/lyy_app/hw/lyy_device" \
    -I "$ROOT/lyy_app/hw/lyy_user" -I "$ROOT/lyy_app/hw/lyy_screen" \
    -I "$ROOT/lyy_app/hw/lyy_thing" -I "$ROOT/lyy_app/utils" -I "$ROOT/component/network/cJSON" \
    "$ROOT/lyy_app/hw/lyy_device/lyy_zcj.c" "$ROOT/lyy_app/tests/test_zcj.c" -o "$OUT/test_zcj"
"$OUT/test_zcj"
