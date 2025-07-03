#pragma once

#include <stdint.h>

// https://github.com/cranksters/playdate-reverse-engineering/blob/main/formats/pdi.md

#define PDI_MAGIC "Playdate IMG"
#define PDI_FLAG_COMPRESSED 0x80000000
#define PDI_CELL_FLAG_TRANSPARENCY 3

/*
PDIHeader

// if uncompressed:
{
    PDICell

    char white[stride * clip_height]

    // if transparency:
    char opaque[stride * clip_height]
}

// if compressed:
{
    PDIMetadata

    // everything that follows is z-lib compressed
    PDICell

    char white[stride * clip_height]

    // if transparency:
    char opaque[stride * clip_height]
}
*/

#pragma pack(push, 1)
struct PDIHeader
{
    char magic[12];
    uint32_t flags;
};

struct PDIMetadata
{
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
};

struct PDICell
{
    uint16_t clip_width;
    uint16_t clip_height;
    uint16_t stride;  // must be a multiple of 4 (i.e. 32 bits)
    uint16_t clip_left;
    uint16_t clip_right;
    uint16_t clip_top;
    uint16_t clip_bottom;
    uint16_t flags;
};
#pragma pack(pop)