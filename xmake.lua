add_rules("mode.release", "mode.debug")
add_rules("plugin.compile_commands.autoupdate")
set_languages("c++17", "c++23")

task("submodule")
    on_run(function ()
        os.execv("git", {"submodule", "update", "--init", "--recursive"}, {stdout = outfile, stderr = errfile})    
    end)
    set_menu {
        usage = "xmake submodule",
        description = "Fetch all external dependencies",
        options = { }
    }

task("configure")
    on_run(function ()        
        os.execv("xmake", {"project", "-k", "compile_commands"}, {stdout = outfile, stderr = errfile})
        os.execv("xmake", {"project", "-k", "cmakelists"}, {stdout = outfile, stderr = errfile})
    end)
    set_menu {
        usage = "xmake configure",
        description = "Configure the project and generate CMake files",
        options = { }
    }
task_end()

add_requires("glfw")
add_requires("spdlog v1.11.0")
add_requires("raylib 5.5")
add_requires("entt v3.9.0")
add_requires("imgui v1.91.9b-docking")

package("physfs")
    add_deps("cmake")
    set_sourcedir(path.join(os.scriptdir(), "vendor", "physfs"))
    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        import("package.tools.cmake").install(package, configs)
    end)
package_end()

add_requires("physfs")

target("rlimgui")
    set_kind("static")
    
    add_includedirs("vendor/rlImGui", {public = true})
    add_includedirs("vendor/rlImGui/extras", {public = true})
    
    add_files("vendor/rlImGui/*.cpp")
    add_headerfiles("vendor/rlImGui/*.h")
    add_headerfiles("vendor/rlImGui/extras/*.h")
    
    add_packages("raylib", {public = true})
    add_packages("imgui", {public = true})

-- package("raylib-cpp")
--     set_sourcedir(path.join(os.scriptdir(), "vendor", "raylib-cpp"))
--     on_install(function (package)
--         os.cp("include/*.hpp", package:installdir("include"))
--     end)
--     add_deps("raylib")
-- package_end()

target("raylib-cpp")
    set_kind("headeronly")
    
    add_headerfiles("vendor/raylib-cpp/include/*.hpp")
    add_includedirs("vendor/raylib-cpp/include", {public = true})
    
    add_packages("raylib", {public = true})

-- add_requires("rlimgui", { system = false, debug = is_mode("debug") })
-- add_requires("raylib-cpp", { system = false, debug = is_mode("debug") })

target("linp-core")
    set_kind("static")
    add_includedirs(
        "core/include",
        { public = true }
    )
    add_headerfiles("core/include")

    -- On windows needed for physfs
    if is_plat("windows") then
        add_links("Advapi32")
    end

    -- xrepo packages
    add_packages("glfw", { public = true })
    add_packages("raylib", { public = true })
    add_packages("imgui", { public = true })
    add_packages("entt", { public = true })
    add_packages("spdlog", { public = true })
    -- Manual packages
    add_packages("physfs", { public = true })
    add_deps("rlimgui", { public = true })
    add_deps("raylib-cpp", { public = true })

    add_files("core/src/**.cpp")

    -- Dealing with engine resources for PsysFS to read.
    after_build(function(target)
        local target_dir = path.directory(target:targetfile())
        os.rm(target_dir .. "/engine.zip")
        
        -- Directly create a zip archive in the target directory
        if is_plat("windows") then
            os.exec("powershell Compress-Archive -Path 'resources\\*' -DestinationPath '" .. target_dir .. "\\engine.zip'")
        else
            os.exec("zip -r " .. target_dir .. "/engine.zip resources")
        end
    end)
    after_clean(function(target)
        local target_dir = path.directory(target:targetfile())
        os.rm(target_dir .. "/engine.zip")
    end)

target("linp-editor")
    set_kind("binary")
    add_deps("linp-core")
    add_includedirs(
        "editor/include"
    )
    add_files(
        "editor/src/**.cpp"
    )
