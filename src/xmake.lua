target("qrest_data_core")
    set_kind("headeronly")
    add_headerfiles("$(projectdir)/include/qrest_data/*.hpp", {install = false})
    add_includedirs("$(projectdir)/include", {public = true})
    add_includedirs("$(builddir)/generated", {public = true})
    add_packages("nlohmann_json", {public = true})
    add_installfiles("$(projectdir)/include/qrest_data/*.hpp", {
        prefixdir = "include/qrest_data"
    })
    add_installfiles("$(projectdir)/include/qrest_data/qrest_data.h", {
        prefixdir = "include/qrest_data"
    })
    add_installfiles("$(builddir)/generated/qrest_data/version.h", {
        prefixdir = "include/qrest_data"
    })

includes("qrest_data_lib")
includes("test_qrest_data_lib")
includes("qrest_data_hdf5")

includes("qrest_data_tools")
