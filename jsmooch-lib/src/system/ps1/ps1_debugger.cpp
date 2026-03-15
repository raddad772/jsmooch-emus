//
// Created by . on 2/16/25.
//
#include "helpers/multisize_memaccess.cpp"
#include "helpers/color.h"
#include "ps1_debugger.h"
#include "ps1_bus.h"

namespace PS1 {

static void render_image_view_spuinfo(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    //memset(ptr, 0, out_width * 4 * 10);
    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
    static constexpr char EPNAMES[4][50] = {"Attack ", "Decay  ", "Sustain", "Release"};
    for (u32 i = 0; i < 24; i++) {
        auto &v = th->spu.voices[i];

        tb->sprintf("\nV%d  env.vol:%04x   env.phase:%s",  i, v.env.adsr.output, EPNAMES[v.env.phase]);
    }
}

static void render_image_view_sysinfo(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    //memset(ptr, 0, out_width * 4 * 10);
    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
    tb->sprintf("\nI_STAT:%08x\nEnabled IRQs:", th->cpu.io.I_STAT);
    for (u32 i = 0; i < 11; i++) {
        u32 b = 1 << i;
        if (th->cpu.io.I_MASK & b) tb->sprintf("\n  %s", IRQnames[i]);
    }

    tb->sprintf("\n\nDMA channels. Master enable:%08x");
    static constexpr char DMANAMES[7][50] = {"MDEC in ", "MDEC out", "GPU     ", "CDROM   ", "SPU     ", "PIO     ", "OTC     "};
    static constexpr char SYNCS[4][50] = {"manual ", "request", "list   ", "invalid"};
    for (u32 i = 0; i < 7; i++) {
        auto &ch = th->dma.channels[i];
        tb->sprintf("\n  CH%d (%s)   m/enable:%d/%d  sync:%s  block_count:%d block_size:%x", i, DMANAMES[i], ch.master_enable, ch.enable, SYNCS[ch.sync], ch.block_count, ch.block_size);
    }

    tb->sprintf("\n\nTimers:");
    for (u32 i = 0; i < 3; i++) {
        auto &t = th->timers[i];
        tb->sprintf("\n. TMR%d  sync:%d/%d  reset:%d  clock_source:%d. irq_on_target:%d  irq_on_ffff:%d  target:%04x",
            i, t.mode.sync_enable, t.mode.sync_mode, t.mode.reset_when, t.mode.clock_source, t.mode.irq_on_target, t.mode.irq_on_ffff, t.target);
    }
}


static void render_image_view_vram(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, 1024*512*4);

    u8 *a = th->gpu.VRAM;
    u16 *gbao = reinterpret_cast<u16 *>(a);
    u32 *img32 = outbuf;
    i32 mx = iv->mouse_x - 2;
    i32 my = iv->mouse_y - 2;

    for (u32 ry = 0; ry < 512; ry++) {
        u32 y = ry;
        u32 *line_out_ptr = img32 + (ry * out_width);
        for (u32 rx = 0; rx < 1024; rx++) {
            u32 x = rx;
            u32 di = ((y * 1024) + x);

            u32 color = ps1_to_screen(gbao[di]);
            if (rx == mx && ry == my) {
                color ^= 0xFFFFFF;
            }
            *line_out_ptr = color;
            line_out_ptr++;
        }
    }
    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
    tb->sprintf("COORD %d,%d  ", mx, my);
    if ((mx >= 0) && (mx <= 1023) && (my >= 0) && (my <= 511)) {
        tb->sprintf("CMD %02x", th->gpu.dbg.cmdbuf[(my * 1024) + iv->mouse_x]);
    }
}

void setup_console_view(core* th, debugger_interface *dbgr)
{
    th->dbg.console_view = dbgr->make_view(dview_console);
    debugger_view *dview = &th->dbg.console_view.get();

    console_view *cv = &dview->console;

    snprintf(cv->name, sizeof(cv->name), "System Console");

    th->cpu.dbg.console = cv;
}

static void kernel_call(void *ptr) {
    auto *th = static_cast<core *>(ptr);
    th->kernel_call_hook();
}

