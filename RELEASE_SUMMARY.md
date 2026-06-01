# RYAZHENKA UPDATER - RELEASE SUMMARY

**Current Version:** v2.23.8 (2026-06-01)

## Overview

Ryazhenka Updater represents a complete redesign and stabilization of the AIO Switch Updater application. The fork introduces a branded interface, comprehensive tooling, and resolves 25+ critical stability issues accumulated in the upstream codebase.

## Key Features

### UI/UX Redesign

- New Ryazhenka branded interface with 4 switchable color palettes
- Animated procedurally-generated wave background on startup
- Branded launch splash screen with logo
- Card-based layout for Tools, About, and Settings tabs
- Smooth fade-in transitions between tab views
- Responsive touch input support (tap to press A)

### System Tooling

- **Sysmodule Manager** - Enable/disable system modules via boot2.flag toggle with safety restrictions on 7 system-critical TIDs
- **Factory Restore** - Snapshot-based recovery of /atmosphere/ directory with pre-restore backup
- **CFW Health Probe** - Sigpatches staleness detector, firmware version check, CFW status validation
- **Diagnostics** - Log viewer, diagnostic dump export, network connectivity test
- **Ecosystem Browser** - Direct link resolution and opening of About-tab ecosystem resources

### Audio and Haptics

- Procedural UI sound effects via libnx audout
- Joy-Con and Pro Controller rumble engine with configurable intensity
- Toggleable audio in Settings with lazy initialization

### Network and Downloads

- Self-update mechanism with correct SD installation path and chain-loading
- Banner system with dynamic image resolution from releases
- NX-Cheats integration card with daily cheat database updates
- Dynamic URL resolution with @latest_asset, @download_to markers
- Intelligent retry logic with exponential backoff and curl error logging
- 15-second timeout cap on downloads with per-operation curl timeouts
- TTL-based caching of nx-links catalog (6-hour default for sigpatches)

### Distribution

- Ryazhenka_AIO.zip archive for direct SD root extraction
- Backward compatibility maintained - config paths unchanged (/switch/aio-switch-updater/, /config/aio-switch-updater/)
- Internal updater file naming convention preserved (aio-switch-updater.nro on SD)

## Bug Fixes (25+)

- Startup crashes with infinite splash screen loop (StatusTab constructor blocking)
- RyazhenkaCard migration crashes on Settings/Status tabs
- Haptics thread spawn crash when HID not ready
- Banner download crash on app launch (detached thread issue)
- Timeout errors reported after successful downloads
- Application freezing on download failures
- Lost download completion signal handling
- Self-update no-op and crash (now installs to SD root with proper chain-load)
- Stale nx-links catalog (cache not updating on app version change)
- Memory leak in logging system and shutdown race condition
- UI audio initialization errors
- Settings tab crash with haptics threads
- Status/audio toggle removal in intermediate versions

## Infrastructure

### CI/CD Pipeline

- GitHub Actions workflows for build, sync-upstream, and lint (replaced upstream main.yml)
- Automatic release publication on every push to main
- Numeric tag releases (run numbers #82, #81, #80, etc.)
- Idempotent patch application system with reverse-check verification
- Daily upstream synchronization with distinct exit codes for merge conflicts and patch failures
- i18n key set diff checker (Russian/English parity)
- Cppcheck static analysis job
- devkitA64 pinned to 20260215 for SwitchWave ABI compatibility

### Build System

- Makefile configuration with APP_TITLE, APP_VERSION, APP_AUTHOR metadata
- Automatic patch regeneration on constants.hpp changes
- Build artifact packaging via pack_release.sh
- Dependency management with action version pinning

## Localization

- 228 translation keys (Russian-English parity maintained)
- Default language set to Russian (instead of system locale)
- 16 new keys added for new features (status_tab, sysmodule_manager, factory_restore, etc.)
- Bilingual README, documentation, and issue templates

## Documentation

- docs/dashboard.md - Feature overview (deprecated)
- docs/sysmodule-manager.md - Module management guide
- docs/factory-restore.md - Recovery procedure documentation
- docs/sync-policy.md - Upstream synchronization policy
- docs/build.md - Build and development guide
- TROUBLESHOOTING.md - User support documentation in Russian
- SECURITY.md - Security vulnerability reporting guidelines
- CONTRIBUTING.md - Contribution guidelines

## Release Timeline

| Version | Date | Key Changes |
|---------|------|-------------|
| v2.23.8 | 2026-06-01 | Self-update path correction and chain-load fix |
| v2.23.7 | 2026-06-01 | nx-links cache refresh on app update and TTL management |
| v2.23.6 | 2026-06-01 | Cheats database, firmware list, payloads, crash-safe workers |
| v2.23.5 | 2026-06-01 | Download timeout error elimination |
| v2.23.4 | 2026-05-31 | Retry logic implementation and curl error reporting |
| v2.23.3 | 2026-05-29 | Major UI redesign, audio/haptics integration, branding system |

## Development Scope

1. **Stability** - Elimination of 25+ critical crashes and hangs
2. **Interface** - Complete UI redesign with Ryazhenka branding and visual consistency
3. **Tooling** - System management features for Atmosphere, sigpatches, firmware
4. **Networking** - Robust download handling with intelligent retry and timeout protection
5. **Backward Compatibility** - Maintained user configuration paths and internal updater naming

## Technical Debt Resolved

- Removed startup-time network calls that caused launch crashes
- Eliminated duplicate curl_global_init calls
- Fixed upstream i18n fallback mechanism
- Resolved window coordinate access in touch handler
- Corrected HID initialization ordering in haptics system
- Proper thread lifecycle management in UI operations
- Memory safety improvements in logging and shutdown paths

## Installation

Download Ryazhenka_AIO.zip and extract to SD card root, or place ryazhenka-updater.nro in /switch/aio-switch-updater/.

SD card file naming: aio-switch-updater.nro (for internal updater compatibility)

## Repository

https://github.com/Dimasick-git/AIO-Switch-Updater
