#!/usr/bin/env bash
# Build the golden-data harness against the firmware's native_test objects, then regenerate
# golden.json from corpus.txt. Run from anywhere; needs PlatformIO (`pio`) and a host C++ compiler.
#
# The native_test link step uses GNU ld's --wrap, which macOS ld rejects, so this script only asks
# PlatformIO to compile the firmware objects and links the harness itself.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
build="$root/.pio/build/native_test"
pio="${PIO:-$(command -v pio || echo "$HOME/.local/bin/pio")}"

cd "$root"
# Compiles every firmware object; the final link may fail on macOS, which is expected here.
"$pio" test -e native_test -f test_pacing --without-testing >/dev/null 2>&1 || true
for object in src/reader/ReadingLoop.o src/text/RsvpTokenizer.o src/text/TextNormalizer.o; do
    [[ -f "$build/$object" ]] || { echo "missing $build/$object; native_test compile failed" >&2; exit 1; }
done

objects=()
while IFS= read -r object; do objects+=("$object"); done < <(find "$build/src" -name '*.o' | sort)
libraries=()
while IFS= read -r library; do libraries+=("$library"); done < <(find "$build" -path '*/lib*' -name 'lib*.a' ! -name 'libUnity.a' | sort)

"${CXX:-c++}" -std=gnu++23 -O1 -w \
    -DPLATFORMIO=60200 -DARDUINO_ESP32S3_DEV_16M_OPI -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 \
    -DARDUINO_RUNNING_CORE=1 -DARDUINO_EVENT_RUNNING_CORE=1 -DBOARD_HAS_PSRAM \
    -DGLZ_DEFAULT_OPTIMIZATION_SIZE -DGLZ_DISABLE_ALWAYS_INLINE -DPIO_UNIT_TESTING -DUNIT_TEST \
    -Ilib/SheenBidi/upstream/Headers -Ilib/SheenBidi/upstream/Source -Ilib/PDFio/upstream \
    -Ilib/zlib/upstream -Ilib/HarfBuzz/upstream/src -I.pio/libdeps/native_test/glaze/include \
    -I"$build/generated" -Itest/support -Isrc \
    "$here/harness.cpp" "${objects[@]}" "${libraries[@]}" -lz -o "$build/rigels_golden_harness"

"$build/rigels_golden_harness" < "$here/corpus.txt" > "$here/golden.json"
echo "wrote $here/golden.json ($(grep -c '"t":' "$here/golden.json") tokens)"
