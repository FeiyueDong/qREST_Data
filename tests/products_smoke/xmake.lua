set_project("qrest_data_products_smoke")
set_version("9.9.9")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

includes("../../xmake/products.lua")

target("products_smoke")
    set_kind("binary")
    add_files("main.cpp")
    add_deps("qrest_data_lib")
    after_load(function (target)
        import("core.project.project")
        assert(target:dep("qrest_data_lib"):version() ~= project.version(),
               "qrest_data_lib inherited the parent project version")
    end)
