# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Migration guide for transitioning from adapters to NetworkSystemBridge pattern
  - Comprehensive step-by-step migration instructions
  - API comparison tables for old vs new patterns
  - Common migration patterns and examples
  - Troubleshooting section

### Deprecated
- `thread_system_pool_adapter` class
  - Replaced by `ThreadPoolBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `common_thread_pool_adapter` class
  - Replaced by `ThreadPoolBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `common_logger_adapter` class
  - Replaced by `ObservabilityBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `common_monitoring_adapter` class
  - Replaced by `ObservabilityBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `bind_thread_system_pool_into_manager()` function
  - Replaced by `NetworkSystemBridge::with_thread_system()`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

### Deprecation Timeline
- **v2.1.0** (current): Deprecated adapters marked with `[[deprecated]]` attribute
- **v2.2.0** (Q2 2026): Migration strongly encouraged
- **v3.0.0** (Q3 2026): Deprecated adapters will be removed

Users are encouraged to migrate to the new `NetworkSystemBridge` facade as soon as possible.
See the [migration guide](docs/migration/adapter_to_bridge_migration.md) for detailed instructions.

---

## How to Read This Changelog

- **Added**: New features
- **Changed**: Changes in existing functionality
- **Deprecated**: Features that will be removed in future versions
- **Removed**: Removed features
- **Fixed**: Bug fixes
- **Security**: Security fixes

For migration assistance, please refer to the migration guides in the `docs/migration/` directory.
