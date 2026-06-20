#include "hdf5_reader.hpp"

namespace qrest_data
{

void Hdf5Reader::open(const std::string &filename)
{
    close();
    file_ = std::make_unique<H5::H5File>(filename, H5F_ACC_RDONLY);
}

Metadata Hdf5Reader::read_metadata() const
{
    if (!file_)
        throw std::runtime_error("Hdf5Reader: file is not open");

    H5::DataSet ds = file_->openDataSet("Metadata");
    H5::DataSpace sp = ds.getSpace();

    hsize_t dims[1];
    sp.getSimpleExtentDims(dims);

    std::string meta_str(static_cast<std::size_t>(dims[0]), '\0');
    ds.read(meta_str.data(), H5::PredType::NATIVE_CHAR);

    return Metadata::from_bytes(meta_str);
}

std::vector<double> Hdf5Reader::read_accform() const
{
    if (!file_)
        throw std::runtime_error("Hdf5Reader: file is not open");

    std::size_t npts_val = get_npts();
    std::size_t ch_val = get_channel_num();

    H5::DataSet ds = file_->openDataSet("acceleration");

    std::vector<double> time_major(npts_val * ch_val);
    ds.read(time_major.data(), H5::PredType::NATIVE_DOUBLE);

    // Transpose from time-sequential (HDF5 storage) back to
    // channel-sequential (qrest_data internal format)
    std::vector<double> result(npts_val * ch_val);
    for (std::size_t c = 0; c < ch_val; ++c)
    {
        for (std::size_t r = 0; r < npts_val; ++r)
        {
            result[c * npts_val + r] = time_major[r * ch_val + c];
        }
    }

    return result;
}

std::size_t Hdf5Reader::get_npts() const
{
    if (!file_)
        throw std::runtime_error("Hdf5Reader: file is not open");

    H5::Attribute attr = file_->openAttribute("npts");
    unsigned long long val;
    attr.read(H5::PredType::NATIVE_ULLONG, &val);
    return static_cast<std::size_t>(val);
}

std::size_t Hdf5Reader::get_channel_num() const
{
    if (!file_)
        throw std::runtime_error("Hdf5Reader: file is not open");

    H5::Attribute attr = file_->openAttribute("channel_num");
    unsigned long long val;
    attr.read(H5::PredType::NATIVE_ULLONG, &val);
    return static_cast<std::size_t>(val);
}

void Hdf5Reader::close() { file_.reset(); }

} // namespace qrest_data
