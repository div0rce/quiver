<!-- Draft release notes for Quiver {{VERSION}} (REQ-CI-009). A maintainer reviews and edits
     before publishing (REQ-REL-003 §5 step 7). See CHANGELOG.md for the full change list. -->
## Quiver {{VERSION}}

<!-- One-paragraph summary of what this release adds. -->

### Highlights

- <!-- notable change -->

### Consuming this release

- **CMake package:** `find_package(Quiver CONFIG)` after installing, or `FetchContent` from the tag.
- **Amalgamation drop-in:** `quiver.h` + `quiver.cpp` attached below — build with a single compiler
  command (`c++ -std=c++23 quiver.cpp your_app.cpp`). See `docs/guides/vendoring.md`.

### Verifying the artifacts

`SHA256SUMS` lists the checksum of every attached file; each artifact carries a GitHub build
provenance attestation (`gh attestation verify <file> --repo div0rce/quiver`).

### Full change list

See [CHANGELOG.md](https://github.com/div0rce/quiver/blob/{{VERSION}}/CHANGELOG.md).
