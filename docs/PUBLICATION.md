# Source publication

This initial publication was prepared from local development commit
`7933c575b1b650bff7efb5f471877fb77ac5d6d8` on 2026-10-01.
The development history remains local. The public repository starts with a
new snapshot authored and committed by djultra64. Historical commit hashes
in development records identify local builds and are not public Git objects.

The snapshot adds the public README, the owner-supplied Blocksphere banner,
and the collected third-party notice inventory. Two historical ROM command
examples use generic paths instead of personal paths. Runtime source, build
tools, tests, and dependency locks are retained from the development snapshot.

ROM data, generated game code, extracted assets, saves, captured media,
credentials, private dependencies, and local build artifacts are excluded.
The images under assets/branding are owner-supplied project branding.
No binary release is included. Binary distribution still requires matching
notices and the applicable corresponding-source materials to the exact binaries.

The complete test suite assumes locally prepared pinned dependencies and,
for integration scenarios, a private ROM and recompiler. Running it on a
fresh source checkout without those inputs produces missing-input errors.
Publication checks cover file provenance, identity, private-data exclusions,
README links, banner integrity, and tests that do not need private inputs.
These checks do not establish a newly compiled or playtested game build.
