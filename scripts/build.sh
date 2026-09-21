#!/bin/bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/.." && pwd)
: "${STP_BUILDER_IMAGE:?Set a matching cached SONiC Bookworm builder image}"
: "${STP_BASE_IMAGE:?Set the compatible cached STP runtime image}"
: "${STP_DEPS_DIR:?Set the path to matching cached Bookworm dependency packages}"
: "${STP_BUILD_USER:?Set the non-root account present in the builder}"
: "${STP_WORK_DIR:?Set the isolated directory containing source/}"
stp_output_image=${STP_OUTPUT_IMAGE:-docker-stp:wirefix-v1}
if [[ "$stp_output_image" = "$STP_BASE_IMAGE" || "$stp_output_image" = docker-stp:latest ]]; then
    echo 'Choose a separate output tag; existing runtime tags must be preserved.' >&2
    exit 1
fi
work_dir=$(cd "$STP_WORK_DIR" && pwd)
deps_dir=$(cd "$STP_DEPS_DIR" && pwd)
test -f "$work_dir/source/tests/wire_regression.c"
builder_id=$(docker image inspect "$STP_BUILDER_IMAGE" --format '{{.Id}}')
base_id=$(docker image inspect "$STP_BASE_IMAGE" --format '{{.Id}}')
printf 'Builder: %s\nRuntime base: %s\n' "$builder_id" "$base_id"
docker run --rm --network none --user root \
    -e STP_BUILD_USER \
    -v "$work_dir:/work" -v "$deps_dir:/deps:ro" -v "$repo_dir:/recipe:ro" \
    --entrypoint /bin/bash "$builder_id" /recipe/scripts/build-inner.sh
context_dir=$(mktemp -d "$work_dir/image-context.XXXXXX")
cp "$repo_dir/Dockerfile" "$context_dir/"
cp "$work_dir/stp_1.0.0+wirefix1_amd64.deb" "$context_dir/"
# Pin the resolved base ID for this build. The legacy builder accepts a local ID
# without trying to resolve it in a registry. No original image is re-tagged.
DOCKER_BUILDKIT=0 docker build --network none --pull=false \
    --build-arg "STP_BASE_IMAGE=$base_id" -t "$stp_output_image" "$context_dir"
