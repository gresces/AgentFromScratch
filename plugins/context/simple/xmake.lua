local core = os.getenv("CORE") or path.absolute("../../../core")

add_rules("mode.debug", "mode.release")
set_targetdir("$(projectdir)")

set_defaultmode("release")

target("context")
    set_kind("shared")
    set_languages("c++23")
    set_filename("ContextPluginSimple")
    add_includedirs(core .. "/include")
    add_files("context.cpp")
    add_cxxflags("-fPIC", "-fvisibility=hidden")
