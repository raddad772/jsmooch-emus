//
// Created by howprice on 5/7/25.
//

#pragma once

// Cross platform struct packing macros.
// 
// Usage:
// PACK_BEGIN
// struct Foo
// {
//     int x;
// } PACK_END;
// 
// #TODO: Move to helpers/pack.h 
#ifdef _MSC_VER
#define PACK_BEGIN __pragma(pack(push, 1)) 
#define PACK_END __pragma(pack(pop))
#else
#define PACK_BEGIN
#define PACK_END __attribute__((__packed__))
#endif

