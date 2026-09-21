set_project("qrest_data_installed_sdk_smoke")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

option("qrest_data_sdk")
    set_showmenu(true)
    set_description("Path to an installed qrest_data SDK")
option_end()

target("installed_sdk_smoke")
    set_kind("binary")
    add_files("main.cpp")
    on_load(function (target)
        local sdk_root = get_config("qrest_data_sdk")
        assert(sdk_root and #sdk_root > 0,
               "configure with --qrest_data_sdk=/path/to/installed/sdk")
        target:add("includedirs", path.join(sdk_root, "include"))
        target:add("linkdirs", path.join(sdk_root, "lib"))
        target:add("links", "qrest_data_lib")
        if target:is_plat("linux") then
            target:add("rpathdirs", path.join(sdk_root, "lib"))
        end
    end)
