# Project Ambrose by Imjustchico
# Self-tests for the installer: that conf copies each installed template once, that running it again leaves an edited .conf alone, that a relative install prefix resolves against the checkout rather than the working directory, and that both the shell and the PowerShell script agree, each skipping where its interpreter is absent.
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


if __name__ == "__main__":
    unittest.main(verbosity=1)
