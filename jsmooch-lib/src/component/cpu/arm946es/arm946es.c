//
// Created by . on 1/18/25.
//
#include <string.h>
#include "arm946es.h"
#include "arm946es_decode.h"

void ARM946ES_flush_pipeline(struct ARM946ES *this)
{
    this->pipeline.flushed = 1;
}

void ARM946ES_init(struct ARM946ES *this, u32 *waitstates)
{
    //dbg.trace_on = 1;
    memset(this, 0, sizeof(*this));
    ARM946ES_fill_arm_table(this);
    this->waitstates = waitstates;
    for (u32 i = 0; i < 16; i++) {
        this->regmap[i] = &this->regs.R[i];
    }
    for (u32 i = 0; i < 65536; i++) {
        decode_thumb2(i, &this->opcode_table_thumb[i]);
    }
    jsm_string_init(&this->trace.str, 100);
    jsm_string_init(&this->trace.str2, 100);

    DBG_EVENT_VIEW_INIT;
    DBG_TRACE_VIEW_INIT;
}

void ARM946ES_delete(struct ARM946ES *this)
{

}

void ARM7TDMI_idle(struct ARM946ES*this, u32 num)
{
    *this->waitstates += num;
}
