set_project("qrest_data")
set_version("1.0.0")
set_xmakever("3.0.5")
set_warnings("all")
set_allowedplats("windows", "linux", "macosx", "mingw", "msys")

add_rules("mode.debug", "mode.release")
set_config("plat", "mingw")
if is_plat("msys") then
    set_config("sdk", "C:/Programing/msys64/ucrt64")
    set_toolchains("gcc")
elseif is_plat("linux") then
    set_toolchains("gcc")
elseif is_plat("macosx") then
    set_toolchains("clang")
end

set_languages("c++20")

if is_plat("linux", "macosx") then
    add_requires("eigen", {system = true})
    add_requires("nlohmann-json", {system = true})
end

if is_plat("mingw") then
    set_targetdir("$(projectdir)/out/mingw",{ bindir = "bin", libdir = "lib" })
elseif is_plat("windows") then
    set_targetdir("$(projectdir)/out/windows",{ bindir = "bin", libdir = "lib" })
elseif is_plat("linux") then
    set_targetdir("$(projectdir)/out/linux",{ bindir = "bin", libdir = "lib" })
elseif is_plat("macosx") then
    set_targetdir("$(projectdir)/out/macosx",{ bindir = "bin", libdir = "lib" })
end

if is_plat("linux", "macosx", "mingw") then
    add_cxflags("-fPIC")
end

includes("src")
