target("test_qrest_data_lib")
    set_kind("binary")
    add_files("./*.cpp")
    add_deps("qrest_data_lib")