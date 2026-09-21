# Repository maintenance

This repository is public. Read `docs/PRIVACY.md` before preparing a commit.
Publish only reviewed source patches, synthetic tests and generic documentation.
Never copy private build trees, configurations, logs, packet captures, binaries,
image layers, credentials, personal attribution or device identifiers into it.
Retain required public upstream license notices.

Keep environment settings and build evidence outside tracked files. Use explicit
file lists when staging. Review content and commit metadata for sensitive data.
Use the generic maintenance identity already established in this repository for
automated commits; do not import private Git history.

Prepare the pinned upstream source with `scripts/prepare-source.sh`. Run the
wire regression suite in the matching builder for code changes. Keep the
README and change log precise about what was tested and what still requires
hardware validation. Building does not authorize deploying to switches.

Preserve existing runtime images and provide rollback instructions. Do not
rewrite published history without explicit authorization.
