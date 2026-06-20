#ifndef QREST_DATA_HDF5_WRITER_HPP
#define QREST_DATA_HDF5_WRITER_HPP

#include <memory>
#include <string>
#include <vector>

#include <H5Cpp.h>

#include "metadata.hpp"

namespace qrest_data
{

class Hdf5Writer
{
public:
    Hdf5Writer() = default;
    ~Hdf5Writer() { close(); }

    Hdf5Writer(const Hdf5Writer &) = delete;
    Hdf5Writer &operator=(const Hdf5Writer &) = delete;
    Hdf5Writer(Hdf5Writer &&) = default;
    Hdf5Writer &operator=(Hdf5Writer &&) = default;

    void open(const std::string &filename);
    void write(const Metadata &metadata,
               const std::vector<double> &data,
               std::size_t npts,
               std::size_t channel_num);
    void close();
    bool is_open() const noexcept { return file_ != nullptr; }

private:
    std::unique_ptr<H5::H5File> file_;
};

} // namespace qrest_data

#endif // QREST_DATA_HDF5_WRITER_HPP
