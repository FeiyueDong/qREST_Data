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
        local expected_targets = {
            qrest_data_headers = true,
            qrest_data_lib = true,
            qrest_data_hdf5 = true,
            qrest_data_import_formats = true,
            qrest_data_tools_core = true,
            qrest_data_tools_cli = true,
            products_smoke = true
        }
        for name in pairs(project.targets()) do
            assert(expected_targets[name],
                   "Unexpected target in products entry: " .. name)
        end
        for name in pairs(expected_targets) do
            assert(project.target(name), "Missing product target: " .. name)
        end
        assert(target:dep("qrest_data_lib"):version() ~= project.version(),
               "qrest_data_lib inherited the parent project version")
    end)
