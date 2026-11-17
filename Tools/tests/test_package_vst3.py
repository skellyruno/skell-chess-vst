import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

from Tools.package_vst3 import PackageValidationError, remove_trailing_commas, stage_package


class CheckmatePackageTests(unittest.TestCase):
    def make_pe(self, path: Path, machine: int = 0x8664, optional_magic: int = 0x20B) -> None:
        data = bytearray(512)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 128)
        data[128:132] = b"PE\0\0"
        struct.pack_into("<H", data, 132, machine)
        struct.pack_into("<H", data, 152, optional_magic)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def make_bundle(self, root: Path, name: str = "Checkmate", version: str = "2.4.0", vendor: str = "Skell Audio", cids=None, machine: int = 0x8664, optional_magic: int = 0x20B) -> Path:
        bundle = root / f"{name}.vst3"
        resources = bundle / "Contents" / "Resources"
        module = bundle / "Contents" / "x86_64-win" / f"{name}.vst3"
        resources.mkdir(parents=True)
        if cids is None:
            cids = ["ABCDEF019182FAEB536B4175436B4D74", "ABCDEF011234ABCD536B4175436B4D74"]
        metadata = {
            "Name": name,
            "Version": version,
            "Factory Info": {"Vendor": vendor, "URL": "", "E-Mail": ""},
            "Classes": [
                {"CID": cids[0], "Category": "Audio Module Class", "Name": name, "Vendor": vendor, "Version": version},
                {"CID": cids[1], "Category": "Component Controller Class", "Name": name, "Vendor": vendor, "Version": version},
            ],
        }
        raw = json.dumps(metadata, indent=2).replace('"E-Mail": ""', '"E-Mail": "",')
        (resources / "moduleinfo.json").write_text(raw, encoding="utf-8")
        self.make_pe(module, machine, optional_magic)
        return bundle

    def test_trailing_comma_removal_is_string_aware(self):
        raw = '{"text":"literal ,} and \\"quoted\\"","nested":[1,2,],}'
        parsed = json.loads(remove_trailing_commas(raw))
        self.assertEqual(parsed["text"], 'literal ,} and "quoted"')
        self.assertEqual(parsed["nested"], [1, 2])

    def test_unterminated_string_is_rejected(self):
        with self.assertRaises(PackageValidationError):
            remove_trailing_commas('{"broken":"value}')

    def test_stages_valid_bundle_and_writes_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, package = root / "source", root / "package"
            self.make_bundle(source)
            info = stage_package(source, package)
            self.assertEqual(info["plugins"][0]["name"], "Checkmate")
            self.assertTrue((package / "build-info.json").is_file())
            self.assertTrue((package / "PREVIEW-ONLY.txt").is_file())
            self.assertTrue((package / "WINDOWS-VST3-INSTALL.txt").is_file())
            self.assertTrue((package / "SHA256SUMS.txt").is_file())
            self.assertFalse((package / "Checkmate.exe").exists())
            staged = json.loads((package / "Checkmate.vst3/Contents/Resources/moduleinfo.json").read_text())
            self.assertEqual(staged["Name"], "Checkmate")
            for line in (package / "SHA256SUMS.txt").read_text().splitlines():
                digest, relative = line.split("  ", 1)
                self.assertEqual(hashlib.sha256((package / relative).read_bytes()).hexdigest(), digest)

    def test_refuses_to_overwrite_existing_package(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, package = root / "source", root / "package"
            self.make_bundle(source)
            package.mkdir()
            sentinel = package / "keep.txt"
            sentinel.write_text("known-good package", encoding="utf-8")
            with self.assertRaisesRegex(PackageValidationError, "choose a new output path"):
                stage_package(source, package)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "known-good package")

    def test_rejects_non_amd64_module(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_bundle(root / "source", machine=0x14C)
            with self.assertRaisesRegex(PackageValidationError, "expected AMD64"):
                stage_package(root / "source", root / "package")

    def test_rejects_pe32_not_pe32_plus(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_bundle(root / "source", optional_magic=0x10B)
            with self.assertRaisesRegex(PackageValidationError, r"expected PE32\+"):
                stage_package(root / "source", root / "package")

    def test_rejects_duplicate_architecture_module(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            bundle = self.make_bundle(root / "source")
            extra = bundle / "Contents" / "x86_64-linux" / "Checkmate.vst3"
            extra.parent.mkdir(parents=True)
            extra.write_bytes(b"unexpected architecture module")
            with self.assertRaisesRegex(PackageValidationError, "exactly one VST3 module"):
                stage_package(root / "source", root / "package")

    def test_rejects_duplicate_class_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_bundle(root / "source", cids=["ABCDEF019182FAEB536B4175436B4D74", "ABCDEF019182FAEB536B4175436B4D74"])
            with self.assertRaisesRegex(PackageValidationError, "duplicate VST3 class ID"):
                stage_package(root / "source", root / "package")

    def test_rejects_wrong_version(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_bundle(root / "source", version="2.3.0")
            with self.assertRaisesRegex(PackageValidationError, "expected version 2.4.0"):
                stage_package(root / "source", root / "package")

    def test_rejects_wrong_vendor(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_bundle(root / "source", vendor="Unknown Vendor")
            with self.assertRaisesRegex(PackageValidationError, "vendor mismatch"):
                stage_package(root / "source", root / "package")

    def test_rejects_unexpected_bundle_inventory(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_bundle(root / "source", name="Other Plugin")
            with self.assertRaisesRegex(PackageValidationError, "expected exactly one Checkmate.vst3"):
                stage_package(root / "source", root / "package")

    def test_rejects_missing_module_metadata(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            bundle = self.make_bundle(root / "source")
            (bundle / "Contents/Resources/moduleinfo.json").unlink()
            with self.assertRaisesRegex(PackageValidationError, "missing Contents/Resources/moduleinfo.json"):
                stage_package(root / "source", root / "package")


if __name__ == "__main__":
    unittest.main()
