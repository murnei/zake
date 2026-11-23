#include "zrn.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "error.h"

namespace fs = std::filesystem;

void write_zrn(const fs::path& flat_bin, const fs::path& out, uint32_t version) {
    std::ifstream in(flat_bin, std::ios::binary);
    if (!in.is_open())
        throw ZakeError(ErrorCode::IoError, "cannot open " + flat_bin.string());

    std::vector<char> code((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());

    if (code.empty())
        throw ZakeError(ErrorCode::LinkFailed, "linker produced an empty binary");

    zrn_header_t header{};
    std::memcpy(header.magic, ".ZRN", 4);
    header.header_size = sizeof(zrn_header_t);
    header.version     = version;
    header.flags       = 0;
    header.code_size   = static_cast<uint32_t>(code.size());

    std::ofstream file(out, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        throw ZakeError(ErrorCode::IoError, "cannot create " + out.string());

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(code.data(), static_cast<std::streamsize>(code.size()));
    file.close();

    if (!file)
        throw ZakeError(ErrorCode::IoError, "failed to write " + out.string());
}
