//
// Created by RadDad772 on 3/2/24.
//

#pragma once

#include "helpers/cdrom_formats.h"
#include "helpers/int.h"
#include "dreamcast.h"
#include "dc_mem.h"
#include "spi.h"

namespace DC {
    struct core;
}

namespace DC::GDROM {
enum gd_states {
    //Generic
    gds_waitcmd,
    gds_procata,
    gds_waitpacket,
    gds_procpacket,
    gds_pio_send_data,
    gds_pio_get_data,
    gds_pio_end,
    gds_procpacketdone,

    //Command spec.
    gds_readsector_pio,
    gds_readsector_dma,
    gds_process_set_mode,
};

struct DRIVE {
    explicit DRIVE(DC::core *parent);
    void clear_interrupt();
    void setdisc();
    void setstate(gd_states state);
    void reset();
    void write(u32 addr, u64 val, u8 sz, bool* success);
    u64 read(u32 addr, u8 sz, bool* success);
    void dma_start();
    void insert_disc(multi_file_set &mfs);
    core *bus;
    union {
        struct {
            u32 : 1;
            u32 nIEN: 1;
        };
        u32 u{};
    } device_control{};

    union {
        struct {
            u32 CHECK : 1; // 1 = error occurred
            u32 NU: 1; // RESERVED
            u32 CORR: 1; // 1 = correctable error
            u32 DRQ: 1; // 1 when data transfer with host is possible (waiting?)
            u32 DSC : 1; // 1 means seek processing complete
            u32 DF: 1; // drive fault info
            u32 DRDY: 1; // 1 when response to ATA command is possible (waiting?)
            u32 BSY: 1; // 1 when command is accepted
        };
        u8 u{};
    } device_status{};

    union {
        struct {
            u8 lo;
            u8 hi;
        };
        u16 u{};
    } byte_count{};

    union {
        struct {
            u32 mode_value: 3;
            u32 transfer_mode: 5;
        };
        u32 u{};
    } sector_count{};

    u32 ata_cmd{};

    union {
        struct {
            u32 status : 4;
            u32 disc_format : 4;
        };
        u32 u{};
    } sector_number{};

    union {
        struct {
            u32 ILI: 1;
            u32 EOMF: 1;
            u32 ABORT: 1;
            u32 MCR: 1;
            u32 ERROR: 4;
        };
        u32 u{};
    } error{};

    u32 sns_asc{};
    u32 sns_ascq{};
    u32 sns_key{};

    u32 features{};
    u32 cmd{};

    gd_states state{};

    struct {
        u32 playing{};
    } cdda{};

    struct {
        union {
            struct {
                u32 CoD:1; //Bit 0 (CoD) : "0" indicates data and "1" indicates a command.
                u32 IO:1;  //Bit 1 (IO)  : "1" indicates transfer from device to host{}, and "0" from host to device.
                u32 any :6;//not used
            };
            u8 u{};
        };
    } interrupt_reason{};

    SPI_packet_cmd packet_cmd{};

    struct GDROM_PIOBUF {
        gd_states next_state{};
        u32 index{};
        u32 size{};
        u16 data[0x8000]{}; //64 kb
    } pio_buff{};

    CDROM_DISC disc{};

private:
    void spi_pio_end(const u8* buffer, u32 len, gd_states next_state);
    void process_spi_cmd();
    void ATA_command();
};

}

