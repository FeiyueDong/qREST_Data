local qrest_data_root = path.normalize(path.join(os.scriptdir(), ".."))
local qrest_data_version = "1.1.1"
local qrest_data_standalone =
    path.absolute(os.projectdir()) == path.absolute(qrest_data_root)

add_requires("nlohmann_json")
add_requires("cli11")
add_requires("hdf5")

option("qrest_data_enable_gui")
    set_default(qrest_data_standalone)
    set_showmenu(true)
    set_description("Build the qrest_data Qt Quick GUI")
option_end()

target("qrest_data_headers")
    set_kind("headeronly")
    -- This target-local version keeps the generated header independent of a
    -- parent project's version. The standalone entry validates the match.
    set_version(qrest_data_version)
    set_languages("c++20")
    set_configdir(path.join("$(builddir)", "generated"))
    add_configfiles(path.join(qrest_data_root, "config/version.h.in"), {
        filename = "version.h",
        prefixdir = "qrest_data"
    })
    add_headerfiles(path.join(qrest_data_root, "include/qrest_data/*.hpp"),
                    {install = false})
    add_includedirs(path.join(qrest_data_root, "include"), {public = true})
    add_includedirs(path.join("$(builddir)", "generated"), {public = true})
    add_packages("nlohmann_json", {public = true})
    add_installfiles(path.join(qrest_data_root, "include/qrest_data/*.hpp"), {
        prefixdir = "include/qrest_data"
    })
    add_installfiles(path.join(qrest_data_root, "include/qrest_data/qrest_data.h"), {
        prefixdir = "include/qrest_data"
    })
    add_installfiles(path.join("$(builddir)", "generated/qrest_data/version.h"), {
        prefixdir = "include/qrest_data"
    })

includes(path.join(qrest_data_root, "src/qrest_data_lib"))
includes(path.join(qrest_data_root, "src/qrest_data_hdf5"))
includes(path.join(qrest_data_root, "src/qrest_data_tools"))

for _, name in ipairs({"qrest_data_lib", "qrest_data_hdf5",
                      "qrest_data_import_formats", "qrest_data_tools_core",
                      "qrest_data_tools_cli"}) do
    target(name)
        set_version(qrest_data_version)
        if is_plat("windows") then
            add_cxflags("/utf-8")
        end
end

local gui_enabled = get_config("qrest_data_enable_gui")
if gui_enabled == nil then
    gui_enabled = qrest_data_standalone
end
if gui_enabled then
    includes(path.join(qrest_data_root, "src/qrest_data_tools/gui"))
    target("qrest_data_tools_gui")
        set_version(qrest_data_version)
        if is_plat("windows") then
            add_cxflags("/utf-8")
        end
end
