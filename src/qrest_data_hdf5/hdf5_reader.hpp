#ifndef QREST_DATA_HDF5_READER_HPP
#define QREST_DATA_HDF5_READER_HPP

#include <memory>
#include <string>
#include <vector>

#include <H5Cpp.h>

#include "metadata.hpp"

namespace qrest_data
{

class Hdf5Reader
{
public:
    Hdf5Reader() = default;
    ~Hdf5Reader() { close(); }

    Hdf5Reader(const Hdf5Reader &) = delete;
    Hdf5Reader &operator=(const Hdf5Reader &) = delete;
    Hdf5Reader(Hdf5Reader &&) = default;
    Hdf5Reader &operator=(Hdf5Reader &&) = default;

    void open(const std::string &filename);

    Metadata read_metadata() const;
    std::vector<double> read_accform() const;

    std::size_t get_npts() const;
    std::size_t get_channel_num() const;

    void close();
    bool is_open() const noexcept { return file_ != nullptr; }

private:
    std::unique_ptr<H5::H5File> file_;
};

} // namespace qrest_data

#endif // QREST_DATA_HDF5_READER_HPP
