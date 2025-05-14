#include <assert.h>
#include <stdio.h>
//#include <SDL.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <cmath>
#include "build.h"
#include "my_texture.h"
#include "application.h"
#include "helpers/sys_interface.h"
#include "helpers/cvec.h"
#include "helpers/sys_present.h"
#include "helpers/debug.h"
#include "helpers/buf.h"
#include "full_sys.h"
#include "system/dreamcast/gdi.h"
#include "helpers/physical_io.h"
#include "helpers/debugger/debugger.h"
//#include "system/gb/gb_enums.h"

// mac overlay - 14742566
// i get to    - 88219648

//#define NEWSYS
#define NEWSYS_STEP2
//#define DO_DREAMCAST
//#define SIDELOAD
//#define STOPAFTERAWHILE


#ifdef JSM_SDLR3
#define TS(f,a,b,c) f.setup(renderer, a, b, c);
#else
#ifdef JSM_WEBGPU
#define TS(f,a,b,c) f.setup(wgpu_device, a, b, c)
#else
#define TS(f,a,b,c) f##.setup(a,b,c);
#endif
#endif



#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static u32 get_closest_pow2(u32 b)
{
    //u32 b = MAX(w, h);
    u32 out = 128;
    while(out < b) {
        out <<= 1;
        assert(out < 0x40000000);
    }
    return out;
}

void test_gdi() {
    struct GDI_image foo;
    GDI_init(&foo);
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    char PATH[500];
    snprintf(PATH, sizeof(PATH), "%s/Documents/emu/rom/dreamcast/crazy_taxi", homeDir);
    printf("\nHEY! %s", PATH);
    GDI_load(PATH, "crazy_taxi/crazytaxi.gdi", &foo);

    GDI_delete(&foo);
}

