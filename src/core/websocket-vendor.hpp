/*
Control Panel for OBS Studio
Copyright (C) 2026 Voidscape Development

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

namespace cp::websocket {

constexpr const char *kVendorName = "obs-control-panel";

/* Register the vendor and its requests. Call from obs_module_post_load; a
 * no-op if obs-websocket isn't installed. */
void start();
void stop();

} // namespace cp::websocket
