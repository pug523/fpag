-- Copyright 2026 pugur
-- This source code is licensed under the Apache License, Version 2.0
-- which can be found in the LICENSE file.

set_project("fpag")
local project_version = "0.1.0"
set_version(project_version)

option("coverage")
set_default(false)
set_showmenu(true)
set_description("use llvm-cov for analyzing test coverage")
option_end()

option("xray")
set_default(false)
set_showmenu(true)
set_description("use llvm-xray for determining performance bottleneck")
option_end()

option("optreport")
set_default(false)
set_showmenu(true)
set_description("report optimization result")
option_end()

option("sanitizers")
set_default(false)
set_showmenu(true)
set_description("enable address/undefined behaviour/leak sanitizer")
option_end()

option("timetrace")
set_default(false)
set_showmenu(true)
set_description("generate timetrace json that can be see with perfetto ui")
option_end()

option("native")
set_default(false)
set_showmenu(true)
set_description("native architecture optimization")
option_end()

option("libunwind")
set_default(false)
set_showmenu(true)
set_description("use libunwind for stack tracing")
option_end()

option("unitybuild")
set_default(false)
set_showmenu(true)
set_description("enable unity build to shorten build time")
option_end()

option("lto")
set_default(false)
set_showmenu(true)
set_description("do link time optimization on release builds")
option_end()

option("tests")
set_default(false)
set_showmenu(true)
set_description("build unit tests")
option_end()

option("benchmarks")
set_default(false)
set_showmenu(true)
set_description("build micro benchmarks")
option_end()

option("stdlib")
set_default("libstdc++")
set_showmenu(true)
set_description("specify stl to use")
option_end()

set_policy("build.ccache", true)
set_policy("check.auto_ignore_flags", false)
set_policy("build.optimization.lto", has_config("lto"))
set_policy("build.c++.msvc.runtime", "MD")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
-- add_rules("plugin.compile_commands.autoupdate", { outputdir = "build/" })
add_rules("plugin.compile_commands.autoupdate")

-- Helper functions
local function is_clang()
  return is_config("toolchain", "clang", "llvm")
    or (
      not is_config("toolchain", "gcc")
      and (is_plat("macosx", "iphoneos") or is_host("macosx"))
    )
end

local function is_gcc()
  return is_config("toolchain", "gcc")
    or (not is_config("toolchain", "clang", "llvm") and is_plat("linux"))
end

local function stdlib_config()
  if is_clang() and not is_plat("windows") and has_config("stdlib") then
    local std = get_config("stdlib")
    return { cxxflags = "-stdlib=" .. std, ldflags = "-stdlib=" .. std }
  end
  return {}
end

local function coverage(target)
  return has_config("coverage")
    and target:name() == "tests"
    and not is_plat("windows")
    and is_clang()
end

local function optreport()
  return has_config("optreport") and not is_mode("debug")
end

local function sanitizers()
  return has_config("sanitizers")
    and is_mode("debug")
    and not is_plat("windows")
end

local function xray()
  return has_config("xray") and not is_mode("release")
end

local function libunwind()
  return has_config("libunwind") and is_plat("linux")
end

local subdirs = { "src", "include", "tests", "benchmarks" }

local function source_files()
  local files = os.files("src/**.cc")
  table.join2(files, os.files("src/**.h"))
  table.join2(files, os.files("include/**.h"))

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

add_requires(
  "xxhash v0.8.3",
  { system = false, external = true, configs = stdlib_config() }
)

if has_config("tests") then
  add_requires(
    "catch2 v3.13.0",
    { system = false, external = true, configs = stdlib_config() }
  )
end

if has_config("benchmarks") then
  add_requires("benchmark v1.9.5", {
    system = false,
    external = true,
    configs = table.join(stdlib_config(), {
      exceptions = false,
      cxflags = "-DBENCHMARK_USE_LIBCXX="
        .. (
          stdlib_config()
            and (has_config("stdlib") and get_config("stdlib") == "libc++")
            and "ON"
          or "OFF"
        ),
    }),
  })
end

if libunwind() then
  add_requires(
    "libunwind v1.8.3",
    { system = false, external = true, configs = stdlib_config() }
  )
end

add_requires(
  "fmt 12.1.0",
  { system = false, external = false, configs = stdlib_config() }
)

-- Tasks
task("format")
set_menu({ usage = "xmake format", description = "format source code" })
on_run(function()
  local files = source_files()
  if #files > 0 then
    os.execv(
      "clang-format",
      table.join({
        "--fail-on-incomplete-format",
        "--ferror-limit=1",
        "--sort-includes",
        "-i",
      }, files)
    )
  end
  os.exec("uv sync")
  os.exec("uv run scripts/header_license.py")
end)
task_end()

task("tidy")
set_menu({ usage = "xmake tidy", description = "Run clang-tidy --fix" })
on_run(function()
  local files = source_files()
  if #files > 0 then
    os.execv(
      "clang-tidy",
      table.join(
        { "--use-color", "--fix", "--config-file=./.clang-tidy" },
        files
      )
    )
  end
end)
task_end()

task("lint")
set_menu({
  usage = "xmake lint",
  description = "lint using cpplint & clang-format",
})
on_run(function()
  os.exec("uv sync")
  os.execv("uv", table.join({ "run", "cpplint", "--recursive" }, subdirs))
  local files = source_files()
  if #files > 0 then
    os.execv(
      "clang-format",
      table.join({ "--dry-run", "--fail-on-incomplete-format", "-i" }, files)
    )
  end
end)
task_end()