/*
 A(02h) or B(34h) read(fd,dst,length)
  A(03h) or B(35h) write(fd,src,length)
  A(04h) or B(36h) close(fd)
  A(05h) or B(37h) ioctl(fd,cmd,arg)
  A(06h) or B(38h) exit(exitcode)
  A(07h) or B(39h) isatty(fd)
  A(08h) or B(3Ah) getc(fd)
  A(09h) or B(3Bh) putc(char,fd)
  A(0Ah) todigit(char)
  A(0Bh) atof(src)     ;Does NOT work - uses (ABSENT) cop1 !!!
  A(0Ch) strtoul(src,src_end,base)
  A(0Dh) strtol(src,src_end,base)
  A(0Eh) abs(val)
  A(0Fh) labs(val)
  A(10h) atoi(src)
  A(11h) atol(src)
  A(12h) atob(src,num_dst)
  A(13h) setjmp(buf)
  A(14h) longjmp(buf,param)
  A(15h) strcat(dst,src)
  A(16h) strncat(dst,src,maxlen)
  A(17h) strcmp(str1,str2)
  A(18h) strncmp(str1,str2,maxlen)
  A(19h) strcpy(dst,src)
  A(1Ah) strncpy(dst,src,maxlen)
  A(1Bh) strlen(src)
  A(1Ch) index(src,char)
  A(1Dh) rindex(src,char)
  A(1Eh) strchr(src,char)  ;exactly the same as "index"
  A(1Fh) strrchr(src,char) ;exactly the same as "rindex"
  A(20h) strpbrk(src,list)
  A(21h) strspn(src,list)
  A(22h) strcspn(src,list)
  A(23h) strtok(src,list)  ;use strtok(0,list) in further calls
  A(24h) strstr(str,substr)       ;Bugged
  A(25h) toupper(char)
  A(26h) tolower(char)
  A(27h) bcopy(src,dst,len)
  A(28h) bzero(dst,len)
  A(29h) bcmp(ptr1,ptr2,len)      ;Bugged
  A(2Ah) memcpy(dst,src,len)
  A(2Bh) memset(dst,fillbyte,len)
  A(2Ch) memmove(dst,src,len)     ;Bugged
  A(2Dh) memcmp(src1,src2,len)    ;Bugged
  A(2Eh) memchr(src,scanbyte,len)
  A(2Fh) rand()
  A(30h) srand(seed)
  A(31h) qsort(base,nel,width,callback)
  A(32h) strtod(src,src_end) ;Does NOT work - uses (ABSENT) cop1 !!!
  A(33h) malloc(size)
  A(34h) free(buf)
  A(35h) lsearch(key,base,nel,width,callback)
  A(36h) bsearch(key,base,nel,width,callback)
  A(37h) calloc(sizx,sizy)            ;SLOW!
  A(38h) realloc(old_buf,new_siz)     ;SLOW!
  A(39h) InitHeap(addr,size)
  A(3Ah) _exit(exitcode)
  A(3Bh) or B(3Ch) getchar()
  A(3Ch) or B(3Dh) putchar(char)
  A(3Dh) or B(3Eh) gets(dst)
  A(3Eh) or B(3Fh) puts(src)
  A(3Fh) printf(txt,param1,param2,etc.)
  A(40h) SystemErrorUnresolvedException()
  A(41h) LoadTest(filename,headerbuf)
  A(42h) Load(filename,headerbuf)
  A(43h) Exec(headerbuf,param1,param2)
  A(44h) FlushCache()
  A(45h) init_a0_b0_c0_vectors
  A(46h) GPU_dw(Xdst,Ydst,Xsiz,Ysiz,src)
  A(47h) gpu_send_dma(Xdst,Ydst,Xsiz,Ysiz,src)
  A(48h) SendGP1Command(gp1cmd)
  A(49h) GPU_cw(gp0cmd)   ;send GP0 command word
  A(4Ah) GPU_cwp(src,num) ;send GP0 command word and parameter words
  A(4Bh) send_gpu_linked_list(src)
  A(4Ch) gpu_abort_dma()
  A(4Dh) GetGPUStatus()
  A(4Eh) gpu_sync()
  A(4Fh) SystemError
  A(50h) SystemError
  A(51h) LoadExec(filename,stackbase,stackoffset)
  A(52h) GetSysSp
  A(53h) SystemError           ;PS2: set_ioabort_handler(src)
  A(54h) or A(71h) _96_init()
  A(55h) or A(70h) _bu_init()
  A(56h) or A(72h) _96_remove()  ;does NOT work due to SysDeqIntRP bug
  A(57h) return 0
  A(58h) return 0
  A(59h) return 0
  A(5Ah) return 0
  A(5Bh) dev_tty_init()                                      ;PS2: SystemError
  A(5Ch) dev_tty_open(fcb,and unused:"path\name",accessmode) ;PS2: SystemError
  A(5Dh) dev_tty_in_out(fcb,cmd)                             ;PS2: SystemError
  A(5Eh) dev_tty_ioctl(fcb,cmd,arg)                          ;PS2: SystemError
  A(5Fh) dev_cd_open(fcb,"path\name",accessmode)
  A(60h) dev_cd_read(fcb,dst,len)
  A(61h) dev_cd_close(fcb)
  A(62h) dev_cd_firstfile(fcb,"path\name",direntry)
  A(63h) dev_cd_nextfile(fcb,direntry)
  A(64h) dev_cd_chdir(fcb,"path")
  A(65h) dev_card_open(fcb,"path\name",accessmode)
  A(66h) dev_card_read(fcb,dst,len)
  A(67h) dev_card_write(fcb,src,len)
  A(68h) dev_card_close(fcb)
  A(69h) dev_card_firstfile(fcb,"path\name",direntry)
  A(6Ah) dev_card_nextfile(fcb,direntry)
  A(6Bh) dev_card_erase(fcb,"path\name")
  A(6Ch) dev_card_undelete(fcb,"path\name")
  A(6Dh) dev_card_format(fcb)
  A(6Eh) dev_card_rename(fcb1,"path\name1",fcb2,"path\name2")
  A(6Fh) ?   ;card ;[r4+18h]=00000000h  ;card_clear_error(fcb) or so
  A(70h) or A(55h) _bu_init()
  A(71h) or A(54h) _96_init()
  A(72h) or A(56h) _96_remove()   ;does NOT work due to SysDeqIntRP bug
  A(73h) return 0
  A(74h) return 0
  A(75h) return 0
  A(76h) return 0
  A(77h) return 0
  A(78h) CdAsyncSeekL(src)
  A(79h) return 0               ;DTL-H: Unknown?
  A(7Ah) return 0               ;DTL-H: Unknown?
  A(7Bh) return 0               ;DTL-H: Unknown?
  A(7Ch) CdAsyncGetStatus(dst)
  A(7Dh) return 0               ;DTL-H: Unknown?
  A(7Eh) CdAsyncReadSector(count,dst,mode)
  A(7Fh) return 0               ;DTL-H: Unknown?
  A(80h) return 0               ;DTL-H: Unknown?
  A(81h) CdAsyncSetMode(mode)
  A(82h) return 0               ;DTL-H: Unknown?
  A(83h) return 0               ;DTL-H: Unknown?
  A(84h) return 0               ;DTL-H: Unknown?
  A(85h) return 0               ;DTL-H: Unknown?, or reportedly, CdStop (?)
  A(86h) return 0               ;DTL-H: Unknown?
  A(87h) return 0               ;DTL-H: Unknown?
  A(88h) return 0               ;DTL-H: Unknown?
  A(89h) return 0               ;DTL-H: Unknown?
  A(8Ah) return 0               ;DTL-H: Unknown?
  A(8Bh) return 0               ;DTL-H: Unknown?
  A(8Ch) return 0               ;DTL-H: Unknown?
  A(8Dh) return 0               ;DTL-H: Unknown?
  A(8Eh) return 0               ;DTL-H: Unknown?
  A(8Fh) return 0               ;DTL-H: Unknown?
  A(90h) CdromIoIrqFunc1()
  A(91h) CdromDmaIrqFunc1()
  A(92h) CdromIoIrqFunc2()
  A(93h) CdromDmaIrqFunc2()
  A(94h) CdromGetInt5errCode(dst1,dst2)
  A(95h) CdInitSubFunc()
  A(96h) AddCDROMDevice()
  A(97h) AddMemCardDevice()     ;DTL-H: SystemError
  A(98h) AddDuartTtyDevice()    ;DTL-H: AddAdconsTtyDevice ;PS2: SystemError
  A(99h) add_nullcon_driver()
  A(9Ah) SystemError            ;DTL-H: AddMessageWindowDevice
  A(9Bh) SystemError            ;DTL-H: AddCdromSimDevice
  A(9Ch) SetConf(num_EvCB,num_TCB,stacktop)
  A(9Dh) GetConf(num_EvCB_dst,num_TCB_dst,stacktop_dst)
  A(9Eh) SetCdromIrqAutoAbort(type,flag)
  A(9Fh) SetMem(megabytes)
  */

