#include "mapper.h"
#include "no_mapper.h"
#include <malloc.h>
#include <stdio.h>

struct GB_mapper* new_GB_mapper(struct GB_clock* clock, struct GB_bus* bus, enum GB_mappers which)
{
	struct GB_mapper* mapper = malloc(sizeof(struct GB_mapper));
	mapper->which = which;
	switch (which) {
	case NONE: // No mapper!
		GB_mapper_none_new(mapper, clock, bus);
		break;
	default:
		printf("NO SUPPORTED MAPPER! %d", which);
		break;
	}
	return mapper;
}

void delete_GB_mapper(struct GB_mapper* whom)
{
	switch (whom->which) {
	case NONE: // No-mapper!
		GB_mapper_none_delete(whom);
		break;
	}
    free(whom);
}
