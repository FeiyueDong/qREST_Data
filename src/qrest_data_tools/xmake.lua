target("qrest_data_import_formats")
    set_kind("static")
    set_languages("c++20")
    add_files("formats/mseed/modified_mseed.cpp")
    add_files("formats/mseed/steim2.cpp")
    add_files("formats/sit/sit_dat_reader.cpp")
    add_files("formats/sit/sit_txt_reader.cpp")
    add_files("formats/mseed/modified_mseed_export.cpp")
    add_files("formats/tdms/tdms_reader.cpp")
    add_files("formats/tdms/tdms_export.cpp")
    add_includedirs("formats/mseed", "formats/tdms", {public = true})
    add_packages("nlohmann_json")
    if is_plat("linux", "macosx", "mingw") then
        add_cxflags("-fPIC")
    end
    set_group("data_tools")

target("qrest_data_tools_core")
    set_kind("static")
    set_languages("c++20")
    add_files("core/qrest_file.cpp")
    add_files("core/text_matrix.cpp")
    add_files("core/validation.cpp")
    add_files("core/external_import.cpp")
    add_files("core/dataset_draft.cpp")
    add_includedirs("core", {public = true})
    add_deps("qrest_data_import_formats")
    add_deps("qrest_data_hdf5")
    add_deps("qrest_data_headers", {public = true})
    add_packages("hdf5", "nlohmann_json")
    if is_plat("linux", "macosx", "mingw") then
        add_cxflags("-fPIC")
    end
    set_group("data_tools")

target("qrest_data_tools_cli")
    set_kind("binary")
    set_languages("c++20")
    add_files("cli/qrest_data_tools_cli.cpp")
    add_deps("qrest_data_tools_core")
    add_packages("hdf5", "nlohmann_json", "cli11")
    if is_plat("linux") then
        add_rpathdirs("$ORIGIN/../lib")
    end
    set_group("data_tools")

target("qrest_data_readers")
    set_kind("shared")
    set_languages("c++20")
    add_files("core/external_reader_c.cpp")
    add_defines("QREST_DATA_READERS_BUILD_SHARED")
    add_deps("qrest_data_tools_core")
    add_deps("qrest_data_headers", {public = true})
    if is_plat("linux") then
        add_rpathdirs("$ORIGIN")
    end
    set_group("data_tools")
