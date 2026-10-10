# Project Ambrose by Imjustchico
# Self-tests for the Windows setup's packaging: a stage built from a fake install and a fake MariaDB zip carries the servers, the tools, the SQL, the installer script, the notices, the MariaDB programs it needs and the runtime beside each program, and nothing it skips; the manifest lists every file with its size and SHA-256 and a file changed afterwards fails its check; a client archive, a type dump or a file off the content list is refused; a zip whose SHA-256 differs from the pin is refused, as is a release with no pin; the uninstall list deletes every staged file and then its folders, deepest first; and, where makensis is installed, the setup program builds with its SHA-256 written beside it.
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import package

PROGRAMS = ("loginserver.exe", "gameserver.exe", "patchserver.exe", "supervisor.exe", "dbimport.exe", "client.exe", "typeextract.exe", "schemaprobe.exe")


def fake_prefix(folder):
    prefix = os.path.join(folder, "prefix")
    for name in PROGRAMS + ("fmt.dll", "loginserver.pdb"):
        write(os.path.join(prefix, "bin", name), name)
    write(os.path.join(prefix, "bin", "plugins", "libmariadb", "caching_sha2_password.dll"), "plugin")
    for name in ("loginserver", "gameserver", "patchserver", "supervisor", "dbimport"):
        write(os.path.join(prefix, "etc", f"{name}.conf.dist"), "# Project Ambrose by Imjustchico\n# test\n")
    write(os.path.join(prefix, "share", "ambrose", "data", "sql", "base", "db_login", "base.sql"), "-- base\n")
    return prefix


def fake_mariadb(folder, extra=None):
    path = os.path.join(folder, "mariadb-11.8.2-winx64.zip")
    with zipfile.ZipFile(path, "w") as bundle:
        top = "mariadb-11.8.2-winx64/"
        for name in ("bin/mariadbd.exe", "bin/mariadb-install-db.exe", "bin/mariadb-admin.exe", "bin/mariadb.exe", "bin/server.dll",
                     "bin/mariadbd.pdb", "bin/mariadb-test.exe", "share/english/errmsg.sys", "include/mysql.h", "lib/libmariadb.lib",
                     "lib/plugin/auth_ed25519.dll", "COPYING", "mysql-test/run.pl"):
            bundle.writestr(top + name, name)
        for name, data in (extra or {}).items():
            bundle.writestr(top + name, data)
    return path


def fake_runtime(folder):
    runtime = os.path.join(folder, "runtime")
    for name in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        write(os.path.join(runtime, name), name)
    return runtime


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


class StageTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp()
        self.stage = os.path.join(self.folder, "stage")

    def tearDown(self):
        shutil.rmtree(self.folder, ignore_errors=True)

    def staged(self):
        package.stage_package(fake_prefix(self.folder), self.stage, fake_mariadb(self.folder), fake_runtime(self.folder))
        return set(package.relative_files(self.stage))

    def test_the_stage_carries_what_the_servers_need_and_nothing_it_skips(self):
        files = self.staged()
        for expected in ["bin/" + name for name in PROGRAMS] + ["bin/fmt.dll", "bin/plugins/libmariadb/caching_sha2_password.dll",
                         "etc/gameserver.conf.dist", "share/ambrose/data/sql/base/db_login/base.sql", "ambrose.ps1", "LICENSE.txt",
                         "THIRD-PARTY-NOTICES.txt", "mariadb/bin/mariadbd.exe", "mariadb/bin/mariadb-install-db.exe", "mariadb/bin/server.dll",
                         "mariadb/share/english/errmsg.sys", "mariadb/lib/plugin/auth_ed25519.dll", "mariadb/COPYING.txt",
                         "bin/vcruntime140.dll", "mariadb/bin/msvcp140.dll"]:
            self.assertIn(expected, files)
        for skipped in ("bin/loginserver.pdb", "mariadb/bin/mariadbd.pdb", "mariadb/bin/mariadb-test.exe", "mariadb/include/mysql.h",
                        "mariadb/lib/libmariadb.lib", "mariadb/mysql-test/run.pl"):
            self.assertNotIn(skipped, files)
        package.check_content(self.stage)
        with open(os.path.join(self.stage, "LICENSE.txt"), "rb") as handle:
            self.assertIn(b"\r\n", handle.read())

    def test_an_install_missing_a_server_is_refused(self):
        prefix = fake_prefix(self.folder)
        os.remove(os.path.join(prefix, "bin", "gameserver.exe"))
        with self.assertRaisesRegex(package.PackageError, "no bin/gameserver.exe"):
            package.stage_package(prefix, self.stage, fake_mariadb(self.folder), None)

    def test_client_files_type_dumps_and_strays_are_refused(self):
        self.staged()
        write(os.path.join(self.stage, "bin", "Root.wad"), "x")
        write(os.path.join(self.stage, "bin", "types", "806919.json"), "{}")
        write(os.path.join(self.stage, "notes.md"), "x")
        write(os.path.join(self.stage, "bin", "logs", "Server.log"), "x")
        with self.assertRaises(package.PackageError) as caught:
            package.check_content(self.stage)
        message = str(caught.exception)
        self.assertIn("bin/Root.wad is a client file", message)
        self.assertIn("bin/types/806919.json may be a type dump", message)
        self.assertIn("notes.md is not on the package's content list", message)
        self.assertIn("bin/logs/Server.log is a client file, capture, token or log", message)

    def test_the_manifest_lists_every_file_and_catches_a_change(self):
        files = self.staged()
        manifest = package.write_manifest(self.stage, "0.1.0")
        self.assertEqual({entry["path"] for entry in manifest["files"]}, files)
        server = os.path.join(self.stage, "bin", "gameserver.exe")
        entry = next(entry for entry in manifest["files"] if entry["path"] == "bin/gameserver.exe")
        self.assertEqual(entry["sha256"], hashlib.sha256(open(server, "rb").read()).hexdigest())
        self.assertEqual(package.verify_manifest(self.stage), [])
        write(server, "changed")
        self.assertEqual(package.verify_manifest(self.stage), ["bin/gameserver.exe changed after the manifest was written"])

    def test_the_uninstall_list_deletes_every_file_then_its_folders_deepest_first(self):
        files = self.staged()
        target = os.path.join(self.folder, "uninstall.nsh")
        package.write_uninstall_list(self.stage, target)
        lines = open(target, encoding="utf-8").read().splitlines()
        deleted = {line[len('Delete "$INSTDIR\\'):-1].replace("\\", "/") for line in lines if line.startswith("Delete ")}
        self.assertEqual(deleted, files)
        folders = [line[len('RMDir "$INSTDIR\\'):-1] for line in lines if line.startswith("RMDir ")]
        self.assertLess(folders.index("share\\ambrose\\data\\sql\\base\\db_login"), folders.index("share"))
        self.assertTrue(all(line.startswith(("Delete ", "RMDir ", "; ")) for line in lines))
        self.assertFalse(any("/r" in line for line in lines))


class MariaDbTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.folder, ignore_errors=True)

    def test_a_zip_that_differs_from_the_pin_is_refused(self):
        archive = fake_mariadb(self.folder)
        spec = {"version": "11.8.2", "url": "https://example.invalid/mariadb-11.8.2-winx64.zip", "sha256": "0" * 64}
        with self.assertRaisesRegex(package.PackageError, "not the 0{64} mariadb.json pins"):
            package.mariadb_zip(spec, archive, self.folder, False)

    def test_a_release_needs_a_pin(self):
        archive = fake_mariadb(self.folder)
        spec = {"version": "11.8.2", "url": "https://example.invalid/mariadb-11.8.2-winx64.zip", "sha256": ""}
        self.assertEqual(package.mariadb_zip(spec, archive, self.folder, False), archive)
        with self.assertRaisesRegex(package.PackageError, "pins no SHA-256"):
            package.mariadb_zip(spec, archive, self.folder, True)

    def test_the_published_sum_is_read_from_its_list(self):
        zip_sum, msi_sum = "B" * 64, "a" * 64
        for listed in (f"{zip_sum} *mariadb-11.8.2-winx64.zip", f"{zip_sum}  ./mariadb-11.8.2-winx64.zip",
                       f"{zip_sum}  mariadb-11.8.2/winx64-packages/mariadb-11.8.2-winx64.zip", f"SHA256 (mariadb-11.8.2-winx64.zip) = {zip_sum}"):
            text = f"{msi_sum}  mariadb-11.8.2-winx64.msi\n{listed}\n"
            self.assertEqual(package.published_sum(text, "mariadb-11.8.2-winx64.zip"), "b" * 64)
        text = f"{msi_sum}  mariadb-11.8.2-winx64.msi\n"
        with self.assertRaisesRegex(package.PackageError, "list no"):
            package.published_sum(text, "missing.zip")

    def test_a_zip_with_an_unsafe_path_is_refused(self):
        archive = fake_mariadb(self.folder, {"../escape.txt": "x"})
        stage = os.path.join(self.folder, "stage")
        os.makedirs(stage)
        with self.assertRaisesRegex(package.PackageError, "unsafe path"):
            package.stage_mariadb(archive, stage)

    def test_the_pinned_spec_names_a_winx64_zip_and_its_published_sums(self):
        spec = json.load(open(os.path.join(os.path.dirname(HERE), "mariadb.json"), encoding="utf-8"))
        self.assertTrue(spec["url"].endswith(f"mariadb-{spec['version']}-winx64.zip"))
        self.assertTrue(spec["sha256sums"].startswith("https://"))


@unittest.skipUnless(shutil.which("makensis"), "makensis is not installed")
class SetupBuildTests(unittest.TestCase):
    def test_the_setup_program_builds_from_a_fake_install(self):
        with tempfile.TemporaryDirectory() as folder:
            output = os.path.join(folder, "dist", "ProjectAmbrose-Setup-0.1.0.exe")
            spec_zip = fake_mariadb(folder)
            result = package.main(["--prefix", fake_prefix(folder), "--version", "v0.1.0-test", "--output", output,
                                   "--runtime", fake_runtime(folder), "--mariadb-zip", spec_zip])
            self.assertEqual(result, 0)
            with open(output, "rb") as handle:
                self.assertEqual(handle.read(2), b"MZ")
            digest = open(output + ".sha256", encoding="utf-8").read().split()
            self.assertEqual(digest, [hashlib.sha256(open(output, "rb").read()).hexdigest(), "ProjectAmbrose-Setup-0.1.0.exe"])

    def test_numeric_versions_have_four_parts(self):
        self.assertEqual(package.numeric_version("v1.2.3-rc1"), "1.2.3.1")
        self.assertEqual(package.numeric_version("0.1"), "0.1.0.0")
        self.assertEqual(package.numeric_version("dev"), "0.0.0.0")


if __name__ == "__main__":
    unittest.main(verbosity=1)
