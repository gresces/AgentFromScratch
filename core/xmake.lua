local outputdir = "$(projectdir)/../bin"

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "$(projectdir)" })
set_targetdir(outputdir)
set_rundir(outputdir)

add_requires("nlohmann_json", "ftxui", "boost_sml", "taskflow", "cpr", "spdlog")

target("afs")
    set_kind("binary")
    set_languages("c++23")
    add_includedirs("src")
    add_includedirs("include")
    add_files("src/**.cc")
    add_syslinks("dl")
    add_ldflags("-rdynamic")
    add_packages("nlohmann_json", "ftxui", "boost_sml", "taskflow", "cpr", "spdlog")
    add_installfiles("include/(afs.hh)", {prefixdir = "include"})
    add_installfiles("include/(afs/**.hh)", {prefixdir = "include"})
