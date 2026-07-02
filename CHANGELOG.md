# Changelog

All notable changes to Quiver are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and Quiver adheres to [Semantic Versioning](https://semver.org/) (0.x rules per Charter §7.5: breaking changes permitted with a minor bump and an entry here).

## [Unreleased]

### Added

- M0 — Repository bootstrap: governance files (LICENSE, CONTRIBUTING, SECURITY), documentation skeleton with per-directory ownership, materialized ADR-001…ADR-026 under `docs/adr/`, configure-only CMake skeleton with the full option surface (REQ-BUILD-006) and presets (REQ-BUILD-011), CI skeleton (format / repo-lint / docs-build / configure gates), MkDocs documentation site, and the M0 gate record (`docs/releases/gates/M0.md`).

No library code exists yet; the first kernels ship with v0.1 (milestone M3).
