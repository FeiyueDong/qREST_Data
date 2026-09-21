set_project("qrest_data")
set_version("1.1.1")
set_xmakever("3.0.5")
set_warnings("all")
set_allowedplats("windows", "linux", "macosx", "mingw")

add_rules("mode.debug", "mode.release")
set_languages("c++20")

if is_plat("windows") then
    set_toolchains("msvc")
    add_cxflags("/utf-8")
elseif is_plat("mingw") then
    local msys2_root = os.getenv("MSYS2_ROOT")
    if msys2_root and #msys2_root > 0 then
        set_config("sdk", msys2_root)
    end
    set_toolchains("gcc")
end

if is_plat("linux", "macosx", "mingw") then
    add_cxflags("-fPIC")
end

rule("qrest_data.validate_standalone_version")
    on_load(function (target)
        import("core.project.project")
        assert(target:version() == project.version(),
               "qrest_data product version must match the standalone project version")
    end)
rule_end()

includes("xmake/products.lua")
target("qrest_data_headers")
    add_rules("qrest_data.validate_standalone_version")
includes("src")
