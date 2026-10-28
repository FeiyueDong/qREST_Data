target("test_qrest_data_import_formats")
    set_kind("binary")
    add_files("test_import_formats.cpp")
    add_deps("qrest_data_tools_core")
    add_deps("qrest_data_import_formats")
    add_packages("hdf5", "nlohmann_json")
    set_rundir("$(projectdir)")

target("test_qrest_data_dataset_draft")
    set_kind("binary")
    add_files("test_dataset_draft.cpp")
    add_deps("qrest_data_tools_core")
    if is_plat("linux") then add_syslinks("pthread") end
    set_rundir("$(projectdir)")

target("test_qrest_data_format_readers")
    set_kind("binary")
    add_files("test_format_readers.cpp")
    add_deps("qrest_data_tools_core", "qrest_data_readers")
    set_rundir("$(projectdir)")

target("test_qrest_data_channel_import")
    set_kind("binary")
    add_files("test_channel_import.cpp")
    add_deps("qrest_data_tools_core")
    set_rundir("$(projectdir)")

target("test_qrest_data_utf8_paths")
    set_kind("binary")
    add_files("test_utf8_paths.cpp")
    add_deps("qrest_data_tools_core", "qrest_data_readers")
    if is_plat("windows") then add_cxflags("/utf-8") end
    set_rundir("$(projectdir)")
