# Public-repository hygiene

Publish source changes, synthetic fixtures, and sanitized explanations only.
Never commit tokens, SSH keys, personal names/emails, organization-specific
details, device hostnames, management addresses, serial numbers, real MAC
addresses, internal topology, or private filesystem paths.

Do not upload container archives, packages, packet captures, database dumps,
switch configurations, raw logs, inspection data or backups without separate
content review. Image layers can retain data removed from the final filesystem.
`.gitignore` is a convenience, not a security boundary: review files and history.

Test fixtures use a synthetic locally administered unicast MAC,
`02:00:00:00:00:01`. Well-known protocol multicast destinations, public upstream
URLs/commit IDs and upstream license notices are not deployment identifiers and
are retained where technically or legally necessary.

Before publishing:

1. Stage specific reviewed files and inspect `git diff --cached`.
2. Review contents, paths, commit messages and author/committer attribution.
3. Replace captured examples with independently chosen synthetic values.
4. Scan for secrets and run affected tests.
5. Keep environment inputs and build evidence outside tracked files.

Revoke or rotate exposed credentials. Removing a value from a file or branch
does not revoke it or guarantee its removal from GitHub caches or forks.
