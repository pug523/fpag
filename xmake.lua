-- Copyright 2026 pugur
-- This source code is licensed under the Apache License, Version 2.0
-- which can be found in the LICENSE file.

set_project("fpag")
local project_version = "0.1.0"
set_version(project_version)

option("coverage", { default = false, description = "use llvm-cov for analyzing test coverage" })
option("xray", { default = false, description = "use llvm-xray for determining performance bottleneck" })
option("optreport", { default = false, description = "report optimization result" })
option("sanitizers", { default = false, description = "enable address/undefined behaviour/leak sanitizer" })
option("timetrace", { default = false, description = "generate timetrace json that can be see with perfetto ui" })
option("native", { default = false, description = "native architecture optimization" })
option("libunwind", { default = false, description = "use libunwind for stack tracing" })
option("fmtlib", { default = true, description = "use fmtlib for formatting (use std::format if false)" })
option("unitybuild", { default = false, description = "enable unity build to shorten build time" })
option("lto", { default = false, description = "use link time optimization on release builds" })
option("tests", { default = false, description = "build unit tests" })
option("benchmarks", { default = false, description = "build micro benchmarks" })
option("stdlib", { default = "libstdc++", description = "stl to use" })

set_policy("build.ccache", true)
set_policy("check.auto_ignore_flags", false)
set_policy("build.optimization.lto", has_config("lto"))
set_policy("build.c++.msvc.runtime", "MD")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.compile_commands.autoupdate", { outputdir = "out" })


set_targetdir("out/$(plat)-$(arch)-$(mode)")

local function is_clang()
    return is_config("toolchain", "clang", "llvm") or (not is_config("toolchain", "gcc") and (is_plat("macosx", "iphoneos") or is_host("macosx")))
end
local function is_gcc()
    return is_config("toolchain", "gcc") or (not is_config("toolchain", "clang", "llvm") and is_plat("linux"))
end

-- Helper functions
local function stdlib_config()
    if is_clang() and not is_plat("windows") and has_config("stdlib") then
        local std = get_config("stdlib")
        return { cxxflags = "-stdlib=" .. std, ldflags = "-stdlib=" .. std }
    end
    return { }
end

local subdirs = "src tests benchmarks"
local function source_files()
    local files = os.files("src/**.cc")
    table.join2(files, os.files("src/**.h"))

    if has_config("tests") then
        table.join2(files, os.files("tests/**.cc"))
        table.join2(files, os.files("tests/**.h"))
    end

    if has_config("benchmarks") then
        table.join2(files, os.files("benchmarks/**.cc"))
        table.join2(files, os.files("benchmarks/**.h"))
    end
    return files
end

add_requires("xxhash v0.8.3", { system = false, configs = stdlib_config() })
if has_config("tests") then
    add_requires("catch2 v3.13.0", { system = false, configs = stdlib_config() })
end
if has_config("benchmarks") then
    add_requires("benchmark v1.9.5", { system = false, configs = table.join(stdlib_config(), { exceptions = false, cxflags = "-DBENCHMARK_USE_LIBCXX=" .. (is_clang() and not is_plat("windows") and has_config("stdlib") and "ON" or "OFF"), }) })
end
if has_config("libunwind") and is_plat("linux") then
  add_requires("libunwind v1.8.3", { system = false, configs = stdlib_config() })
end
if has_config("fmtlib") then
    add_requires("fmt", { system = false, configs = stdlib_config() })
end

-- Tasks
task("format")
    set_menu({ usage = "xmake format", description = "format source code" })
    on_run( function ()
        local files = source_files()
        if #files > 0 then
            os.runv("clang-format", table.join({ "--fail-on-incomplete-format", "--ferror-limit=1", "--sort-includes", "-i" }, files))
        end
        os.run("uv sync")
        print(os.iorun("uv run scripts/header_license.py"):trim())
    end)
task_end()

task("tidy")
    set_menu({ usage = "xmake tidy", description = "Run clang-tidy --fix" })
    on_run( function ()
        local files = source_files()
        if #files > 0 then
          os.runv("clang-tidy", table.join({ "--use-color", "--fix", "--config-file=./.clang-tidy", "-p", "out/" }, files))
        end
    end)
task_end()

task("lint")
    set_menu({ usage = "xmake lint", description = "lint using cpplint & clang-format" })
    on_run( function ()
        os.run("uv sync")
        print(os.iorun("uv run cpplint --recursive " .. subdirs):trim())
        local files = source_files()
        if #files > 0 then
            os.runv("clang-format", table.join({ "--dry-run", "--fail-on-incomplete-format", "-i" }, files))
        end
    end)
task_end()

-- Events
after_build( function (target)
  if has_config("timetrace") then
    local trace_dir = path.join(os.projectdir(), "out/timetrace")
    os.mkdir(trace_dir)
    for _, objfile in ipairs(target:objectfiles()) do
      local json = objfile .. ".json"
      if os.exists(json) then
        os.cp(json, path.join(trace_dir, path.basename(json)))
      end
    end
  end

  if has_config("optreport") and is_mode("release") then
    local remark_dir = "out/remarks"
    os.mkdir(remark_dir)
    for _, yaml in ipairs(os.files(path.join(target:targetdir(), "**.opt.yaml"))) do
      os.cp(yaml, remark_dir)
    end
  end
end)

