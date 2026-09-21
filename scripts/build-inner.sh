#!/bin/bash
set -euo pipefail
# The builder must match the Bookworm/amd64 runtime. Only locally cached
# dependency packages are installed; the build container has no network.
if [[ $(id -u) = 0 ]]; then
    dpkg -i \
        /deps/libnl-3-200_3.7.0-0.2+b1sonic1_amd64.deb \
        /deps/libnl-3-dev_3.7.0-0.2+b1sonic1_amd64.deb \
        /deps/libnl-route-3-200_3.7.0-0.2+b1sonic1_amd64.deb \
        /deps/libnl-route-3-dev_3.7.0-0.2+b1sonic1_amd64.deb \
        /deps/libnl-nf-3-200_3.7.0-0.2+b1sonic1_amd64.deb \
        /deps/libyang_1.0.73_amd64.deb \
        /deps/libswsscommon_1.0.0_amd64.deb \
        /deps/libswsscommon-dev_1.0.0_amd64.deb
    exec runuser -u "${STP_BUILD_USER:?Set the non-root user present in your builder}" -- bash /recipe/scripts/build-inner.sh
fi
cd /work/source
STP_TEST_PCAP_OUT=/work/wire-regression.pcap bash tests/run-wire-tests.sh
autoreconf --force --install
dpkg-buildpackage -b -us -uc -j4
