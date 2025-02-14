//
// Created by . on 2/13/25.
//

#include <stdlib.h>
#include <printf.h>

#include "ps1_bus.h"
#include "ps1_device.h"

static void gamepad_new(struct PS1_gamepad *this)
{
    this->state = 0xFFFF;
}

void PS1_peripheral_init(struct PS1_peripheral *this, enum PS1_peripheral_kinds kind)
{
    this->kind = kind;
    switch(kind) {
        case PS1PK_disconnected:
            return;
        case PS1PK_memorycard:
            //memorycard_new(&this->device.memcard);
            return;
        case PS1PK_gamepad:
            gamepad_new(&this->device.gamepad);
            return;
    }
}

void PS1_peripheral_delete(struct PS1_peripheral *this)
{

}

static void peripheral_exchange_byte(struct PS1_peripheral *this, u8 cmd, struct u8_dsr_state *out)
{
    if (!this->active) {
        out->r1 = 0xFF;
        out->r2.kind = PS1DSK_idle;
        return;
    }

    // device_handle_command(this, this->seq, cmd); XX

    this->active = out->r2.kind != PS1DSK_idle;
    this->seq += 1;
}

static void dsr_state_delay_by(struct dsr_state *this, i32 offset)
{
    if (this->kind == PS1DSK_pending)
        this->delay += offset;
}

static void memcard_maybe_exchange_byte(struct PS1 *psx, struct PS1_pad_memcard *this)
{
    if (this->tx_pending == -1) return;
    if (!this->tx_en) return;
    // this->kind === TransferStateKind.Idle
    if (this->transfer_state.kind != PS1TSK_idle) return;

    u8 to_send = (u8)this->tx_pending;

    if ((this->baud_div < 80) || (this->baud_div > 239)) {
        printf("\nWARNING unimplemented baud divider! %d", this->baud_div);
        return;
    }
    if (!this->select) {
        printf("\nWARNING pad/memcard TX without selection");
        return;
    }

    this->tx_pending = -1;
    i32 bd = (i32)this->baud_div;
    i32 to_tx_start = bd - 40;
    i32 tx_total = (bd - 11) * 11;
    i32 to_tx_end = tx_total - to_tx_start;

    i32 to_dsr_start = tx_total - bd;

    struct u8_dsr_state padr;
    struct u8_dsr_state mcr;
    u8 resp = 0;

    switch(this->target) {
        case PS1PT_PadMemCard1:

            peripheral_exchange_byte(&this->pad1, to_send, &padr);
            peripheral_exchange_byte(&this->memcard1, to_send, &mcr);

            this->pad1_dsr = padr.r2;
            dsr_state_delay_by(&this->pad1_dsr, to_dsr_start);
            this->memcard1_dsr = mcr.r2;
            dsr_state_delay_by(&this->memcard1_dsr, to_dsr_start);
            resp = padr.r1 & mcr.r1;
            break;
        case PS1PT_PadMemCard2:
            peripheral_exchange_byte(&this->pad2, to_send, &padr);
            peripheral_exchange_byte(&this->memcard2, to_send, &mcr);

            this->pad2_dsr = padr.r2;
            dsr_state_delay_by(&this->pad2_dsr, to_dsr_start);
            this->memcard2_dsr = mcr.r2;
            dsr_state_delay_by(&this->memcard2_dsr, to_dsr_start);
            resp = padr.r1 & mcr.r1;
            break;
        default:
            NOGOHERE;
            return;
    }

    this->transfer_state.kind = PS1TSK_tx_start;
    this->transfer_state.delay = to_tx_start;
    this->transfer_state.rx_available_delay = to_tx_end;
    this->transfer_state.value = resp;
}

static void run_transfer(struct PS1 *psx, i32 cycles)
{
    while (cycles > 0) {
        i32 elapsed = 0;
        struct PS1_transfer_state *ts = &psx->pad_memcard.transfer_state;
        switch(ts->kind) {
            case PS1TSK_idle:
                elapsed = cycles;
                break;
            case PS1TSK_tx_start:
                if (cycles < ts->delay) {
                    ts->delay = ts->delay - cycles;
                    elapsed = cycles;
                }
                else {
                    ts->kind = PS1TSK_rx_available;
                    // HEREI S GOOD? pad_memcardmod.rs line 370
                    ts->delay = ts->rx_available_delay;
                    elapsed = ts->delay;
                }
                break;
            case PS1TSK_rx_available:
                if (cycles < ts->delay) {
                    ts->kind = PS1TSK_rx_available;
                    ts->delay = ts->rx_available_delay = ts->delay - cycles;
                    elapsed = cycles;
                }
                else {
                    if (psx->pad_memcard.rx_not_empty) {
                        printf("\nERROR! Gamepad RX while FIFO isn't empty!");
                    }
                    psx->pad_memcard.response = ts->value;
                    psx->pad_memcard.rx_not_empty = 1;
                    psx->pad_memcard.transfer_state.kind = PS1TSK_idle;
                    elapsed = ts->delay;
                }
                break;
        }

        memcard_maybe_exchange_byte(psx, &psx->pad_memcard);

        cycles -= elapsed;
    }
}

static void dsr_run(struct dsr_state *this, i32 cycles)
{
    while (cycles > 0) {
        switch(this->kind) {
            case PS1DSK_idle:
                cycles = 0;
                break;
            case PS1DSK_pending:
                if (this->delay > cycles) {
                    this->delay = this->delay - cycles;
                    cycles = 0;
                    this->kind = PS1DSK_pending;
                } else {
                    cycles -= this->delay;
                    this->delay = 0;
                    this->kind = PS1DSK_active;
                }
                break;
            case PS1DSK_active:
                if (this->duration > cycles) {
                    this->duration = this->duration - cycles;
                    cycles = 0;
                } else {
                    cycles -= this->duration;
                    this->kind = PS1DSK_idle;
                }
        }
    }
}

static void pmc_refresh_irq_level(struct PS1_pad_memcard *this)
{
    u32 active = this->pad1_dsr.kind == PS1DSK_active;
    active |= this->pad2_dsr.kind == PS1DSK_active;
    active |= this->memcard1_dsr.kind == PS1DSK_active;
    active |= this->memcard2_dsr.kind == PS1DSK_active;

    this->interrupt |= active & this->dsr_it;
}

static void run_dsr(struct PS1 *this, i32 cycles)
{
    dsr_run(&this->pad_memcard.pad1_dsr, cycles);
    dsr_run(&this->pad_memcard.pad2_dsr, cycles);
    dsr_run(&this->pad_memcard.memcard1_dsr, cycles);
    dsr_run(&this->pad_memcard.memcard2_dsr, cycles);
    pmc_refresh_irq_level(&this->pad_memcard);
    // XX PS1_set_irq(PS1IRQ_PadMemCardByteRecv, &this->pad_memcard.interrupt);
}

void PS1_run_controllers(struct PS1 *this, u32 numcycles)
{
    run_transfer(this, numcycles);
    run_dsr(this, numcycles);
}