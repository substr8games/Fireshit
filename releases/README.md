# Fireshit 1.1 Release Artifacts

Prebuilt binaries are intentionally not included in this source release.

The binaries formerly present in the draft archive were built from the previously published Fireshit 1.0 source and were not rebuilt during the licensing correction. Relabeling those executables as 1.1 would misrepresent their build provenance.

Build Fireshit 1.1 from the repository root:

```bash
./run.sh build
```

Install the resulting suite through the same authority:

```bash
./run.sh install
```

When official 1.1 binaries are built, publish them as separate release assets with checksums generated from those exact artifacts.