void core::kernel_call_hook() {
    if (!dbg.dvptr->ids_enabled[PS1D_GENERAL_KERNAL_FUNCS]) return;
    u32 PC = cpu.regs.PC & 0x1FFFFFFF;
    u32 r9 = cpu.regs.R[9];
    char bstr[500];
    char *ptr = bstr;
    *ptr = 0;
    if (r9 > 0x100) {
        ptr += snprintf(ptr, sizeof(bstr) - (ptr - bstr), "Invalid %02x call r9:%02x: ", PC, r9);
    }
    else     if ((PC != 0xA0) && (PC != 0xB0) && (PC != 0xC0)) {
        ptr += snprintf(ptr, sizeof(bstr) - (ptr - bstr), "Kernal call hook unknown PC %08x", PC);
    }
    else {
#define HA(na, ...) case 0xA##na: ptr += snprintf(ptr, sizeof(bstr) - (ptr - bstr), __VA_ARGS__); break
#define HB(nb, ...)      case 0xB##nb: ptr += snprintf(ptr, sizeof(bstr) - (ptr - bstr), __VA_ARGS__); break
#define HAB(na, nb, ...) case 0xA##na: case 0xB##nb: ptr += snprintf(ptr, sizeof(bstr) - (ptr - bstr), __VA_ARGS__); break
#define HAC(nc, ...) case 0xC##nc: ptr += snprintf(ptr, sizeof(bstr) - (ptr - bstr), __VA_ARGS__); break
        PC = (PC << 4) | r9;
        switch (PC) {
            HAB(00, 32, "open(filename, accessmode)");
            HAB(01, 33, "lseek(fd, offset, seektype)");
            HAB(02, 34, "read(fd, dst, length)");
            HAB(03, 35, "write(fd, src, length)");
            HAB(04, 36, "close(fd)");
            HAB(05, 37, "ioctl(fd, cmd, arg)");
            HAB(06, 38, "exit(exitcode)");
            HAB(07, 39, "isatty(fd)");
            HAB(08, 3A, "getc(fd)");
            HAB(09, 3B, "putc(char, fd)");
            HA(0A, "todigit(char)");
            HA(0B, "atof(src)");
            HA(0C, "strtoul(src, src_end, base)");
            HA(0D, "strtol(src, src_end, base)");
            HA(0E, "abs(val)");
            HA(0F, "labs(val)");
            HA(10, "atoi(src)");
            HA(11, "atol(src)");
            HA(12, "atob(src, num_dst)");
            HA(13, "setjmp(buf)");
            HA(14, "longjmp(buf, param)");
            HA(15, "strcat(dst, src)");
            HA(16, "strncat(dst, src, maxlen)");
            HA(17, "strcmp(str1, str2)");
            HA(18, "strncmp(str1, str2, maxlen)");
            HA(19, "strcpy(dst, src)");
            HA(1A, "strncpy(dst, src, maxlen)");
            HA(1B, "strlen(src)");
            HA(1C, "index(src, char)");
            HA(1D, "rindex(src, char)");
            HA(1E, "strchr(src, char)");
            HA(1F, "strrchr(src, char)");
            HA(20, "strpbrk(src, list)");
            HA(21, "strspn(src, list)");
            HA(22, "strcspn(src, list)");
            HA(23, "strtok(src, list)");
            HA(24, "strstr(str, substr)");
            HA(25, "toupper(char)");
            HA(26, "tolower(char)");
            HA(27, "bcopy(src, dst, len)");
            HA(28, "bzero(dst, len)");
            HA(29, "bcmp(ptr1, ptr2, len)");
            HA(2A, "memcpy(dst, src, len)");
            HA(2B, "memset(dst, fillbyte, len)");
            HA(2C, "memmove(dst, src, len)");
            HA(2D, "memcmp(src1, src2, len)");
            HA(2E, "memchr(src, scanbyte, len)");
            HA(2F, "rand()");
            HA(30, "srand(seed)");
            HA(31, "qsort(base, nel, width, callback)");
            HA(32, "strtod(src, src_end)");
            HA(33, "malloc(size)");
            HA(34, "free(buf)");
            HA(35, "lsearch(key, base, nel, width, callback)");
            HA(36, "bsearch(key, base, nel, width, callback)");
            HA(37, "calloc(sizx, sizy)");
            HA(38, "realloc(old_buf, new_siz)");
            HA(39, "InitHeap(addr, size)");
            HA(3A, "_exit(exitcode)");
            HAB(3B, 3C, "getchar()");
            HAB(3C, 3D, "putchar(char)");
            HAB(3D, 3E, "gets(dst)");
            HAB(3E, 3F, "puts(src)");
            HA(3F, "printf(txt, param1, param2, ...)");
            HA(40, "SystemErrorUnresolvedException()");
            HA(41, "LoadTest(filename, headerbuf)");
            HA(42, "Load(filename, headerbuf)");
            HA(43, "Exec(headerbuf, param1, param2)");
            HA(44, "FlushCache()");
            HA(45, "init_a0_b0_c0_vectors");
            HA(46, "GPU_dw(Xdst, Ydst, Xsiz, Ysiz, src)");
            HA(47, "gpu_send_dma(Xdst, Ydst, Xsiz, Ysiz, src)");
            HA(48, "SendGP1Command(gp1cmd)");
            HA(49, "GPU_cw(gp0cmd)");
            HA(4A, "GPU_cwp(src, num)");
            HA(4B, "send_gpu_linked_list(src)");
            HA(4C, "gpu_abort_dma()");
            HA(4D, "GetGPUStatus()");
            HA(4E, "gpu_sync()");
            HA(4F, "SystemError");
            HA(50, "SystemError");
            HA(51, "LoadExec(filename, stackbase, stackoffset)");
            HA(52, "GetSysSp");
            HA(53, "SystemError");
            HA(54, "_96_init()");
            HA(55, "_bu_init()");
            HA(56, "_96_remove()");
            HA(57, "return 0");
            HA(58, "return 0");
            HA(59, "return 0");
            HA(5A, "return 0");
            HA(5B, "dev_tty_init()");
            HA(5C, "dev_tty_open(fcb, path, accessmode)");
            HA(5D, "dev_tty_in_out(fcb, cmd)");
            HA(5E, "dev_tty_ioctl(fcb, cmd, arg)");
            HA(5F, "dev_cd_open(fcb, path, accessmode)");
            HA(60, "dev_cd_read(fcb, dst, len)");
            HA(61, "dev_cd_close(fcb)");
            HA(62, "dev_cd_firstfile(fcb, path, direntry)");
            HA(63, "dev_cd_nextfile(fcb, direntry)");
            HA(64, "dev_cd_chdir(fcb, path)");
            HA(65, "dev_card_open(fcb, path, accessmode)");
            HA(66, "dev_card_read(fcb, dst, len)");
            HA(67, "dev_card_write(fcb, src, len)");
            HA(68, "dev_card_close(fcb)");
            HA(69, "dev_card_firstfile(fcb, path, direntry)");
            HA(6A, "dev_card_nextfile(fcb, direntry)");
            HA(6B, "dev_card_erase(fcb, path)");
            HA(6C, "dev_card_undelete(fcb, path)");
            HA(6D, "dev_card_format(fcb)");
            HA(6E, "dev_card_rename(fcb1, path1, fcb2, path2)");
            HA(6F, "?");
            HA(70, "_bu_init()");
            HA(71, "_96_init()");
            HA(72, "_96_remove()");
            HA(73, "return 0");
            HA(74, "return 0");
            HA(75, "return 0");
            HA(76, "return 0");
            HA(77, "return 0");
            HA(78, "CdAsyncSeekL(src)");
            HA(79, "return 0");
            HA(7A, "return 0");
            HA(7B, "return 0");
            HA(7C, "CdAsyncGetStatus(dst)");
            HA(7D, "return 0");
            HA(7E, "CdAsyncReadSector(count, dst, mode)");
            HA(7F, "return 0");
            HA(80, "return 0");
            HA(81, "CdAsyncSetMode(mode)");
            HA(82, "return 0");
            HA(83, "return 0");
            HA(84, "return 0");
            HA(85, "return 0");
            HA(86, "return 0");
            HA(87, "return 0");
            HA(88, "return 0");
            HA(89, "return 0");
            HA(8A, "return 0");
            HA(8B, "return 0");
            HA(8C, "return 0");
            HA(8D, "return 0");
            HA(8E, "return 0");
            HA(8F, "return 0");
            HA(90, "CdromIoIrqFunc1()");
            HA(91, "CdromDmaIrqFunc1()");
            HA(92, "CdromIoIrqFunc2()");
            HA(93, "CdromDmaIrqFunc2()");
            HA(94, "CdromGetInt5errCode(dst1, dst2)");
            HA(95, "CdInitSubFunc()");
            HA(96, "AddCDROMDevice()");
            HA(97, "AddMemCardDevice()");
            HA(98, "AddDuartTtyDevice()");
            HA(99, "add_nullcon_driver()");
            HA(9A, "SystemError");
            HA(9B, "SystemError");
            HA(9C, "SetConf(num_EvCB, num_TCB, stacktop)");
            HA(9D, "GetConf(num_EvCB_dst, num_TCB_dst, stacktop_dst)");
            HA(9E, "SetCdromIrqAutoAbort(type, flag)");
            HA(9F, "SetMem(megabytes)");
            HA(A0, "_boot()");
            HA(A1, "SystemError(type, errorcode)");
            HA(A2, "EnqueueCdIntr()");
            HA(A3, "DequeueCdIntr()");
            HA(A4, "CdGetLbn(filename)");
            HA(A5, "CdReadSector(count, sector, buffer)");
            HA(A6, "CdGetStatus()");
            HA(A7, "bufs_cb_0()");
            HA(A8, "bufs_cb_1()");
            HA(A9, "bufs_cb_2()");
            HA(AA, "bufs_cb_3()");
            HA(AB, "_card_info(port)");
            HA(AC, "_card_load(port)");
            HA(AD, "_card_auto(flag)");
            HA(AE, "bufs_cb_4()");
            HA(AF, "card_write_test(port)");
            HA(B0, "return 0");
            HA(B1, "return 0");
            HA(B2, "ioabort_raw(param)");
            HA(B3, "return 0");
            HA(B4, "GetSystemInfo(index)");

            HB(00, "alloc_kernel_memory(size)");
            HB(01, "free_kernel_memory(buf)");
            HB(02, "init_timer(t, reload, flags)");
            HB(03, "get_timer(t)");
            HB(04, "enable_timer_irq(t)");
            HB(05, "disable_timer_irq(t)");
            HB(06, "restart_timer(t)");
            HB(07, "DeliverEvent(class, spec)");
            HB(08, "OpenEvent(class, spec, mode, func)");
            HB(09, "CloseEvent(event)");
            HB(0A, "WaitEvent(event)");
            HB(0B, "TestEvent(event)");
            HB(0C, "EnableEvent(event)");
            HB(0D, "DisableEvent(event)");
            HB(0E, "OpenTh(reg_PC, reg_SP_FP, reg_GP)");
            HB(0F, "CloseTh(handle)");
            HB(10, "ChangeTh(handle)");
            HB(11, "jump_to_00000000h");
            HB(12, "InitPAD2(buf1, siz1, buf2, siz2)");
            HB(13, "StartPAD2()");
            HB(14, "StopPAD2()");
            HB(15, "PAD_init2(type, button_dest, unused, unused)");
            HB(16, "PAD_dr()");
            HB(17, "ReturnFromException()");
            HB(18, "ResetEntryInt()");
            HB(19, "HookEntryInt(addr)");
            HB(1A, "SystemError");
            HB(1B, "SystemError");
            HB(1C, "SystemError");
            HB(1D, "SystemError");
            HB(1E, "SystemError");
            HB(1F, "SystemError");
            HB(20, "UnDeliverEvent(class, spec)");
            HB(21, "SystemError");
            HB(22, "SystemError");
            HB(23, "SystemError");
            HB(24, "jump_to_00000000h");
            HB(25, "jump_to_00000000h");
            HB(26, "jump_to_00000000h");
            HB(27, "jump_to_00000000h");
            HB(28, "jump_to_00000000h");
            HB(29, "jump_to_00000000h");
            HB(2A, "SystemError");
            HB(2B, "SystemError");
            HB(2C, "jump_to_00000000h");
            HB(2D, "jump_to_00000000h");
            HB(2E, "jump_to_00000000h");
            HB(2F, "jump_to_00000000h");
            HB(30, "jump_to_00000000h");
            HB(31, "jump_to_00000000h");
            HB(40, "cd(name)");
            HB(41, "format(devicename)");
            HB(42, "firstfile2(filename, direntry)");
            HB(43, "nextfile(direntry)");
            HB(44, "rename(old_filename, new_filename)");
            HB(45, "erase(filename)");
            HB(46, "undelete(filename)");
            HB(47, "AddDrv(device_info)");
            HB(48, "DelDrv(device_name_lowercase)");
            HB(49, "PrintInstalledDevices()");
            HB(4A, "InitCARD2(pad_enable)");
            HB(4B, "StartCARD2()");
            HB(4C, "StopCARD2()");
            HB(4D, "_card_info_subfunc(port)");
            HB(4E, "_card_write(port, sector, src)");
            HB(4F, "_card_read(port, sector, dst)");
            HB(50, "_new_card()");
            HB(51, "Krom2RawAdd(shiftjis_code)");
            HB(52, "SystemError");
            HB(53, "Krom2Offset(shiftjis_code)");
            HB(54, "_get_errno()");
            HB(55, "_get_error(fd)");
            HB(56, "GetC0Table");
            HB(57, "GetB0Table");
            HB(58, "_card_chan()");
            HB(59, "testdevice(devicename)");
            HB(5A, "SystemError");
            HB(5B, "ChangeClearPAD(int)");
            HB(5C, "_card_status(slot)");
            HB(5D, "_card_wait(slot)");

            HAC(00, "EnqueueTimerAndVblankIrqs(priority)");
            HAC(01, "EnqueueSyscallHandler(priority)");
            HAC(02, "SysEnqIntRP(priority, struc)");
            HAC(03, "SysDeqIntRP(priority, struc)");
            HAC(04, "get_free_EvCB_slot()");
            HAC(05, "get_free_TCB_slot()");
            HAC(06, "ExceptionHandler()");
            HAC(07, "InstallExceptionHandlers()");
            HAC(08, "SysInitMemory(addr, size)");
            HAC(09, "SysInitKernelVariables()");
            HAC(0A, "ChangeClearRCnt(t, flag)");
            HAC(0B, "SystemError");
            HAC(0C, "InitDefInt(priority)");
            HAC(0D, "SetIrqAutoAck(irq, flag)");
            HAC(0E, "return 0");
            HAC(0F, "return 0");
            HAC(10, "return 0");
            HAC(11, "return 0");
            HAC(12, "InstallDevices(ttyflag)");
            HAC(13, "FlushStdInOutPut()");
            HAC(14, "return 0");
            HAC(15, "_cdevinput(circ, char)");
            HAC(16, "_cdevscan()");
            HAC(17, "_circgetc(circ)");
            HAC(18, "_circputc(char, circ)");
            HAC(19, "_ioabort(txt1, txt2)");
            HAC(1A, "set_card_find_mode(mode)");
            HAC(1B, "KernelRedirect(ttyflag)");
            HAC(1C, "AdjustA0Table()");
            HAC(1D, "get_card_find_mode()");

            default:
                ptr += snprintf(ptr, sizeof(bstr) - (ptr - bstr), "Unknown PC:%02x r9:%08x", PC, r9); break;
        }
    }
    dbgloglog(PS1D_GENERAL_KERNAL_FUNCS, DBGLS_TRACE, "KCALL: %s", bstr);
}

