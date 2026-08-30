#ifndef QREST_DATA_HDF5_EXPORT_HPP
#define QREST_DATA_HDF5_EXPORT_HPP

#if defined(_WIN32) && defined(QREST_DATA_HDF5_EXPORTS)
#define QREST_DATA_HDF5_API __declspec(dllexport)
#else
#define QREST_DATA_HDF5_API
#endif

#endif // QREST_DATA_HDF5_EXPORT_HPP
