# Project Ambrose by Imjustchico
# Self-tests for the installer: that conf copies each installed template once, that running it again leaves an edited .conf alone, that a relative install prefix resolves against the checkout rather than the working directory, that compile installs the configuration the release presets build, RelWithDebInfo, rather than the build type's name, that both the shell and the PowerShell script agree, each skipping where its interpreter is absent, and that the PowerShell deps -Plan lists every install step it would take, or skip for what it found, never runs winget, and fails clearly without winget.
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SHELL = os.path.join(ROOT, "apps", "installer", "ambrose.sh")
POWERSHELL = os.path.join(ROOT, "apps", "installer", "ambrose.ps1")
TEMPLATE = "# Project Ambrose by Imjustchico\nLogAsync.Enable = 0\n"
EDITED = "# edited by the operator\n"


def bash():
    return shutil.which("bash")


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

    @unittest.skipUnless(bash(), "bash is not installed")
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

    @unittest.skipUnless(bash(), "bash is not installed")
    def test_the_shell_script_resolves_a_relative_prefix_against_the_checkout(self):
        self.check_relative_prefix_is_not_the_working_directory(run_shell)

    @unittest.skipUnless(powershell(), "PowerShell is not installed")
    def test_the_powershell_script_resolves_a_relative_prefix_against_the_checkout(self):
        self.check_relative_prefix_is_not_the_working_directory(run_powershell)

    @unittest.skipUnless(bash(), "bash is not installed")
    def test_conf_refuses_when_nothing_is_installed_yet(self):
        with tempfile.TemporaryDirectory() as folder:
            result = run_shell("conf", folder)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("run compile first", result.stderr)


def run_powershell_plan(found, *options, with_winget=True):
    folder = tempfile.mkdtemp()
    tools = os.path.join(folder, "tools")
    os.makedirs(tools)
    marker = os.path.join(folder, "winget-ran")
    if with_winget:
        with open(os.path.join(tools, "winget.cmd"), "w", encoding="utf-8", newline="\r\n") as handle:
            handle.write(f'@echo off\necho ran> "{marker}"\n')
    environment = {key: value for key, value in os.environ.items() if key.upper() != "VCPKG_ROOT"}
    environment.update(AMBROSE_DEPS_FOUND=found, USERPROFILE=folder,
                       PATH=tools + os.pathsep + environment.get("PATH", "") if with_winget else tools)
    result = subprocess.run([powershell(), "-NoProfile", "-File", POWERSHELL, "deps", *options], cwd=ROOT,
                            env=environment, capture_output=True, text=True)
    ran = os.path.exists(marker)
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
                     "install: MariaDB (winget MariaDB.Server)", "create: the ambrose account",
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