#undef HA
#undef HB
#undef HAB
#undef HAC

static void setup_dbglog(debugger_interface *dbgr, core *th)
{
    cvec_ptr p = dbgr->make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th->dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = true;

    dbglog_category_node &root = dv.get_category_root();
    root.children.reserve(10);
    dbglog_category_node &r3000 = root.add_node(dv, "R3000", nullptr, 0, 0x80FF80);
    r3000.children.reserve(10);
    r3000.add_node(dv, "Instructions", "R3000", PS1D_R3000_INSTRUCTION, 0x80FF80);
    th->cpu.dbg.dvptr = &dv;
    th->cpu.dbg.dv_id = PS1D_R3000_INSTRUCTION;
    th->cpu.trace.rfe_id = PS1D_R3000_RFE;
    th->cpu.trace.exception_id = PS1D_R3000_EXCEPTION;
    th->cpu.trace.I_MASK_write =
    th->cpu.trace.I_STAT_write = PS1D_BUS_IRQ_ACK;;
    //r3000.add_node(dv, "IRQs", "IRQ", PS1D_R3000_IRQ, 0xFFFFFF);
    r3000.add_node(dv, "RFEs", "RFE", PS1D_R3000_RFE, 0xFFFFFF);
    r3000.add_node(dv, "Exceptions", "Except", PS1D_R3000_EXCEPTION, 0xFFFFFF);

    u32 dma_c = 0xFF2080;
    dbglog_category_node &dma = root.add_node(dv, "DMA", nullptr, 0, dma_c);
    dma.children.reserve(10);
    dma.add_node(dv, "DMA0 MDEC in", "DMA0", PS1D_DMA_CH0, dma_c);
    dma.add_node(dv, "DMA1 MDEC out", "DMA1", PS1D_DMA_CH1, dma_c);
    dma.add_node(dv, "DMA2 GPU", "DMA2", PS1D_DMA_CH2, dma_c);
    dma.add_node(dv, "DMA3 CDROM", "DMA3", PS1D_DMA_CH3, dma_c);
    dma.add_node(dv, "DMA4 SPU", "DMA4", PS1D_DMA_CH4, dma_c);
    dma.add_node(dv, "DMA5 PIO", "DMA5", PS1D_DMA_CH5, dma_c);
    dma.add_node(dv, "DMA6 OTC", "DMA6", PS1D_DMA_CH6, dma_c);

    // CDROM STAT IO, IF/IEs
    // DMA IF/IE
    // IF/IE
    // SPUCNT/STAT
    dma_c = 0x20A0A0;
    dbglog_category_node &cdrom = root.add_node(dv, "CDROM Drive", nullptr, 0, dma_c);
    cdrom.children.reserve(30);
    cdrom.add_node(dv, "Command", "CMD", PS1D_CDROM_CMD, dma_c);
    cdrom.add_node(dv, "Register read/write", "Reg R/W", PS1D_CDROM_REGRW, dma_c);
    cdrom.add_node(dv, "CMD SetMode", "SetMode", PS1D_CDROM_SETMODE, dma_c);
    cdrom.add_node(dv, "CMD Read", "Read", PS1D_CDROM_READ, dma_c);
    cdrom.add_node(dv, "CMD Play", "Play", PS1D_CDROM_PLAY, dma_c);
    cdrom.add_node(dv, "CMD Pause", "Pause", PS1D_CDROM_PAUSE, dma_c);
    cdrom.add_node(dv, "CMD SetLoc", "SetLoc", PS1D_CDROM_SETLOC, dma_c);
    cdrom.add_node(dv, "CMD Seek", "Seek", PS1D_CDROM_SEEK, dma_c);
    cdrom.add_node(dv, "CMD Reset", "Reset", PS1D_CDROM_CMD_RESET, dma_c);
    cdrom.add_node(dv, "Result", "Result", PS1D_CDROM_RESULT, dma_c);
    cdrom.add_node(dv, "IRQ Queued", "IRQ_Q", PS1D_CDROM_IRQ_QUEUE, dma_c);
    cdrom.add_node(dv, "IRQ Asserted", "IRQ_ASRT", PS1D_CDROM_IRQ_ASSERT, dma_c);
    cdrom.add_node(dv, "Sector read", "R.Sector", PS1D_CDROM_SECTOR_READS, dma_c);
    cdrom.add_node(dv, "RDDATA Start", "RDDATA_start", PS1D_CDROM_RDDATA_START, dma_c);
    cdrom.add_node(dv, "RDDATA Finish", "RDDATA_finish", PS1D_CDROM_RDDATA_FINISH, dma_c);

    dma_c = 0x7042FF;
    dbglog_category_node &bus = root.add_node(dv, "Bus Events", nullptr, 0, dma_c);
    bus.children.reserve(10);
    bus.add_node(dv, "IRQs", "IRQ", PS1D_BUS_IRQS, dma_c);
    bus.add_node(dv, "IRQ ACKs", "IRQ ACK", PS1D_BUS_IRQ_ACK, dma_c);
    bus.add_node(dv, "Console Logs", "Console", PS1D_BUS_CONSOLE, dma_c);
    bus.add_node(dv, "IO area R/W", "IO RW", PS1D_BUS_REGAREA, dma_c);
    th->cpu.trace.console_log_id = PS1D_BUS_CONSOLE;

    dma_c = 0x00FFFF;
    dbglog_category_node &sio = root.add_node(dv, "SIO0", nullptr, 0, dma_c);
    sio.children.reserve(10);
    sio.add_node(dv, "SIO0 R/W", "SIO0_RW", PS1D_SIO0_RW, dma_c);
    sio.add_node(dv, "Data XCHG", "SIO0_XCHG", PS1D_SIO0_XCHG, dma_c);
    sio.add_node(dv, "SIO0 ACK", "SIO0_ACK", PS1D_SIO0_ACK, dma_c);
    sio.add_node(dv, "SIO0 IRQ", "SIO0_IRQ", PS1D_SIO0_IRQ, dma_c);

    dma_c = 0x5020FF;
    dbglog_category_node &general = root.add_node(dv, "General", nullptr, 0, dma_c);
    general.children.reserve(10);
    general.add_node(dv, "Kernal Function Calls", "KFC", PS1D_GENERAL_KERNAL_FUNCS, dma_c);
    th->cpu.khook_ptr = th;
    th->cpu.khook = &kernel_call;
}

