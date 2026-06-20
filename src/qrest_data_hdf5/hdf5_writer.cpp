#include "hdf5_writer.hpp"

namespace qrest_data
{

void Hdf5Writer::open(const std::string &filename)
{
    close();
    file_ = std::make_unique<H5::H5File>(filename, H5F_ACC_TRUNC);
}

void Hdf5Writer::write(const Metadata &metadata,
                       const std::vector<double> &data,
                       std::size_t npts,
                       std::size_t channel_num)
{
    if (!file_)
        throw std::runtime_error("Hdf5Writer: file is not open");

    // 1. Write metadata as a 1D char array dataset (serialized JSON)
    {
        std::string meta_str = metadata.to_bytes();
        hsize_t dims[1] = {meta_str.size()};
        H5::DataSpace ds(1, dims);
        H5::DataSet meta_ds =
            file_->createDataSet("Metadata", H5::PredType::NATIVE_CHAR, ds);
        meta_ds.write(meta_str.data(), H5::PredType::NATIVE_CHAR);
    }

    // 2. Write npts and channel_num as scalar attributes for quick access
    {
        H5::DataSpace scalar(H5S_SCALAR);

        unsigned long long npts_val = npts;
        H5::Attribute npts_attr =
            file_->createAttribute("npts", H5::PredType::NATIVE_ULLONG, scalar);
        npts_attr.write(H5::PredType::NATIVE_ULLONG, &npts_val);

        unsigned long long ch_val = channel_num;
        H5::Attribute ch_attr = file_->createAttribute(
            "channel_num", H5::PredType::NATIVE_ULLONG, scalar);
        ch_attr.write(H5::PredType::NATIVE_ULLONG, &ch_val);
    }

    // 3. Write data as a 2D dataset [npts][channel_num]
    //    HDF5 stores in row-major order: rows = time steps, cols = channels
    //    Input data is channel-sequential (ch1_all, ch2_all, ...);
    //    we transpose to time-sequential for storage.
    {
        hsize_t dims[2] = {npts, channel_num};
        H5::DataSpace ds(2, dims);
        H5::DataSet wave_ds = file_->createDataSet(
            "acceleration", H5::PredType::NATIVE_DOUBLE, ds);

        std::vector<double> time_major(npts * channel_num);
        for (std::size_t c = 0; c < channel_num; ++c)
        {
            for (std::size_t r = 0; r < npts; ++r)
            {
                time_major[r * channel_num + c] = data[c * npts + r];
            }
        }

        wave_ds.write(time_major.data(), H5::PredType::NATIVE_DOUBLE);
    }
}

void Hdf5Writer::close() { file_.reset(); }

} // namespace qrest_data
