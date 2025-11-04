add_rules("mode.release", "mode.debug")
add_rules("plugin.compile_commands.autoupdate")
set_languages("c++20")

-- Global debug symbols configuration
if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
    set_warnings("all")
end

if is_mode("release") then
    set_symbols("debug")  -- Keep symbols in release
    set_strip("none")     -- Don't strip
    set_optimize("none")
end

task("submodule")
    on_run(function ()
        os.execv("git", {"submodule", "update", "--init", "--recursive"}, { stdout = outfile, stderr = errfile })    
    end)
    set_menu {
        usage = "xmake submodule",
        description = "Fetch all external dependencies",
        options = { }
    }

task("configure")
    on_run(function ()        
        os.execv("xmake", {"project", "-k", "compile_commands"}, { stdout = outfile, stderr = errfile })
        os.execv("xmake", {"project", "-k", "cmakelists"}, { stdout = outfile, stderr = errfile })
    end)
    set_menu {
        usage = "xmake configure",
        description = "Configure the project and generate CMake files",
        options = { }
    }
task_end()

-- xrepo packages
add_requires("glfw")
add_requires("glad")
add_requires("glm")
add_requires("cereal")
add_requires("spdlog v1.11.0")
-- add_requires("raylib 5.5")
add_requires("entt v3.9.0")
add_requires("imgui v1.91.9b-docking")
add_requires("boost")

local project_dir = os.projectdir()

package("physfs")
    add_deps("cmake")
    set_sourcedir(path.join(project_dir, "vendor", "physfs"))
    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        import("package.tools.cmake").install(package, configs)
    end)
package_end()

target("ImGuiFileDialog")
    set_kind("static")
    add_includedirs("vendor/ImGuiFileDialog", { public = true })
    add_files("vendor/ImGuiFileDialog/*.cpp")
    -- Deps
    add_packages("imgui")

package("tinyobjloader")
    add_deps("cmake")
    set_sourcedir(path.join(project_dir, "vendor", "tinyobjloader"))
    on_install(function (package)
        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        import("package.tools.cmake").install(package, configs)
    end)
package_end()

add_requires("physfs", { system = false })
add_requires("tinyobjloader", { system = false })

target("stb")
    set_kind("static")
    add_includedirs("vendor/stb", { public = true })
    add_files("vendor/stb/*.c")


target("fontawesome")
    set_kind("headeronly")
    add_includedirs("vendor/fontawesome", { public = true })

-- target("rlimgui")
--     set_kind("static")
--     add_includedirs("vendor/rlImGui", { public = true })
--     add_includedirs("vendor/rlImGui/extras", { public = true })
--     add_files("vendor/rlImGui/*.cpp")
--     -- Deps
--     add_packages("raylib")
--     add_packages("imgui")

-- target("raylib-cpp")
--     set_kind("headeronly")
--     add_includedirs("vendor/raylib-cpp/include", { public = true })
--     -- Deps
--     add_packages("raylib")

-- raylib as submodule target
-- target("raylib")
    -- set_kind("static") 
    -- set_languages("c99")

    -- add_includedirs("vendor/raylib/src", { public = true })
    -- add_files("vendor/raylib/src/*.c")

    -- remove_files("vendor/raylib/src/external/glad.c")
    -- remove_files("vendor/raylib/src/external/glfw3/*.c")
    -- remove_files("vendor/raylib/src/external/glfw3/*.m")
    -- remove_files("vendor/raylib/src/external/glfw3/*.mm")

    -- add_defines("RAYLIB_NO_GLAD", "GRAPHICS_API_OPENGL_33")
    -- add_packages("glad", "glfw")

    -- -- Required system libs depending on platform
    -- if is_plat("linux") then
    --     add_syslinks("m", "dl", "pthread", "rt", "X11")
    -- elseif is_plat("macosx") then
    --     add_frameworks("Cocoa", "OpenGL", "IOKit", "CoreVideo")
    -- elseif is_plat("windows") then
    --     add_syslinks("gdi32", "winmm", "user32", "shell32")
    -- end

    -- add_defines("GRAPHICS_API_OPENGL_33", "RAYLIB_NO_GLAD")

target("corvus-core")
    set_kind("static")
    add_includedirs(
        "core/include",
        { public = true }
    )

    -- On windows needed for physfs
    if is_plat("windows") then
        add_links("Advapi32")
    end

    -- TEMPORARY
    if is_plat("linux", "macosx") then
        add_ldflags("-Wl,--allow-multiple-definition", { force = true })
    end

    -- xrepo packages
    add_packages("glfw", { public = true })
    add_packages("glad", { public = true })
    add_packages("glm", { public = true })
    -- add_packages("raylib", { public = true })
    add_packages("boost", { public = true })
    add_packages("imgui", { public = true })
    add_packages("entt", { public = true })
    add_packages("spdlog", { public = true })
    add_packages("cereal", { public = true })
    -- Manual packages
    add_packages("physfs", { public = true })
    add_packages("tinyobjloader", { public = true })
    -- add_deps("rlimgui", { public = true })
    add_deps("ImGuiFileDialog", { public = true })
    add_deps("fontawesome", { public = true })
    -- add_deps("raylib", { public = true })
    -- add_deps("raylib-cpp", { public = true })
    add_deps("stb", { public = true })

    add_files("core/src/**.cpp")

    if is_plat("linux") then
        add_defines("GLFW_EXPOSE_NATIVE_X11")
        add_links("X11")
    elseif is_plat("windows") then
        add_defines("GLFW_EXPOSE_NATIVE_WIN32")
    elseif is_plat("macosx") then
        add_defines("GLFW_EXPOSE_NATIVE_COCOA")
    end

    -- Dealing with engine resources for PsysFS to read.
    after_build(function(target)
        local target_dir = path.directory(target:targetfile())
        local zip_path = target_dir .. "/engine.zip"
        
        os.rm(zip_path)
        
        -- Directly create a zip archive in the target directory
        if is_plat("windows") then
            os.exec("powershell Compress-Archive -Path 'resources\\*' -DestinationPath '" .. zip_path .. "'")
        else
            -- os.exec("zip -r " .. target_dir .. "/engine.zip resources")
            os.cd("resources")
            os.exec("zip -r ../" .. zip_path .. " .")
            os.cd("..")
        end
    end)
    after_clean(function(target)
        local target_dir = path.directory(target:targetfile())
        os.rm(target_dir .. "/engine.zip")
    end)

target("corvus-editor")
    set_kind("binary")
    add_deps("corvus-core")
    add_includedirs(
        "editor/include"
    )
    add_files(
        "editor/src/**.cpp",
        "editor/src/**.c",
        "editor/src/**/**.cpp",
        "editor/src/**/**.c"
    )
