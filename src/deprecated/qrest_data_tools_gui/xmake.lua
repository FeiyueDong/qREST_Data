target("qrest_data_tools_gui")
    add_rules("qt.quickapp")
    set_languages("c++20")
    add_frameworks("QtConcurrent")
    add_files("./*.cpp")
    add_files("./*.h")
    add_files("./qml.qrc")
    add_deps("qrest_data_lib")
    add_deps("qrest_data_tools_core")
    add_packages("hdf5", "nlohmann_json")
    if is_plat("linux") then
        add_rpathdirs("$ORIGIN/../lib")
    end
    set_group("data_tools")
