target("qrest_data_hdf5")
    set_kind("shared")
    add_files("./*.cpp")
    add_packages("hdf5")
    add_syslinks("hdf5_cpp")

target("test_qrest_data_hdf5")
    set_kind("binary")
    add_files("./test_*.cpp")
    add_deps("qrest_data_hdf5")
    add_packages("hdf5")
    add_syslinks("hdf5_cpp")

