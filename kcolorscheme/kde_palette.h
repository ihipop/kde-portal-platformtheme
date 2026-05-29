// SPDX-License-Identifier: LGPL-2.0-or-later
//
// Declarations for the local KColorScheme-style application palette builder.

#pragma once

#include "../portal/kde_portal_settings.h"

#include <QPalette>

namespace KdePortal {

QPalette createKdeApplicationPalette(const PortalSettingsMap &settings);

} // namespace KdePortal
