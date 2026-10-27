#pragma once
#include <span>
#include "modified_mseed.hpp"
namespace qrest_data::tools::mseed {
std::vector<std::int32_t> decode_steim2(std::span<const std::uint8_t> record,
                                        const Header &header);
}