static void setup_waveforms(core& th, debugger_interface *dbgr) {
    th.dbg.waveforms2.view = dbgr->make_view(dview_waveform2);
    auto *dview = &th.dbg.waveforms2.view.get();
    auto *wv = &dview->waveform2;
    snprintf(wv->name, sizeof(wv->name), "SPU");
    auto &root = wv->root;
    root.children.reserve(10);

    th.dbg.waveforms2.main = &root;
    th.dbg.waveforms2.main_cache = &root.data;
    snprintf(root.data.name, sizeof(root.data.name), "Stereo Out");
    root.data.kind = debug::waveform2::wk_big;
    root.data.samples_requested = 400;
    root.data.stereo = true;

    auto &main = root.add_child_category("Voices", 32);
    for (u32 i = 0; i < 24; i++) {
        auto *v = main.add_child_wf(debug::waveform2::wk_small, th.dbg.waveforms2.channels.chan[i]);
        auto *ch = &v->data;
        th.dbg.waveforms2.channels.chan_cache[i] = &v->data;
        snprintf(ch->name, sizeof(ch->name), "CH%d", i);
    }

    auto &cd = root.add_child_category("CD Audio", 2);
    auto *v = cd.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.cd.chan[0]);
    th.dbg.waveforms2.cd.chan_cache[0] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Output");

    auto &reverb = root.add_child_category("Reverb Processing", 8);
    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[0]);
    th.dbg.waveforms2.reverb.chan_cache[0] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Reverb In");

    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[1]);
    th.dbg.waveforms2.reverb.chan_cache[1] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Reverb Out");

    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[2]);
    th.dbg.waveforms2.reverb.chan_cache[2] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Debug");

    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[2]);
    th.dbg.waveforms2.reverb.chan_cache[3] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Debug2");
}

