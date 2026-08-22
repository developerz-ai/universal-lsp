// Engine version, negotiated with clients at `initialize` and reported by the
// stdio shim's daemon handshake — a version mismatch there must respawn the
// daemon rather than speak an older protocol (docs/spec.md §6.3).
#ifndef ULSP_ENGINE_VERSION_H
#define ULSP_ENGINE_VERSION_H

#define ULSP_VERSION_MAJOR 0
#define ULSP_VERSION_MINOR 0
#define ULSP_VERSION_PATCH 1
#define ULSP_VERSION_STRING "0.0.1"

namespace ulsp {

/// Version string of the linked engine, e.g. "0.0.1".
const char* version_string() noexcept;

}  // namespace ulsp

#endif  // ULSP_ENGINE_VERSION_H
