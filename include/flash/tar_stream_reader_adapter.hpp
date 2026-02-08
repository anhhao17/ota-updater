#pragma once

#include "flash/io.hpp"

#include <archive.h>

#include <string>

namespace flash {

int OpenArchiveFromReader(struct archive* ar, IReader& reader);
std::string ArchiveErr(struct archive* ar);

} // namespace flash
