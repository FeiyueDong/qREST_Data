#pragma once
#include "../mseed/modified_mseed.hpp"
namespace qrest_data::tools::sit {
std::vector<mseed::Record> read_dat_records(const std::string &path);
}
