#pragma once

#include "common/Result.h"
#include "domain/Session.h"

namespace shophub::access {

// Every service method calls one of these first. Hiding a button in the GUI
// is only cosmetic; these checks are what actually protect the data.

inline Status requireSignedIn(const Session &s)
{
    return s.isValid() ? Status::ok() : Status::fail(QStringLiteral("Please sign in first."));
}

inline Status requireAdmin(const Session &s)
{
    return s.isAdmin() ? Status::ok()
                       : Status::fail(QStringLiteral("You do not have permission to do this."));
}

inline Status requireCustomer(const Session &s)
{
    return s.isCustomer() ? Status::ok()
                          : Status::fail(QStringLiteral("Only customer accounts can do this."));
}

} // namespace shophub::access
