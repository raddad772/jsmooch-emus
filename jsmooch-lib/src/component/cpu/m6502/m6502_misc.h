//
// Created by Dave on 2/4/2024.
//

#pragma once
namespace M6502 {
struct regs;
struct pins;

typedef void (*ins_func)(regs&, pins&);
}
