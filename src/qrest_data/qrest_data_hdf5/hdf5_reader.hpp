#ifndef QREST_DATA_HDF5_READER_HPP
#define QREST_DATA_HDF5_READER_HPP

#include <string>
#include <vector>

#include <hdf5.h>

#include "hdf5_export.hpp"
#include "metadata.hpp"

namespace qrest_data {

class QREST_DATA_HDF5_API Hdf5Reader {
public:
    Hdf5Reader() = default;
    ~Hdf5Reader() { close(); }

    Hdf5Reader(const Hdf5Reader &) = delete;
    Hdf5Reader &operator=(const Hdf5Reader &) = delete;
    Hdf5Reader(Hdf5Reader &&other) noexcept;
    Hdf5Reader &operator=(Hdf5Reader &&other) noexcept;

    void open(const std::string &filename);

    Metadata read_metadata() const;
    std::vector<double> read_accform() const;

    std::size_t get_npts() const;
    std::size_t get_channel_num() const;

    void close();
    bool is_open() const noexcept { return file_ >= 0; }

private:
    hid_t file_ = H5I_INVALID_HID;
};

} // namespace qrest_data

#endif // QREST_DATA_HDF5_READER_HPP
