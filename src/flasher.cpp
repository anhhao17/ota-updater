#include "flash/flasher.hpp"

#include "flash/flasher_engine.hpp"

namespace flash {

Result Flasher::Run(IReader& reader, IWriter& writer, const FlashOptions& opt) {
    FlasherEngine engine;
    return engine.Run(reader, writer, opt);
}

} // namespace flash
