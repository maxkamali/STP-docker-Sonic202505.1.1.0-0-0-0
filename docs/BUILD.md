# Build

Target: Linux amd64 with matching SONiC 202505 Debian Bookworm runtime libraries.
Docker and a cached SONiC build image are required. This is not a complete SONiC
operating-system build and does not publish a runtime base image.

Keep environment-specific settings outside version control. Supply:

- `STP_BUILDER_IMAGE`: a local SONiC Bookworm builder with GCC/G++, binutils,
  Debian packaging tools, autotools, libtool, tcpdump, libnl/libevent/OpenSSL
  development headers, and sanitizer runtimes.
- `STP_BASE_IMAGE`: the compatible local STP runtime image to extend, preserving
  the deployment's manager/IPC/startup behavior.
- `STP_DEPS_DIR`: directory containing the cached dependency `.deb` packages
  listed in `scripts/build-inner.sh`. Check versions before substituting packages.
- `STP_BUILD_USER`: a non-root account in the builder with permission to write
  the bind-mounted build directory.
- `STP_WORK_DIR`: an isolated build directory; use an absolute path.

Replace these placeholders with local values:

```bash
export STP_BUILDER_IMAGE='YOUR_CACHED_BUILDER_IMAGE'
export STP_BASE_IMAGE='YOUR_COMPATIBLE_STP_IMAGE'
export STP_DEPS_DIR='/absolute/path/to/cached/bookworm/packages'
export STP_BUILD_USER='BUILDER_ACCOUNT'
export STP_WORK_DIR="$PWD/.work/build"
export STP_OUTPUT_IMAGE='docker-stp:wirefix-v1'
bash scripts/prepare-source.sh "$STP_WORK_DIR/source"
bash scripts/build.sh
```

Preparation refuses to overwrite an existing directory. It checks out the
pinned public commit, applies the three patches, and installs the tests. To use
an existing upstream checkout offline, set `STP_UPSTREAM_SOURCE` to its absolute
path. Only the pinned commit is exported; local modifications are not copied.

Compilation and container assembly run with networking disabled. Dependencies
and the recipe are mounted read-only. The build resolves local image IDs at
startup and prints them for a private record. Builder and runtime base tags are
preserved; the selected output tag is created or updated.
The recipe uses Docker's legacy builder; environments without it need an
equivalent BuildKit setup with access to the same local base image.

Outputs under `STP_WORK_DIR` include the package, debug package and test PCAP.
The default output image is `docker-stp:wirefix-v1`. Keep build outputs private
until individually reviewed.

To rerun only the tests inside a prepared source tree in the matching builder:

```bash
bash tests/run-wire-tests.sh
```

The harness links production `stp_data.c`, `stp_util.c`, and `stp_pkt.c`; only
interface lookup and `sendto()` are replaced. It checks tagged/untagged PVST on
synthetic VLANs 123/456/4094, IEEE configuration and topology-change BPDUs, tagged
PVST topology changes, receive conversion, odd-address conversions and transmit
limits. It opens no packet socket and needs no Redis server.

Archive a successful image for a controlled transfer:

```bash
set -o pipefail
mkdir -p out
docker save docker-stp:wirefix-v1 | gzip -1 > out/docker-stp-wirefix-v1.tar.gz
sha256sum out/docker-stp-wirefix-v1.tar.gz
```

Record package/daemon checksums privately. Docker's containerd and classic image
stores can report different identifiers for the same exported content; validate
archive and daemon checksums as well as image metadata.
