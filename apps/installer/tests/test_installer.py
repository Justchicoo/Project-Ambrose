# Project Ambrose by Imjustchico
# Self-tests for the installer: that a MariaDB deps installs gets the account with no root password prompt while one already there is asked for its root password, that conf copies each installed template once, that running it again leaves an edited .conf alone, that a relative install prefix resolves against the checkout rather than the working directory, that compile installs the configuration the release presets build, RelWithDebInfo, rather than the build type's name, that run starts an app from its bin folder, so a supervisor finds the configurations it names relatively, that both the shell and the PowerShell script agree, each skipping where its interpreter is absent or, like WSL's bash on Windows, cannot take the checkout's paths, and that the PowerShell deps -Plan lists every install step it would take, or skip for what it found, never runs winget, and fails clearly without winget.
import functools
import json
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SHELL = os.path.join(ROOT, "apps", "installer", "ambrose.sh")
POWERSHELL = os.path.join(ROOT, "apps", "installer", "ambrose.ps1")
TEMPLATE = "# Project Ambrose by Imjustchico\nLogAsync.Enable = 0\n"
EDITED = "# edited by the operator\n"


def takes_this_checkouts_paths(candidate):
    try:
        probe = subprocess.run([candidate, "-c", 'test -f "$1"', "bash", SHELL], capture_output=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return False
    return probe.returncode == 0


@functools.lru_cache(maxsize=None)
def bash():
    candidates = [shutil.which("bash")]
    if os.name == "nt":
        git = shutil.which("git")
        roots = [os.path.dirname(os.path.dirname(git))] if git else []
        roots += [os.path.join(os.environ.get(name, ""), "Git") for name in ("ProgramFiles", "ProgramW6432", "LOCALAPPDATA")]
        candidates += [os.path.join(root, "bin", "bash.exe") for root in roots]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate) and takes_this_checkouts_paths(candidate):
            return candidate
    return None


def powershell():
    return shutil.which("pwsh") or shutil.which("powershell")


