# Publishing Fireshit 1.1

This archive is a clean source overlay. It intentionally contains no `.git` directory and does not alter the historical `v1.0` tag.

From an existing clone of the canonical repository, replace the working tree with this archive while preserving `.git`, then review and commit the result. One project-relative approach is:

```bash
archive_root=/path/to/extracted/Fireshit-1.1
repo_root=/path/to/your/Fireshit-clone

rsync -a --delete --exclude='.git/' "$archive_root/" "$repo_root/"
cd "$repo_root"
./run.sh legal-check
git status --short
git add -A
git commit -m 'Fireshit 1.1: adopt FCIL and align the suite release'
git tag -a v1.1 -m 'Fireshit 1.1'
git push origin main
git push origin v1.1
```

Do not force-move `v1.0`. Its MIT history should remain accurate.

After building official binaries from the committed 1.1 source, attach those exact artifacts and their checksums to the `v1.1` release.
