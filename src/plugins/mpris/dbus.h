#pragma once

extern "C" {
    #include <systemd/sd-bus.h>
}

extern const sd_bus_vtable musikcube_mp_table[];
extern const sd_bus_vtable musikcube_mpp_table[];