-- Events
after_build(function(target)
  if has_config("timetrace") then
    local trace_dir = path.join(os.projectdir(), "build/timetrace")
    os.mkdir(trace_dir)
    for _, objfile in ipairs(target:objectfiles()) do
      local json = objfile .. ".json"
      if os.exists(json) then
        os.cp(json, path.join(trace_dir, path.basename(json)))
      end
    end
  end

  if optreport() then
    local remark_dir = "build/remarks"
    os.mkdir(remark_dir)
    for _, yaml in
      ipairs(os.files(path.join(target:targetdir(), "**.opt.yaml")))
    do
      os.cp(yaml, remark_dir)
    end
  end
end)

after_run(function(target)
  if coverage(target) then
    local coverage_dir = path.join("build", "coverage", target:name())
    local profraw = path.join(target:targetdir(), "default.profraw")
    local profdata = path.join(target:targetdir(), "default.profdata")

    os.execv("llvm-profdata", { "merge", "-sparse", profraw, "-o", profdata })
    -- os.execv("llvm-profdata", { "merge", "-o", profdata, profraw })
    os.execv(
      "llvm-cov",
      table.join({
        "show",
        target:targetfile(),
        "-instr-profile=" .. profdata,
        "-format=html",
        "-output-dir=" .. coverage_dir,
      }, subdirs)
    )

    cprint(
      "${green}coverage report generated at: "
        .. path.join(coverage_dir, "index.html")
    )
  end
end)

-- Rules
rule("fpag.common_config")
on_load(function(target)
  target:set("languages", "c++20")
  target:set("warnings", { "all", "extra", "error", "pedantic" })
  target:set("encodings", "source:utf-8", "utf-8")

  target:add("includedirs", "src", "include", "third_party")
  target:add(
    "defines",
    'FPAG_PROJECT_VERSION="' .. project_version .. '"',
    { public = true }
  )
  target:add("defines", {
    "__STDC_CONSTANT_MACROS",
    "__STDC_FORMAT_MACROS",
    "FMT_USE_CONSTEXPR=1",
    "FMT_USE_CONSTEVAL=1",
  })

  target:set("exceptions", "none")
  target:add("cxxflags", { "-fno-exceptions", "-fno-rtti" })

  if is_clang() or is_gcc() then
    target:add("cxxflags", {
      "-Wconversion",
      "-Wsign-conversion",
      "-Wnull-dereference",
      "-Wformat=2",
      "-Wundef",
    })
    target:add("cxxflags", "-fstack-protector-strong")

    if is_mode("debug") and not is_plat("windows") then
      target:add("cxxflags", "-rdynamic")
      target:add("ldflags", "-rdynamic")
    end
  end

  -- some libraries use c2y extension in their macro
  if is_clang() then
    target:add("cxxflags", "-Wno-c2y-extensions")
  end

  if is_plat("linux") then
    if is_mode("debug") then
      target:add("ldflags", "-Wl,--build-id")
    end
  end

  if is_mode("debug") then
    target:set("symbols", "debug")
    target:set("optimize", "none")
    target:add("cxxflags", { "-fno-omit-frame-pointer", "-g3" })
  elseif is_mode("release") then
    target:set("symbols", "hidden")
    target:set("optimize", "fastest")
    target:set("strip", "all")
  end

  if is_clang() and has_config("stdlib") then
    local sl = get_config("stdlib")
    target:add("cxxflags", "-stdlib=" .. sl)
    target:add("ldflags", "-stdlib=" .. sl)
  end

  if sanitizers() then
    target:set("policy", "build.sanitizer.address", true)
    target:set("policy", "build.sanitizer.undefined", true)
    target:set("policy", "build.sanitizer.leak", true)
  end

  if xray() then
    target:add(
      "cxxflags",
      { "-fxray-instrument", "-fxray-instruction-threshold=200" }
    )
    target:add("ldflags", "-fxray-instrument")
  end

  if coverage(target) then
    target:add("cxxflags", { "-fprofile-instr-generate", "-fcoverage-mapping" })
    target:add("ldflags", { "-fprofile-instr-generate", "-fcoverage-mapping" })
  end

  if optreport() then
    target:add("cxxflags", "-fsave-optimization-record")
  end

  if has_config("timetrace") then
    target:add("cxxflags", "-ftime-trace")
  end

  if
    has_config("native")
    and not target:is_cross()
    and not is_mode("debug")
  then
    target:add("cxxflags", "-march=native")
  end

  if has_config("unitybuild") then
    target:add("rules", "c++.unity_build", { batchsize = 12 })
  end
end)

-- Targets
target("fpag")
add_rules("fpag.common_config", { public = false })
set_kind("$(kind)")
add_files("src/**.cc")
add_packages("xxhash", { public = true })

if libunwind() then
  add_packages("libunwind", { public = false })
  add_defines("FPAG_BUILD_FLAG_INTERNAL_USE_LIBUNWIND()=1", { public = false })
else
  add_defines("FPAG_BUILD_FLAG_INTERNAL_USE_LIBUNWIND()=0", { public = false })
end

add_packages("fmt", { public = true })

if is_plat("windows") then
  add_links("dbghelp", "onecore")
end

add_headerfiles("include/(**.h)")
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
