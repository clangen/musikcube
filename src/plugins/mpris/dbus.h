#pragma once

/// @file dbus.h
/// @brief D-Bus entry points for the MPRIS plugin.
/// @details Includes the systemd sd-bus header (selected through the
/// SDBUS_HEADER macro) and declares the vtables that expose musikcube's media
/// player and media player player interfaces on the session bus.

extern "C" {
    #include SDBUS_HEADER
}

/** @brief Vtable for the org.mpris.MediaPlayer2 interface. */
extern const sd_bus_vtable musikcube_mp_table[];
/** @brief Vtable for the org.mpris.MediaPlayer2.Player interface. */
extern const sd_bus_vtable musikcube_mpp_table[];
