# Deployment and rollback

This is a generic procedure, not a captured site configuration. Validate service
and image references on each target. Use out-of-band management and a maintenance
window: restarting STP can change forwarding state and interrupt traffic.

Old and corrected packet layouts are incompatible. Upgrade participating peers
in the same window; keep redundant Layer-2 paths isolated until correct root and
port states are verified. Do not change LACP membership or VLAN configuration
merely to install this fix.

## Preflight and backup

Transfer the privately built image and verify its checksum. Inspect
`systemctl cat stp`. If no unit exists, inspect the running container's image,
restart policy, network mode, capabilities, environment and mounts; use the
direct-Docker method below. Confirm the base image preserves the required
manager, IPC header, startup behavior and libraries. Keep those checks and
existing spanning-tree/LAG state in a private record.

Run in Bash from a writable persistent backup location:

```bash
set -o pipefail
backup_dir=$(mktemp -d "$PWD/stp-rollback.XXXXXX")
old_image=$(sudo docker inspect stp --format '{{.Image}}')
rollback_tag="docker-stp:rollback-$(date +%Y%m%d%H%M%S)"
printf '%s\n' "$old_image" > "$backup_dir/image-id.txt"
sudo docker inspect stp > "$backup_dir/container-inspect.json"
sudo docker diff stp > "$backup_dir/container-diff.txt"
sudo docker tag "$old_image" "$rollback_tag"
sudo docker save "$rollback_tag" | gzip -1 > "$backup_dir/image.tar.gz"
gzip -t "$backup_dir/image.tar.gz"
printf 'Keep this backup directory privately: %s\n' "$backup_dir"
sudo docker load -i /path/to/docker-stp-wirefix-v1.tar.gz
sudo docker image inspect docker-stp:wirefix-v1
```

Check every command succeeded. Saving an image does not preserve manual changes
in a container's writable layer; review `container-diff` and preserve intentional
edits before removing the container. Keep backups, configuration, inspection
output, hashes and logs out of this public repository.

## Activate one switch

### Systemd-managed container

Use this only when `systemctl cat stp` identifies the actual launcher. Run each
command only after the previous one succeeds:

```bash
sudo systemctl stop stp
sudo docker rm stp
sudo docker tag docker-stp:wirefix-v1 docker-stp:latest
sudo systemctl start stp
sudo systemctl is-active stp
sudo docker inspect stp --format '{{.Image}}'
sudo docker exec stp dpkg-query -W stp
sudo docker exec stp sha256sum /usr/bin/stpd
sudo docker exec stp supervisorctl status
```

If the stop action already removed the container, verify its absence before
continuing. Restarting an existing container alone does not select a new image.
Verify `stpd` and `stpmgrd` reach `RUNNING`, the package is `1.0.0+wirefix1`, and
the daemon checksum matches the private build record. Allow for the base image's
Redis-readiness timeout. Inspect failures using `journalctl -u stp` and
`docker logs stp`, keeping the output private.

After startup is verified, repeat on the peer. Receive counts can stay zero
while the other peer still sends the malformed layout.

### Direct Docker container

Use this only after confirming these options match the current container. Create
the replacement first, preserve the stopped original under a unique name, then
swap names. Do not remove the original during the maintenance window.

```bash
new_image=docker-stp:wirefix-v1
stamp=$(date +%Y%m%d%H%M%S)
old_container="stp-before-wirefix-$stamp"
candidate="stp-wirefix-candidate-$stamp"
printf '%s\n' "$old_container" > "$backup_dir/container-name.txt"

sudo docker create \
    --name "$candidate" \
    --restart unless-stopped \
    --network host \
    --cap-add NET_ADMIN \
    --cap-add SYS_ADMIN \
    --volume /etc/localtime:/etc/localtime:ro \
    --volume /etc/sonic:/etc/sonic:ro \
    --volume /var/run/redis:/var/run/redis:rw \
    "$new_image"

sudo docker stop --time 30 stp
sudo docker rename stp "$old_container"
sudo docker rename "$candidate" stp
sudo docker start stp
```

