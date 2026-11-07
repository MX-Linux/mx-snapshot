/**********************************************************************
 * instancelock.h
 **********************************************************************
 * Copyright (C) 2026-2026 MX Authors
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 **********************************************************************/
#pragma once

#include <QtCore/QString>

// One snapshot at a time across mx-snapshot and iso-snapshot-cli: both share
// /.bind-root and the installed-to-live cleanup marker, and a second instance
// would take the first one's live state for leftovers and tear it down.
//
// The lock is a flock() on a fixed file, held until the process exits. The
// kernel drops it when the process dies, however it dies, so a crashed run can
// never lock out the next one. flock() works on a read-only descriptor, so the
// unprivileged GUI can take it without authenticating.
namespace InstanceLock
{
inline constexpr auto defaultPath = "/run/lock/mx-snapshot.lock";

enum class Result {
    Acquired,    // this process now holds the lock
    Busy,        // another process holds it
    Unavailable, // the lock file could not be opened or locked
};

// Try once, without waiting. On Acquired the descriptor is deliberately kept
// open (close-on-exec, so no child or detached editor inherits it) for the
// rest of the process. *error is set for Unavailable.
[[nodiscard]] Result acquire(const QString &path, QString *error = nullptr);
} // namespace InstanceLock