def prefix_with_templates(folder, names=("loginserver", "gameserver")):
    etc = os.path.join(folder, "etc")
    os.makedirs(etc, exist_ok=True)
    for name in names:
        with open(os.path.join(etc, name + ".conf.dist"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(TEMPLATE)
    return etc


def run_shell(command, prefix, cwd=None):
    environment = dict(os.environ, AMBROSE_INSTALL_PREFIX=prefix)
    return subprocess.run([bash(), SHELL, command], cwd=cwd or ROOT, env=environment,
                          capture_output=True, text=True)


def run_powershell(command, prefix, cwd=None):
    environment = dict(os.environ, AMBROSE_INSTALL_PREFIX=prefix)
    return subprocess.run([powershell(), "-NoProfile", "-File", POWERSHELL, command], cwd=cwd or ROOT,
                          env=environment, capture_output=True, text=True)


class RunTests(unittest.TestCase):
    @unittest.skipUnless(bash(), "no bash that takes this checkout's paths is installed (on Windows, WSL's bash cannot; install Git for Windows)")
    def test_run_starts_the_app_from_its_bin_folder_so_its_relative_paths_resolve_there(self):
        with tempfile.TemporaryDirectory() as folder:
            prefix_with_templates(folder, ("supervisor",))
            bin_folder = os.path.join(folder, "bin")
            os.makedirs(bin_folder, exist_ok=True)
            fake = os.path.join(bin_folder, "supervisor")
            with open(fake, "w", encoding="utf-8", newline="\n") as handle:
                handle.write("#!/usr/bin/env bash\nif [ -f supervisor.conf ]; then echo started-in-bin; else echo started-elsewhere; fi\n")
            os.chmod(fake, 0o755)
            environment = dict(os.environ, AMBROSE_INSTALL_PREFIX=folder)
            result = subprocess.run([bash(), SHELL, "run", "supervisor"], cwd=ROOT, env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("started-in-bin", result.stdout)


class InstallConfigTests(unittest.TestCase):
    def test_compile_installs_the_configuration_each_release_preset_builds(self):
        with open(os.path.join(ROOT, "CMakePresets.json"), encoding="utf-8") as handle:
            presets = {preset["name"]: preset.get("configuration") for preset in json.load(handle)["buildPresets"]}
        built = {presets[name] for name in ("windows-release", "linux-gcc-release", "linux-clang-release")}
        self.assertEqual(built, {"RelWithDebInfo"}, "the release presets no longer agree on one configuration")
        for script in ("ambrose.sh", "ambrose.ps1"):
            with open(os.path.join(ROOT, "apps", "installer", script), encoding="utf-8") as handle:
                text = handle.read()
            install = [line for line in text.splitlines() if "--install" in line or "InstallConfig =" in line]
            self.assertTrue(any("RelWithDebInfo" in line for line in install), f"{script} does not install what the release preset builds")
            self.assertFalse(any("'--config', $BuildType" in line or '--config "$BUILD_TYPE"' in line for line in install),
                             f"{script} still installs the build type rather than the preset's configuration")


class ConfTests(unittest.TestCase):
    def check_copies_then_preserves(self, run):
        with tempfile.TemporaryDirectory() as folder:
            prefix_with_templates(folder)
            first = run("conf", folder)
            self.assertEqual(first.returncode, 0, first.stderr)
            written = os.path.join(folder, "bin", "loginserver.conf")
            self.assertTrue(os.path.isfile(written), os.listdir(folder))
            self.assertTrue(os.path.isfile(os.path.join(folder, "bin", "gameserver.conf")))
            with open(written, "a", encoding="utf-8", newline="\n") as handle:
                handle.write(EDITED)
            second = run("conf", folder)
            self.assertEqual(second.returncode, 0, second.stderr)
            with open(written, encoding="utf-8") as handle:
                self.assertIn(EDITED.strip(), handle.read())

    @unittest.skipUnless(bash(), "no bash that takes this checkout's paths is installed (on Windows, WSL's bash cannot; install Git for Windows)")
    def test_the_shell_script_copies_each_template_once_and_keeps_an_edit(self):
        self.check_copies_then_preserves(run_shell)

    @unittest.skipUnless(powershell(), "PowerShell is not installed")
    def test_the_powershell_script_copies_each_template_once_and_keeps_an_edit(self):
        self.check_copies_then_preserves(run_powershell)

    def check_relative_prefix_is_not_the_working_directory(self, run):
        with tempfile.TemporaryDirectory() as elsewhere:
            result = run("conf", os.path.join("env", "review-relative"), cwd=elsewhere)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertEqual(os.listdir(elsewhere), [],
                             "a relative prefix was resolved against the working directory")

    @unittest.skipUnless(bash(), "no bash that takes this checkout's paths is installed (on Windows, WSL's bash cannot; install Git for Windows)")
    def test_the_shell_script_resolves_a_relative_prefix_against_the_checkout(self):
        self.check_relative_prefix_is_not_the_working_directory(run_shell)

    @unittest.skipUnless(powershell(), "PowerShell is not installed")
    def test_the_powershell_script_resolves_a_relative_prefix_against_the_checkout(self):
        self.check_relative_prefix_is_not_the_working_directory(run_powershell)

    @unittest.skipUnless(bash(), "no bash that takes this checkout's paths is installed (on Windows, WSL's bash cannot; install Git for Windows)")
    def test_conf_refuses_when_nothing_is_installed_yet(self):
        with tempfile.TemporaryDirectory() as folder:
            result = run_shell("conf", folder)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("run compile first", result.stderr)


def write_shim(tools, name, body_cmd, body_sh):
    if os.name == "nt":
        with open(os.path.join(tools, f"{name}.cmd"), "w", encoding="utf-8", newline="\r\n") as handle:
            handle.write(f"@echo off\n{body_cmd}\n")
        return
    shim = os.path.join(tools, name)
    with open(shim, "w", encoding="utf-8") as handle:
        handle.write(f"#!/bin/sh\n{body_sh}\n")
    os.chmod(shim, 0o755)


def run_powershell_plan(found, *options, with_winget=True, client=None):
    folder = tempfile.mkdtemp()
    tools = os.path.join(folder, "tools")
    staged = os.path.join(folder, "staged")
    os.makedirs(tools)
    os.makedirs(staged)
    marker = os.path.join(folder, "winget-ran")
    asked = os.path.join(folder, "client-ran")
    if client:
        write_shim(tools if client == "present" else staged, "mariadb", f'echo %*>> "{asked}"', f'echo "$@" >> "{asked}"')
    if with_winget:
        write_shim(tools, "winget", f'echo ran> "{marker}"\nif exist "{staged}\\mariadb.cmd" copy /y "{staged}\\mariadb.cmd" "{tools}" >nul',
                   f'echo ran > "{marker}"\n[ -f "{staged}/mariadb" ] && /bin/cp "{staged}/mariadb" "{tools}/"\nexit 0')
    environment = {key: value for key, value in os.environ.items() if key.upper() != "VCPKG_ROOT"}
    environment.update(AMBROSE_DEPS_FOUND=found, USERPROFILE=folder, ProgramFiles=folder,
                       PATH=tools + os.pathsep + environment.get("PATH", "") if with_winget and not client else tools)
    result = subprocess.run([powershell(), "-NoProfile", "-File", POWERSHELL, "deps", *options], cwd=ROOT,
                            env=environment, capture_output=True, text=True)
    ran = os.path.exists(marker)
    if client:
        ran = open(asked, encoding="utf-8").read() if os.path.exists(asked) else ""
    shutil.rmtree(folder, ignore_errors=True)
    return result, folder, ran


@unittest.skipUnless(powershell(), "PowerShell is not installed")
class DepsPlanTests(unittest.TestCase):
    def test_the_plan_lists_every_install_step_when_nothing_is_found(self):
        result, folder, ran = run_powershell_plan("none", "-Install", "-WithDatabase", "-Plan")
        self.assertEqual(result.returncode, 0, result.stderr)
        vcpkg = os.path.join(folder, "vcpkg")
        for step in ("install: Visual Studio 2022 Build Tools with the C++ workload (winget Microsoft.VisualStudio.2022.BuildTools, --add Microsoft.VisualStudio.Workload.VCTools)",
                     "install: CMake (winget Kitware.CMake)", "install: Git (winget Git.Git)",
                     f"install: vcpkg, cloned into {vcpkg}",
                     f"install: vcpkg, bootstrapped with {os.path.join(vcpkg, 'bootstrap-vcpkg.bat')} -disableMetrics",
                     "install: MariaDB as the service MariaDB on port 3306 (winget MariaDB.Server, SERVICENAME=MariaDB PORT=3306)", "create: the ambrose account",
                     "plan only; nothing was installed"):
            self.assertIn(step, result.stdout)
        self.assertNotIn("skip:", result.stdout)
        self.assertFalse(ran, "the plan ran winget")

    def test_the_plan_skips_what_it_finds(self):
        result, _, ran = run_powershell_plan("vs,cmake,git,vcpkg,mariadb", "--install", "--with-database", "--plan")
        self.assertEqual(result.returncode, 0, result.stderr)
        for tool in ("Visual Studio 2022 Build Tools", "CMake", "Git", "vcpkg, cloned", "vcpkg, bootstrapped", "MariaDB"):
            self.assertIn(f"skip: {tool}", result.stdout)
        self.assertNotIn("install:", result.stdout)
        self.assertFalse(ran, "the plan ran winget")

    def test_a_mariadb_it_installs_gets_the_account_without_a_root_password_prompt(self):
        result, _, asked = run_powershell_plan("vs,cmake,git,vcpkg", "-Install", "-WithDatabase", client="installed")
        self.assertIn("MariaDB has the ambrose account", result.stdout, result.stderr)
        self.assertIn("as root through the MariaDB client with no password", result.stdout)
        self.assertIn("-u root -e CREATE USER IF NOT EXISTS 'ambrose'@'localhost'", asked)
        self.assertNotIn("-p", asked.split(" -e ")[0], "a fresh install asked for a root password nobody set")

    def test_a_mariadb_already_there_asks_for_its_root_password(self):
        result, _, asked = run_powershell_plan("vs,cmake,git,vcpkg,mariadb", "-Install", "-WithDatabase", client="present")
        self.assertIn("MariaDB has the ambrose account", result.stdout, result.stderr)
        self.assertIn("which asks for the root password", result.stdout)
        self.assertIn("-u root -p -e CREATE USER", asked)

    def test_the_plan_never_creates_the_account_without_with_database(self):
        result, _, _ = run_powershell_plan("none", "-Install", "-Plan")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("MariaDB", result.stdout)
        self.assertNotIn("create:", result.stdout)

    def test_install_fails_clearly_without_winget(self):
        result, _, _ = run_powershell_plan("none", "-Install", "-Plan", with_winget=False)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("winget is missing", result.stderr + result.stdout)
        self.assertNotIn("install:", result.stdout)

    def test_deps_refuses_an_unknown_option(self):
        result, _, _ = run_powershell_plan("none", "-Bogus")
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("deps takes -Install, -WithDatabase and -Plan", result.stderr + result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=1)