Verify the image ID, package, daemon checksum, restart count and both supervised
processes. If startup fails, rename the failed replacement, restore the original
name and start the original container.

```bash
sudo docker inspect stp --format \
    'image={{.Image}} status={{.State.Status}} restarts={{.RestartCount}}'
sudo docker exec stp dpkg-query -W stp
sudo docker exec stp sha256sum /usr/bin/stpd
sudo docker exec stp supervisorctl status stpd stpmgrd
```

## Verify the pair

Check spanning-tree root election and port state for every configured VLAN,
LAG state, and tagged VLAN membership. LACP and hardware reconciliation can lag
behind container startup. Wait up to five minutes before treating a down LAG as
a deployment failure:

```bash
validation_lag=${STP_VALIDATION_LAG:?Set the inter-switch PortChannel name}
for check in $(seq 1 20); do
    printf 'LAG check %s/20: ' "$check"
    cat "/sys/class/net/$validation_lag/operstate"
    if [ "$(cat "/sys/class/net/$validation_lag/operstate")" = up ]; then
        break
    fi
    sleep 15
done
test "$(cat "/sys/class/net/$validation_lag/operstate")" = up
show interfaces portchannel
sudo teamdctl "$validation_lag" state dump
```

If a switch rebooted during the window, `uptime -s` and container start times
explain the fresh convergence cycle. Restart the five-minute observation window.
Do not roll back solely because a LAG is down while its physical members and
LACP are still converging.

Capture PVST on the physical member actually carrying the packets, substituting
its interface name locally. A selected LAG can transmit on only one member for
this flow, so try every selected member before concluding that no BPDU exists:

```bash
sudo timeout 20 tcpdump -Q in -eni INTERFACE -s 0 -vv -c 5 \
    'ether dst 01:00:0c:cc:cc:cd'
```

Expect correctly decoded STP/PVST with contiguous source addresses and sensible
timers/bridge IDs. Tagged configuration frames generated by the repair are
68 bytes without FCS. Check receive counters where BPDUs are expected; a root's
designated port need not continually receive classic STP configuration BPDUs.
Peers must agree on the intended root per VLAN.

In a healthy root/non-root pair, transmit and receive counters are naturally
asymmetric: the root sends periodic configuration BPDUs and the non-root receives
them. Counter asymmetry by itself is not a failure.

Restore isolated redundant paths only after convergence and forwarding state
are verified. A healthy container alone is not acceptance. If valid incoming
frames are visible but expected receive counts remain zero, investigate the
VLAN/AUXDATA path; this patch does not change its socket filter.

## Rollback

### Systemd-managed container

Use that switch's backup directory and verify each command succeeds:

```bash
backup_dir=/path/to/private/backup
old_image=$(cat "$backup_dir/image-id.txt")
sudo docker load -i "$backup_dir/image.tar.gz"
sudo docker image inspect "$old_image" --format '{{.Id}}'
sudo systemctl stop stp
sudo docker rm stp
sudo docker tag "$old_image" docker-stp:latest
sudo systemctl start stp
sudo docker exec stp supervisorctl status
```

### Direct Docker container

The original stopped container is the fastest rollback. Verify the active
container is the replacement before swapping names:

```bash
backup_dir=/path/to/private/backup
old_container=$(cat "$backup_dir/container-name.txt")
failed_container="stp-wirefix-failed-$(date +%Y%m%d%H%M%S)"
sudo docker inspect stp --format '{{.Image}}'
sudo docker stop --time 30 stp || true
sudo docker rename stp "$failed_container"
sudo docker rename "$old_container" stp
sudo docker start stp
sudo docker exec stp supervisorctl status stpd stpmgrd
```

Account for service scripts that remove stopped containers. Revert participating
peers in the same window. Rollback restores the prior image, including its known
wire-layout defect. A full SONiC operating-system upgrade can replace custom
images; retain these patches for future builds.