static void setup_image_view_vram(core* th, debugger_interface *dbgr) {
    th->dbg.image_views.vram = dbgr->make_view(dview_image);
    auto *dview = &th->dbg.image_views.vram.get();
    image_view *iv = &dview->image;
    iv->width = 1024;
    iv->height = 512;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2( 0, 0 );
    iv->viewport.p[1] = ivec2( iv->width, iv->height );

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_vram;
    snprintf(iv->label, sizeof(iv->label), "VRAM view");
    debugger_widgets_add_textbox(dview->options, "blah!", 1);
}

static void setup_image_view_sysinfo(core* th, debugger_interface *dbgr) {
    debugger_view *dview;
    th->dbg.image_views.sysinfo = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.sysinfo.get();
    image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 10, 10 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_sysinfo;

    snprintf(iv->label, sizeof(iv->label), "Sys Info View");

    debugger_widgets_add_textbox(dview->options, "blah!", 1);
}

static void setup_image_view_spuinfo(core* th, debugger_interface *dbgr) {
    debugger_view *dview;
    th->dbg.image_views.sysinfo = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.sysinfo.get();
    image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 10, 10 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_spuinfo;

    snprintf(iv->label, sizeof(iv->label), "SPU Info View");

    debugger_widgets_add_textbox(dview->options, "blah!", 1);
}

