add_rules("mode.release", "mode.debug")
add_rules("plugin.compile_commands.autoupdate")
set_languages("c++20")
-- GLM uses a lot of ram, temporary to avoid compile ram usage
add_defines(
    "GLM_FORCE_INLINE",
    "GLM_FORCE_SIZE_T_LENGTH",
    "GLM_FORCE_EXPLICIT_CTOR", 
    "GLM_FORCE_NO_CTOR_INIT"
)

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
add_requires("entt v3.9.0")
add_requires("imgui v1.91.9b-docking")
add_requires("boost")

local project_dir = os.projectdir()

target("physfs")
    set_kind("static")
    add_includedirs("vendor/physfs/src", { public = true })
    add_files(
        "vendor/physfs/src/*.cpp",
        "vendor/physfs/src/*.c"
    )

target("ImGuiFileDialog")
    set_kind("static")
    add_includedirs("vendor/ImGuiFileDialog", { public = true })
    add_files("vendor/ImGuiFileDialog/*.cpp")
    add_packages("imgui")

target("tinyobjloader")
    set_kind("static")
    add_includedirs("vendor/tinyobjloader", { public = true })
    add_files("vendor/tinyobjloader/tiny_obj_loader.cc")

target("stb")
    set_kind("static")
    add_includedirs("vendor/stb", { public = true })
    add_files("vendor/stb/*.c")

target("fontawesome")
    set_kind("headeronly")
    add_includedirs("vendor/fontawesome", { public = true })

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
    add_deps("tinyobjloader", { public = true })
    add_deps("physfs", { public = true })
    add_deps("ImGuiFileDialog", { public = true })
    add_deps("fontawesome", { public = true })
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