u32 grab_BIOSes(struct multi_file_set* BIOSes, enum jsm_systems which)
{
    char BIOS_PATH[255];
    char BASE_PATH[255];
    char ALT_PATH[255];
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    snprintf(BASE_PATH, sizeof(BASE_PATH), "%s/Documents/emu/bios", homeDir);

    u32 has_bios = 0;
    switch(which) {
        case SYS_DREAMCAST:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/dreamcast", BASE_PATH);
            mfs_add("dc_boot.bin", BIOS_PATH, BIOSes);
            mfs_add("dc_flash.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_DMG:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/gameboy", BASE_PATH);
            mfs_add("gb_bios.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_GBC:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/gameboy", BASE_PATH);
            mfs_add("gbc_bios.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_APPLEIIe:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/apple2", BASE_PATH);
            mfs_add("apple2e.rom", BIOS_PATH, BIOSes);
            mfs_add("apple2e_video.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_ZX_SPECTRUM_48K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/zx_spectrum", BASE_PATH);
            mfs_add("zx48.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_ZX_SPECTRUM_128K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/zx_spectrum", BASE_PATH);
            mfs_add("zx128.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_MAC128K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/mac", BASE_PATH);
            mfs_add("mac128k.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_MAC512K:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/mac", BASE_PATH);
            mfs_add("mac512k.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_MACPLUS_1MB:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/mac", BASE_PATH);
            mfs_add("macplus_1mb.rom", BIOS_PATH, BIOSes);
            break;
        case SYS_PS1:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/ps1", BASE_PATH);
            mfs_add("scph1001.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_GBA:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/gba", BASE_PATH);
            mfs_add("gba_bios.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_NDS:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/nds", BASE_PATH);
            mfs_add("bios7.bin", BIOS_PATH, BIOSes);
            mfs_add("bios9.bin", BIOS_PATH, BIOSes);
            mfs_add("firmware.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_GALAKSIJA:
            has_bios = 1;
            snprintf(BIOS_PATH, sizeof(BIOS_PATH), "%s/galaksija", BASE_PATH);
            mfs_add("CHRGEN_MIPRO.bin", BIOS_PATH, BIOSes);
            mfs_add("ROM_A_28.bin", BIOS_PATH, BIOSes);
            //mfs_add("ROM_A_29.bin", BIOS_PATH, BIOSes);
            //mfs_add("ROM_B.bin", BIOS_PATH, BIOSes);
            break;
        case SYS_SG1000:
        case SYS_GENESIS_JAP:
        case SYS_GENESIS_USA:
        case SYS_MEGADRIVE_PAL:
        case SYS_NES:
        case SYS_BBC_MICRO:
        case SYS_GG:
        case SYS_ATARI2600:
        case SYS_SMS1:
        case SYS_SMS2:
        case SYS_SNES:
            has_bios = 0;
            break;
        default:
            printf("\nNO BIOS SWITCH FOR CONSOLE %d", which);
            break;
    }
    return has_bios;
}


void GET_HOME_BASE_SYS(char *out, size_t out_sz, enum jsm_systems which, const char* sec_path, u32 *worked)
{
    char BASE_PATH[500];
    char BASER_PATH[500];
    const char *homeDir = getenv("HOME");

    if (!homeDir) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    snprintf(BASER_PATH, 500, "%s/Documents/emu/rom", homeDir);

    u32 has_bios = 0;
    switch(which) {
        case SYS_SG1000:
        case SYS_SMS1:
        case SYS_SMS2:
            snprintf(out, out_sz, "%s/master_system", BASER_PATH);
            *worked = 1;
            break;
        case SYS_DREAMCAST:
            if (sec_path)
                snprintf(out, out_sz, "%s/dreamcast/%s", BASER_PATH, sec_path);
            else
                snprintf(out, out_sz, "%s/dreamcast", BASER_PATH);
            *worked = 1;
            break;
        case SYS_DMG:
        case SYS_GBC:
            snprintf(out, out_sz, "%s/gameboy", BASER_PATH);
            *worked = 1;
            break;
        case SYS_ATARI2600:
            snprintf(out, out_sz, "%s/atari2600", BASER_PATH);
            *worked = 1;
            break;
        case SYS_NES:
            snprintf(out, out_sz, "%s/nes", BASER_PATH);
            *worked = 1;
            break;
        case SYS_APPLEIIe:
            snprintf(out, out_sz, "%s/appleii", BASER_PATH);
            *worked = 1;
            break;
        case SYS_GALAKSIJA:
            snprintf(out, out_sz, "%s/galaksija", BASER_PATH);
            *worked = 1;
            break;
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
            snprintf(out, out_sz, "%s/zx_spectrum", BASER_PATH);
            *worked = 1;
            break;
        case SYS_PS1:
            snprintf(out, out_sz, "%s/ps1", BASER_PATH);
            *worked = 1;
            break;
        case SYS_GBA:
            snprintf(out, out_sz, "%s/gba", BASER_PATH);
            *worked = 1;
            break;
        case SYS_NDS:
            snprintf(out, out_sz, "%s/nds", BASER_PATH);
            *worked = 1;
            break;
        case SYS_SNES:
            snprintf(out, out_sz, "%s/snes", BASER_PATH);
            *worked = 1;
            break;
        case SYS_GENESIS_USA:
        case SYS_GENESIS_JAP:
        case SYS_MEGADRIVE_PAL:
            snprintf(out, out_sz, "%s/genesis", BASER_PATH);
            *worked = 1;
            break;
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
            snprintf(out, out_sz, "%s/mac", BASER_PATH);
            *worked = 1;
            break;
        case SYS_GG:
            snprintf(out, out_sz, "%s/gg", BASER_PATH);
            *worked = 1;
            break;
        case SYS_BBC_MICRO:
            *worked = 0;
            break;
        default:
            *worked = 0;
            printf("\nNO CASE FOR SYSTEM %d", which);
            break;
    }
}

void mfs_add_IP_BIN(struct multi_file_set* mfs)
{
    char BASER_PATH[255];
    char BASE_PATH[255];
    char ROM_PATH[255];
    u32 worked = 0;

    GET_HOME_BASE_SYS(BASE_PATH, 255, SYS_DREAMCAST, nullptr, &worked);
    if (worked == 0) return;

    mfs_add("IP.BIN", BASE_PATH, mfs);
    printf("\nLOADED IP.BIN SIZE %04llx", mfs->files[1].buf.size);
}

u32 grab_ROM(struct multi_file_set* ROMs, enum jsm_systems which, const char* fname, const char* sec_path)
{
    char BASE_PATH[255];
    char ROM_PATH[255];
    u32 worked = 0;

    GET_HOME_BASE_SYS(BASE_PATH, sizeof(BASE_PATH), which, sec_path, &worked);
    if (!worked) return 0;

    mfs_add(fname, BASE_PATH, ROMs);
    //printf("\n%d %s %s", ROMs->files[ROMs->num_files-1].buf.size > 0, BASE_PATH, fname);
    return ROMs->files[ROMs->num_files-1].buf.size > 0;
}

struct physical_io_device* load_ROM_into_emu(struct jsm_system* sys, struct cvec* IOs, struct multi_file_set* mfs)
{
    struct physical_io_device *pio = nullptr;
    switch(sys->kind) {
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
        case SYS_DREAMCAST:
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
        case SYS_APPLEIIe:
            for (u32 i = 0; i < cvec_len(IOs); i++) {
                pio = (struct physical_io_device*)cvec_get(IOs, i);
                if (pio->kind == HID_DISC_DRIVE) {
                    printf("\nINSERT DISC!");
                    pio->disc_drive.insert_disc(sys, pio, mfs);
                    break;
                }
                else if (pio->kind == HID_AUDIO_CASSETTE) {
                    pio->audio_cassette.insert_tape(sys, pio, mfs, NULL);
                    break;
                }
                pio = nullptr;
            }
            return pio;
    }
    pio = nullptr;
    for (u32 i = 0; i < cvec_len(IOs); i++) {
        pio = (struct physical_io_device*)cvec_get(IOs, i);
        if (pio->kind == HID_CART_PORT) break;
        pio = nullptr;
    }
    if (pio) pio->cartridge_port.load_cart(sys, mfs, pio);
    return pio;
}

static void setup_controller(struct system_io* io, struct physical_io_device* pio, u32 pnum)
{
    struct cvec* dbs = &pio->controller.digital_buttons;
    for (u32 i = 0; i < cvec_len(dbs); i++) {
        struct HID_digital_button* db = (struct HID_digital_button*)cvec_get(dbs, i);
        switch(db->common_id) {
            case DBCID_co_up:
                io->p[pnum].up = db;
                continue;
            case DBCID_co_down:
                io->p[pnum].down = db;
                continue;
            case DBCID_co_left:
                io->p[pnum].left = db;
                continue;
            case DBCID_co_right:
                io->p[pnum].right = db;
                continue;
            case DBCID_co_fire1:
                io->p[pnum].fire1 = db;
                continue;
            case DBCID_co_fire2:
                io->p[pnum].fire2 = db;
                continue;
            case DBCID_co_fire3:
                io->p[pnum].fire3 = db;
                continue;
            case DBCID_co_fire4:
                io->p[pnum].fire4 = db;
                continue;
            case DBCID_co_fire5:
                io->p[pnum].fire5 = db;
                continue;
            case DBCID_co_fire6:
                io->p[pnum].fire6 = db;
                continue;
            case DBCID_co_shoulder_left:
                io->p[pnum].shoulder_left = db;
                continue;
            case DBCID_co_shoulder_right:
                io->p[pnum].shoulder_right = db;
                continue;
            case DBCID_co_select:
                io->p[pnum].select = db;
                continue;
            case DBCID_co_start:
                io->p[pnum].start = db;
                continue;
        }
    }
}

void full_system::setup_ios()
{
    struct cvec *IOs = &sys->IOs;
    memset(&inputs, 0, sizeof(struct system_io));
    for (u32 i = 0; i < cvec_len(IOs); i++) {
        struct physical_io_device* pio = (struct physical_io_device*)cvec_get(IOs, i);
        switch(pio->kind) {
            case HID_TOUCHSCREEN:
                if (io.touchscreen.vec == nullptr) {
                    io.touchscreen = make_cvec_ptr(IOs, i);
                }
                continue;
            case HID_CONTROLLER: {
                if (io.controller1.vec == nullptr) {
                    io.controller1 = make_cvec_ptr(IOs, i);
                    setup_controller(&inputs, pio, 0);
                }
                else {
                    io.controller2 = make_cvec_ptr(IOs, i);
                    setup_controller(&inputs, pio, 1);
                }
                continue; }
            case HID_KEYBOARD: {
                io.keyboard = make_cvec_ptr(IOs, i);
                continue; }
            case HID_DISPLAY: {
                io.display = make_cvec_ptr(IOs, i);
                continue; }
            case HID_CHASSIS: {
                io.chassis = make_cvec_ptr(IOs, i);
                //make_cvec_ptr(IOs, i);
                struct cvec* dbs = &pio->chassis.digital_buttons;
                for (u32 j = 0; j < cvec_len(dbs); j++) {
                    struct HID_digital_button* db = (struct HID_digital_button*)cvec_get(dbs, j);
                    switch(db->common_id) {
                        case DBCID_ch_pause:
                            inputs.ch_pause = db;
                            continue;
                        case DBCID_ch_power:
                            inputs.ch_power = db;
                            continue;
                        case DBCID_ch_reset:
                            inputs.ch_reset = db;
                            continue;
                        default:
                            continue;
                    }
                }
                break; }
            case HID_MOUSE:
                io.mouse = make_cvec_ptr(IOs, i);
                break;
            case HID_DISC_DRIVE:
                io.disk_drive = make_cvec_ptr(IOs, i);
                break;
            case HID_CART_PORT:
                io.cartridge_port = make_cvec_ptr(IOs, i);
                break;
            case HID_AUDIO_CASSETTE:
                io.audio_cassette = make_cvec_ptr(IOs, i);
                break;
            default:
                break;
        }
    }
    assert(io.display.vec);
    assert(io.chassis.vec);


    output.display = &((struct physical_io_device *)cpg(io.display))->display;
}

void full_system::setup_wgpu()
{
    setup_display();
}

void full_system::setup_audio()
{
    u32 srate = 0;
    u32 lpf = 0;
    for (u32 i = 0; i < cvec_len(&sys->IOs); i++) {
        struct physical_io_device *pio = (struct physical_io_device *)cvec_get(&sys->IOs, i);

        switch(pio->kind) {
            case HID_AUDIO_CHANNEL:
                audiochans.push_back(&pio->audio_channel);
                srate = pio->audio_channel.sample_rate;
                lpf = pio->audio_channel.low_pass_filter;
                break;
        }
    }
    if (audiochans.empty()) {
        printf("\nNo audio channel found in full_sys!");
        return;
    }
    audio.init_wrapper(audiochans.size(), srate, lpf);
    audio.configure_for_fps(60);
}

void full_system::setup_bios()
{
    enum jsm_systems which = sys->kind;

    struct multi_file_set BIOSes = {};
    mfs_init(&BIOSes);

    u32 has_bios = grab_BIOSes(&BIOSes, which);
    if (has_bios) {
        sys->load_BIOS(sys, &BIOSes);
    }
    mfs_delete(&BIOSes);
}

void full_system::setup_persistent_store(struct persistent_store *ps, struct multi_file_set *mfs)
{
    printf("\nSETTING UP PERSISTENT STORE!");
    struct read_file_buf *rfb = &mfs->files[0];
    if (ps->persistent) {
        snprintf(ps->filename, sizeof(ps->filename), "%s/%s.sram", rfb->path, rfb->name);
        ps->kind = PSK_SIMPLE_FILE;
        // get file size
        printf("\nFILENAME:%s", ps->filename);
        ps->fno = fopen(ps->filename, "rb+");
        if (!ps->fno) {
            ps->fno = fopen(ps->filename, "wb");
            u8 *a = (u8 *) malloc(ps->requested_size);
            memset(a, ps->fill_value, ps->requested_size);
            fwrite(a, 1, ps->requested_size, ps->fno);
            free(a);
            fflush(ps->fno);
            fclose(ps->fno);
            fopen(ps->filename, "rb+");
        }

        fseek(ps->fno, 0, SEEK_SET);
        ps->data = malloc(ps->requested_size);
        fread(ps->data, 1, ps->requested_size, ps->fno);
        ps->old_requested_size = ps->requested_size;
    }
    else {
        ps->data = malloc(ps->requested_size);
    }

    if (ps->old_requested_size != ps->requested_size) {
        printf("\nSIZE CHANGE FROM %lldkB to %lldkB", ps->old_requested_size / 1024, ps->requested_size / 1024);
        printf("\nRemember to implement this...");
        ps->requested_size = ps->old_requested_size;
    }

    ps->actual_size = ps->requested_size;
    ps->ready_to_use = 1;
    my_ps = ps;
}

void full_system::sync_persistent_storage()
{
    if (my_ps) {
        if (my_ps->dirty && my_ps->persistent) {
            //printf("\nWriting save data..,");
            fseek(my_ps->fno, 0, SEEK_SET);
            fwrite(my_ps->data, 1, my_ps->actual_size, my_ps->fno);
            fflush(my_ps->fno);
            my_ps->dirty = 0;
        }
    }
}

full_system::~full_system() {
    sync_persistent_storage();

    destroy_system();

    if (my_ps) {
        sync_persistent_storage();
        fclose(my_ps->fno);
        my_ps = nullptr;
    }

    for (auto & dv : dasm_views) {
        cvec_delete(&dv.dasm_rows);
    }
    debugger_interface_delete(&dbgr);
}


void full_system::load_default_ROM()
{
    struct cvec *IOs = &sys->IOs;
    enum jsm_systems which = sys->kind;

    mfs_init(&ROMs);
    assert(sys);
    switch(which) {
        case SYS_NES:
            //worked = grab_ROM(&ROMs, which, "apu_test.nes", nullptr);
            //NROM
            //worked = grab_ROM(&ROMs, which, "mario.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "dkong.nes", nullptr);

            // MMC3
            //worked = grab_ROM(&ROMs, which, "kirby.nes", nullptr);
            worked = grab_ROM(&ROMs, which, "mario3.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "gauntlet.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "recca.nes", nullptr);

            // Sunsoft 5b
            //worked = grab_ROM(&ROMs, which, "gimmick_jp.nes", nullptr);

            // ANROM
            //worked = grab_ROM(&ROMs, which, "battletoads.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "marblemadness.nes", nullptr);

            // CNROM
            //worked = grab_ROM(&ROMs, which, "arkanoid.nes", nullptr);

            //GNROM
            //worked = grab_ROM(&ROMs, which, "doraemon.nes", nullptr);

            //MMC1
            //worked = grab_ROM(&ROMs, which, "zelda.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "tetris.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "metroid.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "bioniccommando.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "backtothefuture23.nes", nullptr);

            // DXROM
            //worked = grab_ROM(&ROMs, which, "indianatod.nes", nullptr);

            // VRC4
            //worked = grab_ROM(&ROMs, which, "crisisforce.nes", nullptr);

            // UxROM
            //worked = grab_ROM(&ROMs, which, "castlevania.nes", nullptr);
            //worked = grab_ROM(&ROMs, which, "contra.nes", nullptr);

            // SunSoft 5
            //worked = grab_ROM(&ROMs, which, "gimmick_jp.nes", nullptr);

            // MMC5
            //worked = grab_ROM(&ROMs, which, "castlevania3.nes", nullptr);
            break;
        case SYS_SG1000:
            worked = grab_ROM(&ROMs, which, "choplifter.sg", nullptr);
            break;
        case SYS_SMS1:
        case SYS_SMS2:
            worked = grab_ROM(&ROMs, which, "sonic.sms", nullptr);
            worked = grab_ROM(&ROMs, which, "space_harrier.sms", nullptr);
            break;
        case SYS_GG:
            //worked = grab_ROM(&ROMs, which, "megaman.gg", nullptr);
            //worked = grab_ROM(&ROMs, which, "sonic_triple.gg", nullptr);
            //worked = grab_ROM(&ROMs, which, "sonic_chaos.gg", nullptr);
            //worked = grab_ROM(&ROMs, which, "buttontest.gg", nullptr);
            //worked = grab_ROM(&ROMs, which, "gunstar.gg", nullptr);
            //worked = grab_ROM(&ROMs, which, "tails.gg", nullptr);
            worked = grab_ROM(&ROMs, which, "sonicblast.gg", nullptr);
            break;
        case SYS_DMG:
            //worked = grab_ROM(&ROMs, which, "pokemonred.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "dmg-acid2.gb", nullptr);
            worked = grab_ROM(&ROMs, which, "prehistorik.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "marioland2.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "tennis.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "link.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "drmario.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "mbc1_8mb.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "demo_in_pocket.gb", nullptr);
            //worked = grab_ROM(&ROMs, which, "m3_bgp_change_sprites.gb", nullptr);
            break;
        case SYS_GBC:
            worked = grab_ROM(&ROMs, which, "linkdx.gbc", nullptr);
            //worked = grab_ROM(&ROMs, which, "badapple.gbc", nullptr);
            // worked = grab_ROM(&ROMs, which, "densha.gbc", nullptr);
            //worked = grab_ROM(&ROMs, which, "cutedemo.gbc", nullptr);
            //worked = grab_ROM(&ROMs, which, "aitd.gbc", nullptr);
            //worked = grab_ROM(&ROMs, which, "m3_bgp_change_sprites.gb", nullptr);
            break;
        case SYS_ATARI2600:
            worked = grab_ROM(&ROMs, which, "space_invaders.a26", nullptr);
            break;
        case SYS_DREAMCAST:
            worked = grab_ROM(&ROMs, which, "crazytaxi.gdi", "crazy_taxi");
            break;
        case SYS_MAC512K:
        case SYS_MAC128K:
        case SYS_MACPLUS_1MB:
            //worked = grab_ROM(&ROMs, which, "system1_1.img", nullptr);
            worked = grab_ROM(&ROMs, which, "fd1.image", nullptr);
            break;
        case SYS_GALAKSIJA:
            worked = 1;
            break;
        case SYS_ZX_SPECTRUM_48K:
        case SYS_ZX_SPECTRUM_128K:
            worked = 1;
            break;
        case SYS_APPLEIIe:
            worked = 1;
            break;
        case SYS_NDS:
            //worked = grab_ROM(&ROMs, which, "rockwrestler.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "armwrestler.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/print_both_screens.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "armwrestler-2.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/hello_world.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/print_both_screens.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson02.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson03.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson04.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson05.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson06.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson07.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson06.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson06.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson10.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/lesson11.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/256colorTilemap.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/simple_tri.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/touch_test.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/2Dplus3D.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "libnds/bitmap_sprites.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "3d_demos/libnds3d/3d_ortho_projection.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "3d_demos/gl2d/gl2d_spriteset.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "3d_demos/nitro-engine/font_from_ram.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "3d_compressed_texture.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "pokemon_diamond.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "pmdbrt.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "pmdes.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "mariokart.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "phoenixwright.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "sm64.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "nintendogs.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "dbz2.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "rayman.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "mutha_truckers.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "recruit.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "tloz.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "dq9.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "fighting_fantasy.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "kirbycc.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "mp_hunters_rev1.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "okami_den.nds", nullptr); // try again after sprites
            //worked = grab_ROM(&ROMs, which, "tony_hawk.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "poke_black_2.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "infinite_space.nds", nullptr);
            worked = grab_ROM(&ROMs, which, "nsmb.nds", nullptr);
            //worked = grab_ROM(&ROMs, which, "examples/graphics_2d/sprites_ext_palette.nds", nullptr);

            // NEXT: basic PPU mode 3, 5
            // better FIFO
            // NORMAL mode

            break;
        case SYS_PS1:
            //RenderPolygon16BPP
            //worked = grab_ROM(&ROMs, which, "psxtest_cpu.exe", nullptr); // slammin'!
            //worked = grab_ROM(&ROMs, which, "psxtest_cpx.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "psxtest_gte.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "ps1-tests-built/gte/test-all/test-all.exe", nullptr);
            worked = grab_ROM(&ROMs, which, "ps1-tests-built/timers/timers.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "ps1-tests-built/gte-fuzz/gte-fuzz.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "redux_cpu.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "pad.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/CPUTest/CPU/ADD/CPUADD.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GPU/16BPP/RenderPolygon/RenderPolygon16BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/JOY/Joypad/Joypad.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GPU/16BPP/RenderTexturePolygon/CLUT4BPP/RenderTexturePolygonCLUT4BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GPU/16BPP/RenderTexturePolygon/CLUT8BPP/RenderTexturePolygonCLUT8BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GPU/16BPP/RenderTexturePolygon/15BPP/RenderTexturePolygon15BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GPU/16BPP/RenderTexturePolygon/MASK15BPP/RenderTexturePolygonMASK15BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GPU/16BPP/RenderTextureRectangle/CLUT4BPP/RenderTextureRectangleCLUT4BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GPU/16BPP/MemoryTransfer/MemoryTransfer16BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/HelloWorld/16BPP/HelloWorld16BPP.exe", nullptr);
            //worked = grab_ROM(&ROMs, which, "PSX/GTE/GTETransfer/GTETransfer.exe", nullptr);
            break;
        case SYS_GBA:
            //worked = grab_ROM(&ROMs, which, "panda.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "armwrestler-gba-fixed.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "arm.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "thumb.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "retaddr.gba", nullptr);

            //worked = grab_ROM(&ROMs, which, "tonc/hello.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/dma_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/m3_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/pageflip.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/bm_modes.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/key_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/obj_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/brin_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/sbb_reg.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/cbb_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/obj_aff.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/sbb_aff.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/dma_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/irq_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/tmr_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/m7_demo.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/m7_demo_mb.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/m7_ex.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tonc/mos_demo.gba", nullptr);

            //worked = grab_ROM(&ROMs, which, "poke_song_player.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "sram.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "flash64.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "flash128.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "memory.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "bios.gba", nullptr);


            //worked = grab_ROM(&ROMs, which, "kirby.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "pokemon_ruby.gba", nullptr); // needs work! RTC, flash
            //worked = grab_ROM(&ROMs, which, "sonic_advance.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "sma2.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "advance_wars.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "metroid_fusion.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "doom.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "doom2.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "duke3d.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "fzero.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "mariokart.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "superstar.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "sma4.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "sma3.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "pcrysound.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "airforce_delta_storm.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "advance_guardian_heroes.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "supersonic_warriors.gba", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "dbaa.gba", nullptr); // works!

            //worked = grab_ROM(&ROMs, which, "goldensun.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "goldensun2.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "wario4.gba", nullptr);
            worked = grab_ROM(&ROMs, which, "suite.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "suite_built.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "oh my gah.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "aging_cart.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "funni.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "armfrag.gba", nullptr);

            //worked = grab_ROM(&ROMs, which, "hm_fomt.gba", nullptr); // needs work! RTC, flash
            //worked = grab_ROM(&ROMs, which, "metroid_zero.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "dual_blades.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "sonic_battle.gba", nullptr); // locks up with flash commands before battle
            //worked = grab_ROM(&ROMs, which, "gunstar.gba", nullptr); // works great
            //worked = grab_ROM(&ROMs, which, "rave_master.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "ecks_sever2.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "ecks_sever.gba", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "007_nightfire.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "mmz4.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "mmz.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "mmz3.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "drill_dozer.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "rhythm_tengoku.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "driv3r.gba", nullptr); // works great
            //worked = grab_ROM(&ROMs, which, "crazy_taxi.gba", nullptr); // plays fine, gfx issues in menus
            //worked = grab_ROM(&ROMs, which, "lunar_legend.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "tony_hawk_downhill.gba", nullptr); // works great
            //worked = grab_ROM(&ROMs, which, "car_battler_joe.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "nfs_most_wanted.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "big_mutha_truckers.gba", nullptr); // way too fast?
            //worked = grab_ROM(&ROMs, which, "kill_switch.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "ssx3.gba", nullptr); // works great
            //worked = grab_ROM(&ROMs, which, "fzero_gp_legends.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "srr.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "astro_boy.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "minish_cap.gba", nullptr);
            //worked = grab_ROM(&ROMs, which, "motoracer.gba", nullptr);

            //worked = grab_ROM(&ROMs, which, "gang-ldmstm.gba", nullptr);


            //dbg_enable_trace();

            break;
        case SYS_SNES:
            //worked = grab_ROM(&ROMs, which, "smw.sfc", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "link_to_the_past.sfc", nullptr); // works! sprite issues
            worked = grab_ROM(&ROMs, which, "super_metroid.sfc", nullptr); // nowork
            //worked = grab_ROM(&ROMs, which, "run_saber.sfc", nullptr); //
            break;
        case SYS_GENESIS_USA:
        case SYS_GENESIS_JAP:
        case SYS_MEGADRIVE_PAL:
            //dbg_enable_trace();
            //worked = grab_ROM(&ROMs, which, "sonic2.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "sonic3.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "sor3.md", nullptr);
            //worked = grab_ROM(&ROMs, which, "xmen.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "window.bin", nullptr); // works!
            worked = grab_ROM(&ROMs, which, "sonick3.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "ecco.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "ecco2.md", nullptr); // cant detect console properly
            //worked = grab_ROM(&ROMs, which, "gunstar_heroes.md", nullptr); // works fine!
            //worked = grab_ROM(&ROMs, which, "overdrive.bin", nullptr);
            //worked = grab_ROM(&ROMs, which, "dynamite_headdy.bin", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "ristar.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "castlevania_b.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "contra_hc_jp.md", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "sor2.md", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "s1built.bin", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "batman.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "gen_test_ym.bin", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "outrun2019.bin", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "junglestrike.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "battletech.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "test1536.bin", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "crusader_centy.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "xmen2.md", nullptr); // works!
            //worked = grab_ROM(&ROMs, which, "roadrash2.bin", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "roadrash.md", nullptr); // works

            //worked = grab_ROM(&ROMs, which, "240p.bin", nullptr);
            //worked = grab_ROM(&ROMs, which, "240p_emu.bin", nullptr);

            //worked = grab_ROM(&ROMs, which, "alien_storm.md", nullptr); // hangs at menu
            //worked = grab_ROM(&ROMs, which, "tyrants.md", nullptr); // :-(
            // worked = grab_ROM(&ROMs, which, "afterburner2.md", nullptr); // works!
            // worked = grab_ROM(&ROMs, which, "skitchin.md", nullptr); // works
            //worked = grab_ROM(&ROMs, which, "haunting.md", nullptr); // works
            //worked = grab_ROM(&ROMs, which, "herzog_zwei.md", nullptr); // works
            //worked = grab_ROM(&ROMs, which, "desert_strike.md", nullptr);
            //worked = grab_ROM(&ROMs, which, "flashback.md", nullptr); // 6-button doesn't work?
            //worked = grab_ROM(&ROMs, which, "rocket_knight.md", nullptr); // works

            //worked = grab_ROM(&ROMs, which, "vectorman.md", nullptr); // well!
            //worked = grab_ROM(&ROMs, which, "vectorman2.md", nullptr); // well!
            // worked = grab_ROM(&ROMs, which, "golden_axe3.bin", nullptr); // fine!
            //worked = grab_ROM(&ROMs, which, "golden_axe2.md", nullptr); // fine
            //worked = grab_ROM(&ROMs, which, "golden_axe.md", nullptr); // fine
            //worked = grab_ROM(&ROMs, which, "kid_chameleon.md", nullptr); // fine
            //worked = grab_ROM(&ROMs, which, "kawasaki_superbike.md", nullptr); // (less)gfx corruption!
            //worked = grab_ROM(&ROMs, which, "panorama_cotton.bin", nullptr); // seems fine
            //worked = grab_ROM(&ROMs, which, "shadow_run.md", nullptr); // works fine!
            //worked = grab_ROM(&ROMs, which, "mega_man_wiley_wars.md", nullptr); // works fine!
            //worked = grab_ROM(&ROMs, which, "lost_vikings.md", nullptr); // does nothing
            //worked = grab_ROM(&ROMs, which, "shining_force_2.md", nullptr); // works fine!
            // worked = grab_ROM(&ROMs, which, "shining_force.md", nullptr); // works!
            // worked = grab_ROM(&ROMs, which, "battletoads.md", nullptr); // fine!
            //worked = grab_ROM(&ROMs, which, "comix_zone.md", nullptr); // works!


            //worked = grab_ROM(&ROMs, which, "megaturrican.md", nullptr); // title screen bottom row funky. turrican fly-in oddly slow? plays fine
            //worked = grab_ROM(&ROMs, which, "mortalkombat3.md", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "gauntlet4.md", nullptr); // runs unsteadily?
            //worked = grab_ROM(&ROMs, which, "tmnt_hyperstone.md", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "zero_squirrel.md", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "zero_tolerance.md", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "zero_tolerance_2.md", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "devilish.md", nullptr); // slight gfx corruption, seems ifne to play
            //worked = grab_ROM(&ROMs, which, "toy_story.md", nullptr); // works great
            //worked = grab_ROM(&ROMs, which, "rbi_baseball_3.md", nullptr); // works fine
            //worked = grab_ROM(&ROMs, which, "rampart.md", nullptr); // no display
            //worked = grab_ROM(&ROMs, which, "ranger_x.md", nullptr); // works great
            //worked = grab_ROM(&ROMs, which, "red_zone.md", nullptr); // works great


            //worked = grab_ROM(&ROMs, which, "wonder_boy4.md", nullptr); // seems fine
            //worked = grab_ROM(&ROMs, which, "duke3d.bin", nullptr); // issues!
            //worked = grab_ROM(&ROMs, which, "star_cruiser.bin", nullptr); // seems fine? hard to say
            //worked = grab_ROM(&ROMs, which, "sonic_spinball.md", nullptr); // same sprite priority issue
            //worked = grab_ROM(&ROMs, which, "sonic_3d_blast.md", nullptr); // FMV vibrates up and down. otherwise works well
            //worked = grab_ROM(&ROMs, which, "blockout.md", nullptr); // works well
            //worked = grab_ROM(&ROMs, which, "street_fighter_2_special_championship.md", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "sprite_masking_test.bin", nullptr); // works great!
            //worked = grab_ROM(&ROMs, which, "direct_color_dma.bin", nullptr); // does nothing
            //worked = grab_ROM(&ROMs, which, "alien_soldier_jp.md", nullptr); // works well
            //worked = grab_ROM(&ROMs, which, "alien_soldier_usa.md", nullptr);
            //worked = grab_ROM(&ROMs, which, "beyond_oasis.md", nullptr);
            //worked = grab_ROM(&ROMs, which, "granada.md", nullptr);

            break;
        default:
            printf("\nSYS NOT IMPLEMENTED!");
    }
    if (!worked) {
        printf("\nCouldn't open ROM!");
        return;
    }

    if (which == SYS_PS1) {
        sys->sideload(sys, &ROMs);
    }
    else {
        struct physical_io_device *fileioport = load_ROM_into_emu(sys, IOs, &ROMs);
        if (fileioport != NULL) {
            printf("\nSRAM requested size: %lld\n", fileioport->cartridge_port.SRAM.requested_size);
            if (fileioport->cartridge_port.SRAM.requested_size > 0) {
                setup_persistent_store(&fileioport->cartridge_port.SRAM, &ROMs);
            }
        }
    }
}

void full_system::add_waveform_view(u32 idx)
{
    auto *dview = (struct debugger_view *)cvec_get(&dbgr.views, idx);
    struct WVIEW myv;
    myv.view = &dview->waveform;
    auto *wv = (struct waveform_view *)&dview->waveform;
    for (u32 i = 0; i < cvec_len(&wv->waveforms); i++) {
        WFORM wf;
        wf.enabled = true;
        wf.height = 80;
        wf.wf = (struct debug_waveform *)cvec_get(&wv->waveforms, i);
        myv.waveforms.push_back(wf);
    }

    waveform_views.push_back(myv);
}

void full_system::add_console_view(u32 idx)
{
    auto *dview = (struct debugger_view *)cvec_get(&dbgr.views, idx);
    CVIEW myv;
    myv.view = dview;
    console_views.push_back(myv);
}


void full_system::add_trace_view(u32 idx)
{
    auto *dview = (struct debugger_view *)cvec_get(&dbgr.views, idx);
    TVIEW myv;
    myv.view = dview;
    trace_views.push_back(myv);
}

void full_system::add_dbglog_view(u32 idx)
{
    auto *dview = (struct debugger_view *)cvec_get(&dbgr.views, idx);
    DLVIEW myv;
    myv.view = dview;
    dlviews.push_back(myv);
}


void full_system::add_disassembly_view(u32 idx)
{
    auto *dview = (struct debugger_view *)cvec_get(&dbgr.views, idx);
    //printf("\nAdding disassembly view %s:", dview->disassembly.processor_name.ptr);
    DVIEW myv;
    myv.view = dview;
    cvec_init(&myv.dasm_rows, sizeof(struct disassembly_entry_strings), 150);
    for (u32 i = 0; i < 200; i++) {
        auto *das = (struct disassembly_entry_strings *)cvec_push_back(&myv.dasm_rows);
        memset(das, 0, sizeof(*das));
    }

    dasm_views.push_back(myv);
}

void full_system::add_image_view(u32 idx)
{
    auto *dview = (struct debugger_view *)cvec_get(&dbgr.views, idx);
    IVIEW myv;
    myv.enabled = true;
    myv.view = dview;
    images.push_back(myv);
    //printf("\nAdding image view %s: width %d", myv.view->image.label, myv.view->image.width);
}

void full_system::setup_debugger_interface()
{
    sys->setup_debugger_interface(sys, &dbgr);
    for (u32 i = 0; i < cvec_len(&dbgr.views); i++) {
        auto view = (struct debugger_view *)cvec_get(&dbgr.views, i);
        switch(view->kind) {
            case dview_disassembly:
                add_disassembly_view(i);
                break;
            case dview_events:
                events.view = &view->events;
                break;
            case dview_trace:
                add_trace_view(i);
                break;
            case dview_dbglog:
                add_dbglog_view(i);
                break;
            case dview_console:
                add_console_view(i);
                break;
            case dview_image:
                add_image_view(i);
                break;
            case dview_waveforms:
                add_waveform_view(i);
                break;
            default:
                assert(1==2);
        }
    }
}


void full_system::setup_system(enum jsm_systems which)
{
    // Create our emulator
    sys = new_system(which);

    assert(sys);


    setup_ios();

    setup_bios();

    //    backbuffer.setup(wgpu_device, "backbuffer", bb_width, bb_height);

    load_default_ROM();

#ifdef SIDELOAD
    struct multi_file_set sideload_image;
    mfs_init(&sideload_image);
    grab_ROM(&sideload_image, which, "gl_matrix.elf", "kos");
    mfs_add_IP_BIN(&sideload_image);
    fsys.sys->sideload(sys, &sideload_image);
    mfs_delete(&sideload_image);
#endif

    setup_audio();

    setup_debugger_interface();
    sys->reset(sys);
}

void full_system::update_touch(i32 x, i32 y, i32 button_down)
{
    if (io.touchscreen.vec) {
        struct physical_io_device *pio = (struct physical_io_device *)cpg(io.touchscreen);
        struct JSM_TOUCHSCREEN *ts = &pio->touchscreen;
        x += ts->params.x_offset;
        y += ts->params.y_offset;
        u32 in_screen =  (x >= 0) && (x < ts->params.width) && (y >= 0) && (y < ts->params.height);
        ts->touch.down = in_screen && button_down;
        if (in_screen) {
            ts->touch.x = x;
            ts->touch.y = y;
        }
    }
}

void full_system::get_savestate_filename(char *pth, size_t sz)
{
    struct read_file_buf *rfb = &ROMs.files[0];
    snprintf(pth, sz, "%s/%s.save", rfb->path, rfb->name);
    printf("\nSAVE NAME: %s", pth);
}


void full_system::save_state() {
    char PATH[500];
    PATH[0] = 0;
    if (sys->save_state) {
        printf("\nSaving...");
        struct serialized_state state;
        serialized_state_init(&state);
        sys->save_state(sys, &state);

        get_savestate_filename(PATH, sizeof(PATH));
        FILE *f = fopen(PATH, "w");
        serialized_state_write_to_file(&state, f);
        fclose(f);

        serialized_state_delete(&state);
    }
    else {
        printf("\nSaves not defined for system!");
    }
}

void full_system::load_state() {
    char PATH[500];
    PATH[0] = 0;
    if (sys->load_state) {
        printf("\nLoading...");
        struct serialized_state state;
        serialized_state_init(&state);
        struct deserialize_ret ret;
        memset(&ret, 0, sizeof(ret));

        get_savestate_filename(PATH, sizeof(PATH));
        FILE *f = fopen(PATH, "r");
        if (f == NULL) {
            printf("\nFILE NOT FOUND!");
        }
        else {
            fseek(f, 0, SEEK_END);
            u64 sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            serialized_state_read_from_file(&state, f, sz);
            fclose(f);

            sys->load_state(sys, &state, &ret);
        }
        serialized_state_delete(&state);
    }
    else {
        printf("\nSaves not defined for system!");
    }

}

void full_system::destroy_system()
{
    if (sys == nullptr) return;
    jsm_delete(sys);
    sys = nullptr;
}

struct framevars full_system::get_framevars() const
{
    struct framevars fv = {};
    sys->get_framevars(sys, &fv);
    return fv;
}

#define SCALE_DOWN 1

void full_system::setup_display()
{
    struct JSM_DISPLAY_PIXELOMETRY *p = &output.display->pixelometry;

    // Determine final output resolution
    u32 wh = get_closest_pow2(MAX(p->cols.max_visible, p->rows.max_visible));
    TS(output.backbuffer_texture,"emulator backbuffer", wh, wh);
    //printf("\nMAX COLS:%d ROWS:%d POW2:%d", p->cols.max_visible, p->rows.max_visible, wh);

    u32 overscan_x_offset = p->overscan.left;
    u32 overscan_width = p->cols.visible - (p->overscan.left + p->overscan.right);
    u32 overscan_y_offset = p->overscan.top;
    u32 overscan_height = p->rows.visible - (p->overscan.top + p->overscan.bottom);

    // Determine aspect ratio correction
    double visible_width = p->cols.visible;
    double visible_height = p->rows.visible;

    double real_width = output.display->geometry.physical_aspect_ratio.width;
    double real_height = output.display->geometry.physical_aspect_ratio.height;

    // we want a multiplier of 1 in one direction, and >1 in the other
    double visible_how = visible_height / visible_width;  // .5
    double real_how = real_height / real_width;           // .6. real is narrower

    if (fabs(visible_how - real_how) < .01) {
        output.x_scale_mult = 1;
        output.y_scale_mult = 1;
        output.with_overscan.x_size = (float)p->cols.visible;
        output.with_overscan.y_size = (float)p->rows.visible;
        output.without_overscan.x_size = (float)overscan_width;
        output.without_overscan.y_size = (float)overscan_height;
    }
    else if (!SCALE_DOWN && (real_how > visible_how)) { // real is narrower, so we stretch vertically. visible= 4:2 .5  real=3:2  .6
        output.x_scale_mult = 1;
        output.y_scale_mult = (real_how / visible_how); // we must
        output.with_overscan.x_size = (float)p->cols.visible;
        output.with_overscan.y_size = (float)(visible_height * output.y_scale_mult);
        output.without_overscan.x_size = (float)overscan_width;
        output.without_overscan.y_size = (float)(overscan_height * output.y_scale_mult);
    }
    else { // real is wider, so we stretch horizontally  //    visible=4:2 = .5    real=5:2 = .4
        printf("\nNo stretch that way...");
        output.x_scale_mult = (visible_how / real_how);
        output.y_scale_mult = 1;
        output.with_overscan.x_size = (float)(visible_width * output.x_scale_mult);
        output.with_overscan.y_size = (float)p->rows.visible;
        output.without_overscan.x_size = (float)(overscan_width * output.x_scale_mult);
        output.without_overscan.y_size = (float)overscan_height;
    }
    printf("\nOutput with overscan size: %dx%d", (int)output.with_overscan.x_size, (int)output.with_overscan.y_size);
    printf("\nOutput without overscan size: %dx%d", (int)output.with_overscan.x_size, (int)output.with_overscan.y_size);

    // Calculate UV coords for full buffer
    output.with_overscan.uv0 = ImVec2(0, 0);
    output.with_overscan.uv1 = ImVec2((float)((double)p->cols.visible / (double)output.backbuffer_texture.width),
                                      (float)((double)p->rows.visible / (double)output.backbuffer_texture.height));

    // Calculate UV coords for buffer minus overscan
    // we need the left and top, which may be 0 or 10 or whatever... % of total width
    float total_u = output.with_overscan.uv1.x;
    float total_v = output.with_overscan.uv1.y;

    float start_u = (float)overscan_x_offset / (float)visible_width;
    float start_v = (float)overscan_y_offset / (float)visible_height;
    output.without_overscan.uv0 = ImVec2(start_u * total_u, start_v * total_v);

    float end_u = (float)(p->cols.visible - p->overscan.right) / (float)visible_width;
    float end_v = (float)(p->rows.visible - p->overscan.bottom) / (float)visible_height;
    output.without_overscan.uv1 = ImVec2(end_u * total_u, end_v * total_v);

    if (output.backbuffer_backer) free(output.backbuffer_backer);
    output.backbuffer_backer = malloc(output.backbuffer_texture.width*output.backbuffer_texture.height * 4);
    memset(output.backbuffer_backer, 0, output.backbuffer_texture.width*output.backbuffer_texture.height * 4);
}

void full_system::present()
{
    struct framevars fv = {};
    sys->get_framevars(sys, &fv);
    jsm_present(sys->kind, (struct physical_io_device *)cpg(io.display), output.backbuffer_backer, 0, 0, output.backbuffer_texture.width, output.backbuffer_texture.height, 0);
    output.backbuffer_texture.upload_data(output.backbuffer_backer, output.backbuffer_texture.width * output.backbuffer_texture.height * 4, output.backbuffer_texture.width, output.backbuffer_texture.height);
}

void full_system::pre_events_view_present()
{
    if (events.view) {
        if (!events.texture.is_good) {
            u32 szpo2 = get_closest_pow2(MAX(events.view->display[0].width, events.view->display[0].height));
            TS(events.texture, "Events View texture", szpo2, szpo2);
            events.texture.uv0 = ImVec2(0, 0);
            events.texture.uv1 = ImVec2(
                    (float) ((double) events.view->display[0].width / (double) events.texture.width),
                    (float) ((double) events.view->display[0].height / (double) events.texture.height));
            events.texture.sz_for_display = ImVec2((float) events.view->display[0].width,
                                                   (float) events.view->display[0].height);
            events.view->display[0].buf = (u32 *) malloc(szpo2 * szpo2 * 4);
            events.view->display[1].buf = (u32 *) malloc(szpo2 * szpo2 * 4);
            // Setup UVs based off it
        }
    }
}

void full_system::events_view_present()
{
    if (events.view) {
        pre_events_view_present();
        assert(events.view->index_in_use<2);
        struct events_view::DVDP *evd = &events.view->display[events.view->index_in_use];

        struct framevars fv = {};
        sys->get_framevars(sys, &fv);
        struct JSM_DISPLAY *d = &((struct physical_io_device *) cpg(io.display))->display;
        memset(evd->buf, 0, events.texture.width*events.texture.height*4);
        jsm_present(sys->kind, (struct physical_io_device *)cpg(io.display), evd->buf, d->pixelometry.offset.x, d->pixelometry.offset.y, events.texture.width, events.texture.height, 1);
        events_view_render(&dbgr, events.view, evd->buf, events.texture.width, events.texture.height);

        events.texture.upload_data(evd->buf, events.texture.width*events.texture.height*4, events.texture.width, events.texture.height);
    }

}

static void draw_box(u32 *ptr, u32 x0, u32 y0, u32 x1, u32 y1, u32 out_width, u32 out_height, u32 color)
{
    u32 stride = out_width;
    u32 *left_ptr = ptr + (y0 * out_width) + x0;
    u32 *right_ptr = ptr + (y0 * out_width) + x1;
    for (u32 y = y0; y < y1; y++) {
        *left_ptr = color;
        *right_ptr = color;
        left_ptr += stride;
        right_ptr += stride;
    }

    u32 *top_ptr = ptr + (y0 * out_width) + x0;
    u32 *bottom_ptr = ptr + (y1 * out_width) + x0;
    for (u32 x = x0; x < x1; x++) {
        *top_ptr = color;
        *bottom_ptr = color;
        top_ptr++;
        bottom_ptr++;
    }
}

void full_system::waveform_view_present(struct WVIEW &wv)
{
    for (auto& wf : wv.waveforms) {
        if (!wf.tex.is_good) {
            u32 szpo2 = 1024;
            TS(wf.tex, wf.wf->name, szpo2, szpo2);
            assert(wf.tex.is_good);
            wf.tex.uv0 = ImVec2(0, 0);
            wf.drawbuf.resize(1024*1024*4);
        }

        memset(wf.drawbuf.data(), 0, 1024*1024*4);
        // Draw box around

        float hrange = wf.height / 2.0f;
        u32 *ptr = (u32 *)(wf.drawbuf.data());
        draw_box(ptr, 0, 0, wf.wf->samples_requested-1, wf.height-1, 1024, 1024, 0xFF808080);
        i32 last_y = 0;
        if (wf.wf->samples_rendered > 0) {
            float *b = (float *)wf.wf->buf.ptr;
            for (u32 x = 0; x < wf.wf->samples_rendered; x++) {
                float smp = *b;
                float fy = (hrange * smp) * -1.0f;
                i32 iy = ((i32)floor(fy)) + (i32)hrange;
                if (x != 0) {
                    u32 starty = iy < last_y ? iy : last_y;
                    u32 endy = iy > last_y ? iy : last_y;
                    for (u32 sy = starty; sy <= endy; sy++) {
                        ptr[(sy * 1024) + x] = 0xFFFFFFFF;
                    }
                }
                ptr[(iy * 1024) + x] = 0xFFFFFFFF;
                last_y = iy;
                b++;
            }
        }
        wf.tex.upload_data(wf.drawbuf.data(), 1024*1024*4, 1024, 1024);
        wf.tex.uv1 = ImVec2((float)wf.wf->samples_rendered / 1024.0f, (float)wf.height / 1024.0f);
        wf.tex.sz_for_display = ImVec2(wf.wf->samples_rendered, wf.height);
    }
}

void full_system::image_view_present(struct debugger_view *dview, struct my_texture &tex)
{
    struct image_view *iview = &dview->image;
    if (!tex.is_good) {
        u32 szpo2 = get_closest_pow2(MAX(iview->height, iview->width));
        TS(tex, iview->label, szpo2, szpo2);
        assert(tex.is_good);
        tex.uv0 = ImVec2(0, 0);
        tex.uv1 = ImVec2((float)((double)iview->width / (double)szpo2),
                                    (float)((double)iview->height / (double)szpo2));

        tex.sz_for_display = ImVec2((float)iview->width, (float)iview->height);
        buf_allocate(&iview->img_buf[0], szpo2*szpo2*4);
        buf_allocate(&iview->img_buf[1], szpo2*szpo2*4);
        memset(iview->img_buf[0].ptr, 0, szpo2*szpo2*4);
        memset(iview->img_buf[1].ptr, 0, szpo2*szpo2*4);
    }

    iview->update_func.func(&dbgr, dview, iview->update_func.ptr, tex.width);
    void *buf = iview->img_buf[iview->draw_which_buf].ptr;
    assert(buf);

    tex.upload_data(buf, tex.width*tex.height*4, tex.width, tex.height);
}

void full_system::step_frames(int num)
{
    for (u32 i = 0; i < num; i++) {
        sys->finish_frame(sys);
        if (dbg.do_break) break;
    }
}

void full_system::step_seconds(int num)
{
    printf("\nNOT SUPPORT YET");
}

void full_system::step_scanlines(int num)
{
    sync_persistent_storage();
    for (u32 i = 0; i < num; i++) {
        if (dbg.do_break) break;
        sys->finish_scanline(sys);
    }
}

void full_system::step_cycles(int num)
{
    framevars fv{};
    sync_persistent_storage();
    if (dbg.do_break) return;
    sys->get_framevars(sys, &fv);
    u64 cur_frame = fv.master_frame;
    sys->step_master(sys, num);
    sys->get_framevars(sys, &fv);
    u64 new_frame = fv.master_frame;
    if (cur_frame != new_frame) {//
        // frame_advanced();
    }
}

ImVec2 full_system::output_size() const
{
    float z = output.zoom ? 2.0f : 1.0f;
    auto &v = output.hide_overscan ? output.without_overscan : output.with_overscan;
    return {v.x_size * z, v.y_size * z};
}

ImVec2 full_system::output_uv0() const
{
    auto &v = output.hide_overscan ? output.without_overscan.uv0 : output.with_overscan.uv0;
    return v;
}

ImVec2 full_system::output_uv1() const
{
    auto &v = output.hide_overscan ? output.without_overscan.uv1 : output.with_overscan.uv1;
    return v;
}

void full_system::debugger_pre_frame_waveforms(struct waveform_view *wv)
{

}

void full_system::debugger_pre_frame() {
    for (auto &wview : waveform_views) {
        for (auto &dwe: wview.waveforms) {
            struct debug_waveform *dw = dwe.wf;
            switch (dw->kind) {
                case dwk_none:
                    assert(1 == 2);
                    break;
                case dwk_main:
                    dw->samples_requested = 400;
                    break;
                case dwk_channel:
                    dw->samples_requested = 200;
                    break;
                default:
                    NOGOHERE;
            }
        }
    }
}

void full_system::do_frame() {
    if (sys) {
        struct audiobuf *b = audio.get_buf_for_emu();

        struct framevars fv = {};
        if (!dbg.do_break) {
            if (b && sys->set_audiobuf) sys->set_audiobuf(sys, b);
            debugger_pre_frame();
            sys->finish_frame(sys);
            if (b && sys->set_audiobuf) audio.commit_emu_buffer();
        }
        sys->get_framevars(sys, &fv);
        //dbg_flush();
        sync_persistent_storage();
        //TODO: here
    }
    else {
        printf("\nCannot do frame with no system.");
    }
}

void full_system::setup_tracing()
{
    dbg.traces.ps1.sio0.irq = 0;
    dbg.traces.ps1.sio0.rw = 0;
    dbg.traces.ps1.sio0.ack = 0;
    dbg.traces.ps1.pad = 0;
    //dbg_enable_trace();
}
