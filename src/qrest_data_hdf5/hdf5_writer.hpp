#ifndef QREST_DATA_HDF5_WRITER_HPP
#define QREST_DATA_HDF5_WRITER_HPP

#include <string>
#include <vector>

#include <hdf5.h>

#include "hdf5_export.hpp"
#include "metadata.hpp"

namespace qrest_data
{

class QREST_DATA_HDF5_API Hdf5Writer
{
public:
    Hdf5Writer() = default;
    ~Hdf5Writer() { close(); }

    Hdf5Writer(const Hdf5Writer &) = delete;
    Hdf5Writer &operator=(const Hdf5Writer &) = delete;
    Hdf5Writer(Hdf5Writer &&other) noexcept;
    Hdf5Writer &operator=(Hdf5Writer &&other) noexcept;

    void open(const std::string &filename);
    void write(const Metadata &metadata,
               const std::vector<double> &data,
               std::size_t npts,
               std::size_t channel_num);
    void close();
    bool is_open() const noexcept { return file_ >= 0; }

private:
    hid_t file_ = H5I_INVALID_HID;
};

} // namespace qrest_data

#endif // QREST_DATA_HDF5_WRITER_HPP
