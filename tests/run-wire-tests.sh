#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_out=$(mktemp -d /tmp/stp-wire-tests.XXXXXX)
test_sources=(tests/wire_regression.c stp/stp_util.c stp/stp_data.c stp/stp_pkt.c lib/bitmap.c lib/applog.c)
test_flags=(-g -O2 -Iinclude -Ilib -I/usr/include/libnl3 -ffunction-sections -fdata-sections -Werror=address-of-packed-member -Werror=cast-align)
gcc "${test_flags[@]}" "${test_sources[@]}" -Wl,--gc-sections -Wl,--wrap=sendto -o "$test_out/wire-test"
(cd "$test_out" && ./wire-test)
gcc "${test_flags[@]}" -fsanitize=address,undefined -fno-sanitize-recover=all "${test_sources[@]}" \
    -Wl,--gc-sections -Wl,--wrap=sendto -o "$test_out/wire-test-sanitized"
(cd "$test_out" && ASAN_OPTIONS=detect_leaks=1 ./wire-test-sanitized)
tcpdump -n -e -vv -r "$test_out/wire-regression.pcap"
if [[ -n ${STP_TEST_PCAP_OUT:-} ]]; then
    cp "$test_out/wire-regression.pcap" "$STP_TEST_PCAP_OUT"
fi
printf 'Test artifacts: %s\n' "$test_out"
