#pragma once

#include <cstdint>
#include <filesystem>

struct zrn_header_t {
    char     magic[4];
    uint32_t header_size;
    uint32_t version;
    uint32_t flags;
    uint32_t code_size;
    uint32_t reserved[3];
};

static_assert(sizeof(zrn_header_t) == 32, "zrn_header_t must be 32 bytes");
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "ZRN is little-endian");

void write_zrn(const std::filesystem::path& flat_bin,
               const std::filesystem::path& out,
               uint32_t version);
