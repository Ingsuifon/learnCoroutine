add_rules("mode.debug", "mode.release")

target("coroutine")
    set_languages("c++2b")
    set_kind("binary")
    add_files("src/*.cpp")
    add_includedirs("src/co_async")