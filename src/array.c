//
//  array.c
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 23/08/21.
//  Maintained and developed by the CrankBoy dev team.
//

#include "array.h"

#include "utility.h"

PGB_Array* array_new(void)
{
    PGB_Array* array = pgb_malloc(sizeof(PGB_Array));
    array->length = 0;
    array->capacity = 0;
    array->items = NULL;
    return array;
}

void array_reserve(PGB_Array* array, unsigned int capacity)
{
    if (array->capacity >= capacity)
    {
        return;
    }

    array->items = pgb_realloc(array->items, capacity * sizeof(void*));
    if (array->items)
    {
        array->capacity = capacity;
    }
}

void array_push(PGB_Array* array, void* item)
{
    if (array->length == array->capacity)
    {
        unsigned int new_capacity = (array->capacity == 0) ? 8 : array->capacity * 2;
        array_reserve(array, new_capacity);
    }

    array->items[array->length] = item;
    array->length++;
}

void array_clear(PGB_Array* array)
{
    pgb_free(array->items);
    array->items = NULL;
    array->length = 0;
    array->capacity = 0;
}

void array_free(PGB_Array* array)
{
    if (array)
    {
        pgb_free(array->items);
        pgb_free(array);
    }
}
