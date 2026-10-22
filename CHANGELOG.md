# Changelog

All notable changes to Radmind are documented here. This changelog covers the
changes introduced after `radmind-1.14.0` through `radmind-1.15.0`.

## [1.15.0] - 2026-10-22

### Added

- `fsdiff -E` generates shell-safe transcript paths and link targets: spaces
  are encoded as `\ ` rather than `\b`. Readers now also accept `\ `, so
  transcripts can be copied into shell commands without changing their paths.
- `fsdiff -t` omits differences for files whose only changed attribute is the
  modification timestamp, allowing smaller transcripts when timestamps are not
  significant.
- `rash -C generate` prints a SHA-1 checksum for the difference transcript of
  applied changes. `rash -C <checksum>` verifies that checksum in `auto` mode
  and warns about a mismatch in `update` and `force` modes.
- `contrib/specialist` helps generate special-file transcript entries from a
  path list or an existing transcript.
- Docker image, multi-architecture (including ARM64), Docker Compose, macOS
  package, Ubuntu binary, Homebrew, and GitHub release build workflows.
- `bootstrap.sh` prepares a source checkout for the `./configure && make`
  build workflow.

### Changed

- Updated the bundled `libsnet` dependency and moved its source to the
  GitHub-hosted project.
- Modernized TLS support for OpenSSL 1.1 and later, while retaining
  compatibility shims for older OpenSSL releases. Configuration now detects
  the current OpenSSL version symbol.
- Improved macOS support: APFS resource-fork writes work even when the target
  has no existing resource fork; builds and packages support current macOS
  environments; and the macOS installer now uses `pkgbuild`/`productbuild`.
- The build supports parallel `make`, has corrected program prerequisites, and
  produces uniquely named distribution archives. `--with-pam` accepts a path;
  configure also detects the appropriate `makedev(3)` header and common Fink
  installation prefixes.
- Radmind now uses `libsnet` as a Git submodule; checkout and build
  documentation has been updated accordingly.
- Reworked the project documentation into `README.md`, with build, Docker,
  Homebrew, and project-history guidance.

### Fixed

- Eliminated EVP digest-context leaks and double frees in transfer operations,
  along with several other memory and file-handle leaks.
- Prevented a base64 encode/decode buffer overrun and corrected Finder-info
  encoding length handling.
- Fixed OpenSSL API deprecation and opaque-structure build failures, including
  FreeBSD and Apple-system-OpenSSL compatibility.
- Fixed potential path-buffer overflow when processing `file_root`.
- Fixed 64-bit `time_t` support on NetBSD and formatting of custom file mode,
  user, and group metadata.
- `lapply -F` now warns rather than fails when a file to remove is already
  absent; it also handles APFS resource forks correctly.
- `fsdiff -1 -` applies exclusions to stdin paths, skips missing stdin paths
  with a warning, and has more robust relative/absolute path canonicalization.
- Special-file metadata lookup now checks a per-file transcript, the special
  directory transcript, and the default special transcript, with clearer
  diagnostics and safe path handling.
- Improved configuration parsing: allocation failures are checked, nested
  `@include` depth is counted correctly, and rejected hosts report the
  configuration file involved.
- `t2pkg` no longer relies on unnecessary `realpath` calls; existing output
  directories are accepted. The `lcreate` manual page was also corrected.

### Notes

- The default transcript space encoding remains `\b` for compatibility with
  older Radmind readers. Use `fsdiff -E` only where consumers support `\ `.
- Development infrastructure, CI maintenance, formatting cleanup, and release
  automation changes are included in this release but do not alter Radmind's
  runtime protocol.

## [1.14.1] - 2010-12-12

### Fixed

- `lmerge -f` now handles relative paths correctly; pre-existing directories
  are accepted instead of being treated as errors.

[1.15.0]: https://github.com/Radmind/radmind/compare/radmind-1.14.1...radmind-1.15.0
[1.14.1]: https://github.com/Radmind/radmind/compare/radmind-1.14.0...radmind-1.14.1
