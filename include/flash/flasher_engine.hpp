#pragma once

#include "flash/flasher.hpp"

namespace flash {

class FlasherEngine {
public:
    Result Run(IReader& reader, IWriter& writer, const FlashOptions& opt) const;
};

} // namespace flash
