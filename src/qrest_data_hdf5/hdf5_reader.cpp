#include "hdf5_reader.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace qrest_data {

namespace {

class H5Handle {
public:
    using Closer = herr_t (*)(hid_t);

    H5Handle(hid_t id, Closer closer) : id_(id), closer_(closer) {
        if (id_ < 0)
            throw std::runtime_error("HDF5: failed to open object");
    }

    ~H5Handle() {
        if (id_ >= 0)
            closer_(id_);
    }

    H5Handle(const H5Handle &) = delete;
    H5Handle &operator=(const H5Handle &) = delete;

    hid_t get() const noexcept { return id_; }

private:
    hid_t id_;
    Closer closer_;
};

void check_hdf5(herr_t status, const char *message) {
    if (status < 0)
        throw std::runtime_error(message);
}

std::size_t checked_size(hsize_t value, const char *name) {
    if (value > static_cast<hsize_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string("Hdf5Reader: ") + name
                                 + " is too large");
    }
    return static_cast<std::size_t>(value);
}

std::size_t checked_value_count(std::size_t rows, std::size_t columns) {
    if (columns != 0
        && rows > std::numeric_limits<std::size_t>::max() / columns) {
        throw std::runtime_error(
            "Hdf5Reader: acceleration dimensions overflow");
    }
    return rows * columns;
}

} // namespace

Hdf5Reader::Hdf5Reader(Hdf5Reader &&other) noexcept
    : file_(std::exchange(other.file_, H5I_INVALID_HID)) {}

Hdf5Reader &Hdf5Reader::operator=(Hdf5Reader &&other) noexcept {
    if (this != &other) {
        close();
        file_ = std::exchange(other.file_, H5I_INVALID_HID);
    }
    return *this;
}

void Hdf5Reader::open(const std::string &filename) {
    close();
    file_ = H5Fopen(filename.c_str(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_ < 0)
        throw std::runtime_error("Hdf5Reader: failed to open file");
}

Metadata Hdf5Reader::read_metadata() const {
    if (!is_open())
        throw std::runtime_error("Hdf5Reader: file is not open");

    H5Handle ds(H5Dopen2(file_, "Metadata", H5P_DEFAULT), H5Dclose);
    H5Handle sp(H5Dget_space(ds.get()), H5Sclose);

    const int rank = H5Sget_simple_extent_ndims(sp.get());
    if (rank != 1) {
        throw std::runtime_error(
            "Hdf5Reader: Metadata dataset must be one-dimensional");
    }

    hsize_t dims[1];
    check_hdf5(H5Sget_simple_extent_dims(sp.get(), dims, nullptr),
               "Hdf5Reader: failed to read metadata dimensions");

    std::string meta_str(checked_size(dims[0], "metadata length"), '\0');
    check_hdf5(H5Dread(ds.get(),
                       H5T_NATIVE_CHAR,
                       H5S_ALL,
                       H5S_ALL,
                       H5P_DEFAULT,
                       meta_str.data()),
               "Hdf5Reader: failed to read metadata");

    return Metadata::from_bytes(meta_str);
}

std::vector<double> Hdf5Reader::read_accform() const {
    if (!is_open())
        throw std::runtime_error("Hdf5Reader: file is not open");

    std::size_t npts_val = get_npts();
    std::size_t ch_val = get_channel_num();

    H5Handle ds(H5Dopen2(file_, "acceleration", H5P_DEFAULT), H5Dclose);
    H5Handle sp(H5Dget_space(ds.get()), H5Sclose);

    const int rank = H5Sget_simple_extent_ndims(sp.get());
    if (rank != 2) {
        throw std::runtime_error(
            "Hdf5Reader: acceleration dataset must be two-dimensional");
    }
    hsize_t dims[2];
    check_hdf5(H5Sget_simple_extent_dims(sp.get(), dims, nullptr),
               "Hdf5Reader: failed to read acceleration dimensions");
    const auto rows = checked_size(dims[0], "acceleration row count");
    const auto columns = checked_size(dims[1], "acceleration column count");
    if (rows != npts_val || columns != ch_val) {
        std::ostringstream oss;
        oss << "Hdf5Reader: acceleration dimensions [" << rows << ", "
            << columns << "] do not match attributes npts=" << npts_val
            << ", channel_num=" << ch_val;
        throw std::runtime_error(oss.str());
    }

    std::vector<double> time_major(checked_value_count(npts_val, ch_val));
    check_hdf5(H5Dread(ds.get(),
                       H5T_NATIVE_DOUBLE,
                       H5S_ALL,
                       H5S_ALL,
                       H5P_DEFAULT,
                       time_major.data()),
               "Hdf5Reader: failed to read acceleration data");

    // Transpose from time-sequential (HDF5 storage) back to
    // channel-sequential (qrest_data internal format)
    std::vector<double> result(checked_value_count(npts_val, ch_val));
    for (std::size_t c = 0; c < ch_val; ++c) {
        for (std::size_t r = 0; r < npts_val; ++r) {
            result[c * npts_val + r] = time_major[r * ch_val + c];
        }
    }

    return result;
}

std::size_t Hdf5Reader::get_npts() const {
    if (!is_open())
        throw std::runtime_error("Hdf5Reader: file is not open");

    H5Handle attr(H5Aopen(file_, "npts", H5P_DEFAULT), H5Aclose);
    unsigned long long val;
    check_hdf5(H5Aread(attr.get(), H5T_NATIVE_ULLONG, &val),
               "Hdf5Reader: failed to read npts attribute");
    if (val > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Hdf5Reader: npts attribute is too large");
    }
    return static_cast<std::size_t>(val);
}

std::size_t Hdf5Reader::get_channel_num() const {
    if (!is_open())
        throw std::runtime_error("Hdf5Reader: file is not open");

    H5Handle attr(H5Aopen(file_, "channel_num", H5P_DEFAULT), H5Aclose);
    unsigned long long val;
    check_hdf5(H5Aread(attr.get(), H5T_NATIVE_ULLONG, &val),
               "Hdf5Reader: failed to read channel_num attribute");
    if (val > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            "Hdf5Reader: channel_num attribute is too large");
    }
    return static_cast<std::size_t>(val);
}

void Hdf5Reader::close() {
    if (file_ >= 0) {
        H5Fclose(file_);
        file_ = H5I_INVALID_HID;
    }
}

} // namespace qrest_data
