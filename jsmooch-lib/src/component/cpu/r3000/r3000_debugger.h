#pragma once

#define dbgloglog(r_cat, r_severity, r_format, ...) if (dbg.dvptr->ids_enabled[r_cat]) dbg.dvptr->add_printf(r_cat, *clock, r_severity, r_format, __VA_ARGS__)