static void readcpumembus(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    u8 *out = static_cast<u8 *>(dest);
    auto *th = static_cast<core *>(ptr);
    u32 offset = 0;
    for (u32 i = 0; i < 4; i++) {
        u32 a = th->mainbus_read((addr + offset) & 0x1FFFFFFF, 4, false);
        cW32(dest, offset, a);
        offset += 4;
    }
}


static void setup_memory_view(core *th, debugger_interface *dbgr) {
    th->dbg.memory = dbgr->make_view(dview_memory);
    debugger_view *dview = &th->dbg.memory.get();
    memory_view *mv = &dview->memory;
    mv->add_module("CPU Bus", 0, 8, 0, 0x1EFFFFFF, th, &readcpumembus);
    //mv->add_module("CPU View (16bit)", 1, 6, 0, 0xFFFF, th, &readcpumemnative);
    //mv->add_module("VDC0 VRAM", 2, 4, 0, 0xFFFF, &th->vdc0.VRAM, &readvram);

}


void core::setup_debugger_interface(debugger_interface &dbgr)
{
    dbg.interface = &dbgr;
    dbgr.views.reserve(15);

    dbgr.supported_by_core = true;
    dbgr.smallest_step = 1;
    setup_console_view(this, &dbgr);
    setup_dbglog(&dbgr, this);
    setup_waveforms(*this, &dbgr);
    setup_image_view_sysinfo(this, &dbgr);
    setup_image_view_spuinfo(this, &dbgr);
    setup_image_view_vram(this, &dbgr);
    setup_memory_view(this, &dbgr);
}
}