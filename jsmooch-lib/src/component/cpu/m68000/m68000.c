//
// Created by RadDad772 on 4/14/24.
//

#include "m68000.h"

void M68k_init(struct M68k* this)
{

}

void M68k_delete(struct M68k* this)
{

}

void M68k_reset(struct M68k* this)
{
    /* after reset...
1)      Loads IRC from external memory.
2)      Transfer IRC to IR.
3)      Reload IRC from external memory.
4)      Transfer IR to IRD.
     */
}