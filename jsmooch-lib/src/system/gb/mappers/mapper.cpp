#include <cstdlib>
#include <cstdio>
#include <cassert>

#include "../cart.h"
#include "mapper.h"
#include "no_mapper.h"
#include "mbc1.h"
#include "mbc2.h"
#include "mbc3.h"
#include "mbc5.h"


namespace GB {
struct clock;
struct core;

MAPPER* new_GB_mapper(clock* clock_in, core* bus_in, mappers which)
{
	MAPPER* mapper = static_cast<MAPPER *>(malloc(sizeof(MAPPER)));
	mapper->which = which;
    mapper->ptr = nullptr;
	switch (which) {
	case NONE: // No mapper!
		GB_mapper_none_new(mapper, clock_in, bus_in);
        printf("\nNO MAPPER!");
		break;
    case MBC1:
        GB_mapper_MBC1_new(mapper, clock_in, bus_in);
        printf("\nMBC1");
        break;
    case MBC2:
        GB_mapper_MBC2_new(mapper, clock_in, bus_in);
        printf("\nMBC2");
        break;
    case MBC3:
        GB_mapper_MBC3_new(mapper, clock_in, bus_in);
        printf("\nMBC3");
        break;
    case MBC5:
        GB_mapper_MBC5_new(mapper, clock_in, bus_in);
        printf("\nMBC5");
        break;
	default:
		printf("\nNO SUPPORTED MAPPER! %d", which);
		break;
	}
    fflush(stdout);
	return mapper;
}

void delete_GB_mapper(MAPPER* whom)
{
	switch (whom->which) {
	case mappers::NONE: // No-mapper!
		GB_mapper_none_delete(whom);
		break;
    case mappers::MBC1:
        GB_mapper_MBC1_delete(whom);
        break;
    case mappers::MBC2:
        GB_mapper_MBC2_delete(whom);
        break;
    case mappers::MBC3:
        GB_mapper_MBC3_delete(whom);
        break;
    case mappers::MBC5:
        GB_mapper_MBC5_delete(whom);
        break;
    default:
        assert(1==0);
        break;
	}
    free(whom);
}
}