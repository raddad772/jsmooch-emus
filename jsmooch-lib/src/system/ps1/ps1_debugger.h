//
// Created by . on 2/16/25.
//

#pragma once

enum PS1_DBLOG_CATEGORIES {
    PS1D_UNKNOWN = 0,
    PS1D_R3000_INSTRUCTION = 1,
    PS1D_R3000_RFE = 2,
    PS1D_R3000_EXCEPTION = 3,

    PS1D_DMA_CH0=4,
    PS1D_DMA_CH1=PS1D_DMA_CH0+1,
    PS1D_DMA_CH2=PS1D_DMA_CH0+2,
    PS1D_DMA_CH3=PS1D_DMA_CH0+3,
    PS1D_DMA_CH4=PS1D_DMA_CH0+4,
    PS1D_DMA_CH5=PS1D_DMA_CH0+5,
    PS1D_DMA_CH6=PS1D_DMA_CH0+6,

    PS1D_CDROM_CMD,
    PS1D_CDROM_FINISH_CMD,
    PS1D_CDROM_READ,
    PS1D_CDROM_PLAY,
    PS1D_CDROM_PAUSE,
    PS1D_CDROM_SETLOC,
    PS1D_CDROM_CMD_RESET,
    PS1D_CDROM_RESULT,
    PS1D_CDROM_IRQ_QUEUE,
    PS1D_CDROM_IRQ_ASSERT,
    PS1D_CDROM_SECTOR_READS,
    PS1D_CDROM_RDDATA_START,
    PS1D_CDROM_RDDATA_FINISH,

    PS1D_BUS_IRQs,
    PS1D_BUS_CONSOLE
};

#define dbgloglog(r_cat, r_severity, r_format, ...) if (dbg.dvptr->ids_enabled[r_cat]) dbg.dvptr->add_printf(r_cat, clock.master_cycle_count+clock.waitstates, r_severity, r_format, __VA_ARGS__)
#define dbgloglog_bus(r_cat, r_severity, r_format, ...) if (bus->dbg.dvptr->ids_enabled[r_cat]) bus->dbg.dvptr->add_printf(r_cat, bus->clock.master_cycle_count+bus->clock.waitstates, r_severity, r_format, __VA_ARGS__)
#define dbgloglog_busn(r_cat, r_severity, r_format) if (bus->dbg.dvptr->ids_enabled[r_cat]) bus->dbg.dvptr->add_printf(r_cat, bus->clock.master_cycle_count+bus->clock.waitstates, r_severity, r_format)

