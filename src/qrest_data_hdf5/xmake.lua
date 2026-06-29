target("qrest_data_hdf5")
    set_kind("static")
    add_files("./*.cpp")

target("test_qrest_data_hdf5")
    set_kind("binary")
    add_files("./test_*.cpp")
    add_deps("qrest_data_hdf5")
    add_packages("hdf5")

