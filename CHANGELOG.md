# Change log

## wirefix-v1 — 2026-09-21

Implemented:

- Restore 6-byte MAC addresses and 14-byte Ethernet headers.
- Restore complete packed IEEE STP and PVST configuration/TCN layouts.
- Replace integer-pointer casts in identifier conversions with safe loads/stores.
- Add compile-time wire-size and field-offset assertions.
- Validate transmit sizes before VLAN insertion or copying to the stack buffer.
- Preserve the 88-byte buffer allowance; correct tagged PVST occupies 68 bytes.
- Add full-frame fixtures, receive-decode checks, unaligned-access tests and
  buffer boundary tests. Run optimized and sanitizer builds.
- Provide a versioned STP Debian package and separately tagged image build.

Previously established compatibility work, preserved by choosing the appropriate
runtime base:

- Matching STP manager/daemon IPC layout.
- Waiting for Redis before starting STP processes.
- Runtime packaging compatibility with Debian Bookworm.

Those pre-existing runtime changes are not silently reapplied by this patch
set. Select and verify a base image that already contains required changes.
Deployment-specific files and original build outputs remain private.

Public packaging:

- Replace environment-derived regression values with synthetic values.
- Generalize paths, image names, build users and deployment instructions.
- Use package version `1.0.0+wirefix1` for the public recipe. Previously built
  local artifacts can have a different suffix and different checksums; this
  repository does not claim they are byte-identical.

Deployment validation:

- Validate a controlled two-switch pair with both repaired containers.
- Confirm correct 68-byte tagged PVST frames in both directions.
- Confirm root election, forwarding state and asymmetric root/non-root counters.
- Preserve the original stopped container for immediate rollback.
- Allow up to five minutes for LACP and hardware reconciliation before declaring
  a failed cutover; restart that observation window after a switch reboot.

Remaining validation:

- Additional hardware/software combinations need their own acceptance checks.
- MSTP is outside the tested scope; the pinned baseline's STP Makefile does not
  compile its implementation.
