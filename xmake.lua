set_project("qrest_data")
set_version("1.0.0")
set_xmakever("3.0.5")
set_warnings("all")
set_allowedplats("windows", "linux", "macosx", "mingw")

add_rules("mode.debug", "mode.release")
set_config("plat", "mingw")
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

add_requires("nlohmann_json")
add_requires("cli11")
add_requires("hdf5")

if is_plat("linux", "macosx", "mingw") then
    add_cxflags("-fPIC")
end

includes("src")
