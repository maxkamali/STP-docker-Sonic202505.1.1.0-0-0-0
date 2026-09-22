# SONiC STP container repair

Source patches, regression tests, and a repeatable container build workflow for
the STP/PVST daemon used with SONiC 202505. This repository records the repair
and supports future iterations without publishing network configuration, device
identifiers, credentials, or runtime images.

## What was fixed

Ethernet and BPDU structures were aligned for ordinary memory access even
though the daemon sends them directly as wire bytes. This made `MAC_ADDRESS`
8 bytes instead of 6 and `MAC_HEADER` 20 bytes instead of 14. VLAN insertion
still occurred after byte 12, splitting the source MAC and corrupting the frame.
Padding also affected the BPDU body, so fixing only the MAC header was insufficient.

The patch restores packed STP/PVST wire structures, uses `memcpy`-based access
for potentially unaligned integer conversions, asserts protocol sizes/offsets
at compile time, and rejects invalid transmit-buffer lengths. A correct tagged
PVST configuration frame is 68 bytes, excluding the Ethernet FCS. The existing
88-byte buffer allowance is retained as headroom.

## Status

- The STP package and a replacement container have been built successfully.
- Packet-byte comparisons, receive validation/decode, transmit boundary tests,
  AddressSanitizer/UndefinedBehaviorSanitizer runs, and independent `tcpdump`
  decoding passed.
- Public fixtures use synthetic MAC, VLAN, and port values, not captured data.
- A controlled two-switch deployment validated container startup, ASIC delivery,
  LACP/STP convergence, root election, BPDU counters and 68-byte PVST frames.
  Every target environment still requires the deployment checks in this repository.

## Contents

| Path | Purpose |
| --- | --- |
| `patches/` | Wire-format fix, buffer headroom, and package version |
| `tests/` | Production-code regression harness with synthetic fixtures |
| `scripts/` | Prepare pinned upstream source and build with cached dependencies |
| `Dockerfile` | Install the repaired package into an operator-supplied runtime base |
| [Build guide](docs/BUILD.md) | Required inputs and build commands |
| [Deployment guide](docs/DEPLOYMENT.md) | Generic validation, activation, and rollback |
| [Change log](CHANGELOG.md) | Fix history and remaining work |
| [Privacy policy](docs/PRIVACY.md) | What must stay out of this public repository |

The baseline is public
[sonic-net/sonic-stp](https://github.com/sonic-net/sonic-stp/tree/0a74023f3a1bac67e61e2568687aaba78d4a78fc)
commit `0a74023f3a1bac67e61e2568687aaba78d4a78fc`. Preparation uses that exact
revision; this repository does not vendor a private build tree.

The runtime base and builder are operator-supplied. A registry's `latest` image
is not assumed to contain a deployment's startup or IPC fixes. See [NOTICE](NOTICE)
for the upstream license boundary.
