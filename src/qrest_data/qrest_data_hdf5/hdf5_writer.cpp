#include "hdf5_writer.hpp"

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
            throw std::runtime_error("HDF5: failed to create object");
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

std::size_t checked_value_count(std::size_t rows, std::size_t columns) {
    if (columns != 0
        && rows > std::numeric_limits<std::size_t>::max() / columns) {
        throw std::runtime_error(
            "Hdf5Writer: acceleration dimensions overflow");
    }
    return rows * columns;
}

} // namespace

Hdf5Writer::Hdf5Writer(Hdf5Writer &&other) noexcept
    : file_(std::exchange(other.file_, H5I_INVALID_HID)) {}

Hdf5Writer &Hdf5Writer::operator=(Hdf5Writer &&other) noexcept {
    if (this != &other) {
        close();
        file_ = std::exchange(other.file_, H5I_INVALID_HID);
    }
    return *this;
}

void Hdf5Writer::open(const std::string &filename) {
    close();
    file_ =
        H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_ < 0)
        throw std::runtime_error("Hdf5Writer: failed to create file");
}

void Hdf5Writer::write(const Metadata &metadata,
                       const std::vector<double> &data,
                       std::size_t npts,
                       std::size_t channel_num) {
    if (!is_open())
        throw std::runtime_error("Hdf5Writer: file is not open");

    const auto expected_values = checked_value_count(npts, channel_num);
    if (data.size() != expected_values) {
        std::ostringstream oss;
        oss << "Hdf5Writer: expected " << expected_values
            << " channel-sequential values, got " << data.size();
        throw std::runtime_error(oss.str());
    }

    // 1. Write metadata as a 1D char array dataset (serialized JSON)
    {
        std::string meta_str = metadata.to_bytes();
        hsize_t dims[1] = {static_cast<hsize_t>(meta_str.size())};
        H5Handle ds(H5Screate_simple(1, dims, nullptr), H5Sclose);
        H5Handle meta_ds(H5Dcreate2(file_,
                                    "Metadata",
                                    H5T_NATIVE_CHAR,
                                    ds.get(),
                                    H5P_DEFAULT,
                                    H5P_DEFAULT,
                                    H5P_DEFAULT),
                         H5Dclose);
        check_hdf5(H5Dwrite(meta_ds.get(),
                            H5T_NATIVE_CHAR,
                            H5S_ALL,
                            H5S_ALL,
                            H5P_DEFAULT,
                            meta_str.data()),
                   "Hdf5Writer: failed to write metadata");
    }

    // 2. Write npts and channel_num as scalar attributes for quick access
    {
        H5Handle scalar(H5Screate(H5S_SCALAR), H5Sclose);

        unsigned long long npts_val = npts;
        H5Handle npts_attr(H5Acreate2(file_,
                                      "npts",
                                      H5T_NATIVE_ULLONG,
                                      scalar.get(),
                                      H5P_DEFAULT,
                                      H5P_DEFAULT),
                           H5Aclose);
        check_hdf5(H5Awrite(npts_attr.get(), H5T_NATIVE_ULLONG, &npts_val),
                   "Hdf5Writer: failed to write npts attribute");

        unsigned long long ch_val = channel_num;
        H5Handle ch_attr(H5Acreate2(file_,
                                    "channel_num",
                                    H5T_NATIVE_ULLONG,
                                    scalar.get(),
                                    H5P_DEFAULT,
                                    H5P_DEFAULT),
                         H5Aclose);
        check_hdf5(H5Awrite(ch_attr.get(), H5T_NATIVE_ULLONG, &ch_val),
                   "Hdf5Writer: failed to write channel_num attribute");
    }

    // 3. Write data as a 2D dataset [npts][channel_num]
    //    HDF5 stores in row-major order: rows = time steps, cols = channels
    //    Input data is channel-sequential (ch1_all, ch2_all, ...);
    //    we transpose to time-sequential for storage.
    {
        hsize_t dims[2] = {static_cast<hsize_t>(npts),
                           static_cast<hsize_t>(channel_num)};
        H5Handle ds(H5Screate_simple(2, dims, nullptr), H5Sclose);
        H5Handle wave_ds(H5Dcreate2(file_,
                                    "acceleration",
                                    H5T_NATIVE_DOUBLE,
                                    ds.get(),
                                    H5P_DEFAULT,
                                    H5P_DEFAULT,
                                    H5P_DEFAULT),
                         H5Dclose);

        std::vector<double> time_major(expected_values);
        for (std::size_t c = 0; c < channel_num; ++c) {
            for (std::size_t r = 0; r < npts; ++r) {
                time_major[r * channel_num + c] = data[c * npts + r];
            }
        }

        check_hdf5(H5Dwrite(wave_ds.get(),
                            H5T_NATIVE_DOUBLE,
                            H5S_ALL,
                            H5S_ALL,
                            H5P_DEFAULT,
                            time_major.data()),
                   "Hdf5Writer: failed to write acceleration data");
    }
}

void Hdf5Writer::close() {
    if (file_ >= 0) {
        H5Fclose(file_);
        file_ = H5I_INVALID_HID;
    }
}

} // namespace qrest_data
