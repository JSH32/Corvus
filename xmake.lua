add_rules("mode.release", "mode.debug")
set_languages("c++23")
    
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
        options = {
            -- {'p', ""}
        }
    }
task_end()

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

add_requires("glfw")
add_requires("physfs")
add_requires("spdlog v1.11.0")
add_requires("raylib 5.5")
add_requires("entt v3.9.0")
add_requires("imgui v1.91.9b-docking")

target("linp-core")
    set_kind("static")
    add_includedirs(
        "core/include",
        "vendor/rlImGui",
        "vendor/rlImGui/extras",
        "vendor/raylib-cpp/include",
        { public = true }
    )
    -- On windows needed for physfs
    add_links("Advapi32")
    add_packages("glfw", "raylib", "imgui", "entt", "spdlog", "physfs", { public = true })
    add_files("vendor/rlImGui/rlImGui.cpp", "core/src/**.cpp")
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