after_run( function (target)
  if has_config("coverage") and target:name() == "tests" and not is_plat("windows") then
    local profraw = path.join(target:targetdir(), "default.profraw")
    local profdata = path.join(target:targetdir(), "default.profdata")
    local coverage_dir = "out/coverage"

    os.runv("llvm-profdata", { "merge", "-sparse", profraw, "-o", profdata })
    os.runv(
      "llvm-cov", {
        "show",
        target:targetfile(),
        "-instr-profile=" .. profdata,
        "-format=html",
        "-output-dir=" .. coverage_dir,
      }
    )

    cprint("${green}coverage report generated at: " .. path.join(coverage_dir, "index.html"))
  end
end)

-- Rules
rule("fpag.common_config")
  on_load(function (target)
    target:set("languages", "c++23", { public = true })
    target:set("warnings", "all", "extra", "error", "pedantic", { public = true })
    target:set("encodings", "source:utf-8", "utf-8")

    target:add("includedirs", "src", "third_party", { public = true })
    target:add("defines", "FPAG_PROJECT_VERSION=\"" .. project_version .. "\"", { public = true })
    target:add("defines", "__STDC_CONSTANT_MACROS", "__STDC_FORMAT_MACROS", { public = true })

    target:set("exceptions", "none", { public = true })
    target:add("cxxflags", "-fno-exceptions", "-fno-rtti", { public = true })

    if is_clang() or is_gcc() then
      target:add("cxxflags", "-Wconversion", "-Wsign-conversion", "-Wnull-dereference", "-Wformat=2", "-Wundef", { public = true })
      target:add("cxxflags", "-fstack-protector-strong", { public = true })

      if is_mode("debug") and not is_plat("windows") then
        target:add("cxxflags", "-rdynamic", { public = true })
        target:add("ldflags", "-rdynamic", { public = true })
      end
    end

    if is_plat("linux") then
      if is_mode("debug") then
        target:add("ldflags", "-Wl,--build-id", { public = true })
      end
    end

    if is_mode("debug") then
      target:set("symbols", "debug", { public = true })
      target:set("optimize", "none", { public = true })
      target:add("cxxflags", "-fno-omit-frame-pointer", "-g3", { public = true })
      target:add("defines", "LLVM_ENABLE_STATS", "LLVM_ENABLE_DUMP", { public = true })
    elseif is_mode("release") then
      target:set("symbols", "hidden", { public = true })
      target:set("optimize", "fastest", { public = true })
      target:set("strip", "all", { public = true })
    end

    if is_clang() and has_config("stdlib") then
      target:add("cxxflags", "-stdlib=" .. get_config("stdlib"), { public = true })
      target:add("ldflags", "-stdlib=" .. get_config("stdlib"), { public = true })
    end

    if has_config("sanitizers") and is_mode("debug") and not is_plat("windows") then
      target:set("policy", "build.sanitizer.address", true)
      target:set("policy", "build.sanitizer.undefined", true)
      target:set("policy", "build.sanitizer.leak", true)
    end

    if has_config("xray") and is_mode("debug") then
      target:add("cxxflags", "-fxray-instrument", "-fxray-instruction-threshold=200", { public = true })
      target:add("ldflags", "-fxray-instrument", { public = true })
    end

    if has_config("coverage") and not is_plat("windows") then
      target:add("cxxflags", "-fprofile-instr-generate", "-fcoverage-mapping", { public = true })
      target:add("ldflags", "-fprofile-instr-generate", "-fcoverage-mapping", { public = true })
    end

    if has_config("optreport") and is_mode("release") then
      target:add("cxxflags", "-fsave-optimization-record", { public = true })
    end

    if has_config("timetrace") then
      target:add("cxxflags", "-ftime-trace", { public = true })
    end

    if has_config("native") and not is_cross() and is_mode("release") then
      target:add("cxxflags", "-march=native", { public = true })
    end

    if has_config("unitybuild") then
      target:add("rules", "c++.unity_build", { batchsize = 12 })
    end
  end)

-- targets
target("fpag")
  add_rules("fpag.common_config", { public = false })
  set_kind("$(kind)")
  add_files("src/**.cc")
  add_packages("xxhash", { public = true })

  if has_config("libunwind") and is_plat("linux") then
    add_packages("libunwind")
    add_defines("FPAG_BUILD_FLAG_INTERNAL_USE_LIBUNWIND()=1")
  else
    add_defines("FPAG_BUILD_FLAG_INTERNAL_USE_LIBUNWIND()=0")
  end

  if has_config("fmtlib") then
    add_packages("fmt", { public = true })
    add_defines("FPAG_BUILD_FLAG_INTERNAL_USE_FMTLIB()=1", { public = true })
  else
    add_defines("FPAG_BUILD_FLAG_INTERNAL_USE_FMTLIB()=0", { public = true })
  end

  if is_plat("windows") then
    add_links("dbghelp", "onecore")
  end

  add_headerfiles("src/(**.h)", { prefixdir = "fpag" })

  add_configfiles("build_info.h")

  set_default(true)
target_end()

target("tests")
  set_enabled(has_config("tests"))
  add_rules("fpag.common_config", { public = false })
  add_deps("fpag")
  set_kind("binary")
  add_files("tests/**.cc")
  add_packages("catch2")
  add_includedirs("tests", { public = true })

  -- catch2 uses c2y extension in their macro
  if is_clang() then add_cxxflags("-Wno-c2y-extensions") end

  set_default(false)
target_end()

target("benchmarks")
  set_enabled(has_config("benchmarks"))
  add_rules("fpag.common_config", { public = false })
  add_deps("fpag")
  set_kind("binary")
  add_files("benchmarks/**.cc")
  add_packages("benchmark")
  add_includedirs("benchmarks", { public = true })

  set_default(false)
target_end()
