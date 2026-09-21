target("qrest_data_lib")
    set_kind("shared")
    add_files("./*.cpp")
    add_packages("nlohmann_json")
