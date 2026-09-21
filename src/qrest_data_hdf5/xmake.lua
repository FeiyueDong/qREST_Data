target("qrest_data_hdf5")
    set_kind("static")
    set_languages("c++20")
    add_files("hdf5_reader.cpp")
    add_files("hdf5_writer.cpp")
    add_deps("qrest_data_headers", {public = true})
    add_packages("hdf5", "nlohmann_json")
    if is_plat("linux", "macosx", "mingw") then
        add_cxflags("-fPIC")
    end
