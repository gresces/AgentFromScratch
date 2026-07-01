local core = os.getenv("CORE") or path.absolute("../../../core")

add_rules("mode.debug", "mode.release")
set_targetdir("$(projectdir)")
set_defaultmode("release")

add_requires("boost_sml", "nlohmann_json")

target("loop")
    set_kind("shared")
    set_languages("c++23")
    set_filename("LoopPluginSimple")
    add_includedirs(core .. "/include")
    add_files("loop.cpp")
    add_packages("boost_sml", "nlohmann_json")
    add_cxxflags("-fPIC", "-fvisibility=hidden")
