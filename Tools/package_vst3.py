#!/usr/bin/env python3
"""Validate and stage the Windows x64 Checkmate VST3 preview package."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
from pathlib import Path
from typing import Any

EXPECTED_NAME = "Checkmate"
EXPECTED_VENDOR = "Skell Audio"
EXPECTED_VERSION = "2.4.0"
EXPECTED_CLASSES = {"Audio Module Class", "Component Controller Class"}
CID_RE = re.compile(r"^[0-9A-Fa-f]{32}$")


class PackageValidationError(RuntimeError):
    """A build output is not safe to stage as a Windows VST3 preview."""


def remove_trailing_commas(text: str) -> str:
    """Remove JSON trailing commas outside strings, preserving string data."""
    out: list[str] = []
    in_string = False
    escaped = False
    index = 0
    while index < len(text):
        char = text[index]
        if in_string:
            out.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            index += 1
            continue
        if char == '"':
            in_string = True
            out.append(char)
            index += 1
            continue
        if char == ",":
            lookahead = index + 1
            while lookahead < len(text) and text[lookahead].isspace():
                lookahead += 1
            if lookahead < len(text) and text[lookahead] in "]}":
                index += 1
                continue
        out.append(char)
        index += 1
    if in_string:
        raise PackageValidationError("moduleinfo.json contains an unterminated string")
    return "".join(out)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_amd64_pe(path: Path) -> None:
    size = path.stat().st_size
    with path.open("rb") as stream:
        dos = stream.read(64)
        if len(dos) < 64 or dos[:2] != b"MZ":
            raise PackageValidationError(f"{path}: missing DOS MZ header")
        offset = struct.unpack_from("<I", dos, 0x3C)[0]
        if offset < 64 or offset + 26 > size:
            raise PackageValidationError(f"{path}: invalid PE header offset {offset}")
        stream.seek(offset)
        header = stream.read(6)
        stream.seek(offset + 24)
        optional_magic = stream.read(2)
    if len(header) != 6 or header[:4] != b"PE\0\0":
        raise PackageValidationError(f"{path}: missing PE signature")
    machine = struct.unpack_from("<H", header, 4)[0]
    if machine != 0x8664:
        raise PackageValidationError(f"{path}: expected AMD64 0x8664, found 0x{machine:04x}")
    if optional_magic != b"\x0b\x02":
        value = struct.unpack("<H", optional_magic)[0] if len(optional_magic) == 2 else -1
        raise PackageValidationError(f"{path}: expected PE32+ optional header 0x20b, found 0x{value:04x}")


def validate_bundle(bundle: Path, expected_version: str = EXPECTED_VERSION) -> dict[str, Any]:
    if not bundle.is_dir() or bundle.name != f"{EXPECTED_NAME}.vst3":
        raise PackageValidationError(f"{bundle}: expected {EXPECTED_NAME}.vst3 bundle")
    contents = bundle / "Contents"
    metadata_path = contents / "Resources" / "moduleinfo.json"
    windows_dir = contents / "x86_64-win"
    if not metadata_path.is_file():
        raise PackageValidationError(f"{bundle}: missing Contents/Resources/moduleinfo.json")
    if not windows_dir.is_dir():
        raise PackageValidationError(f"{bundle}: missing Contents/x86_64-win")

    modules = sorted(path for path in contents.rglob("*.vst3") if path.is_file())
    if len(modules) != 1:
        raise PackageValidationError(f"{bundle}: expected exactly one VST3 module across all architectures, found {len(modules)}")
    module = modules[0]
    if module.parent != windows_dir or module.name != f"{EXPECTED_NAME}.vst3":
        raise PackageValidationError(f"{bundle}: module must be Contents/x86_64-win/{EXPECTED_NAME}.vst3")
    require_amd64_pe(module)

    try:
        normalized = remove_trailing_commas(metadata_path.read_text(encoding="utf-8-sig"))
        metadata = json.loads(normalized)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise PackageValidationError(f"{metadata_path}: invalid JSON: {exc}") from exc
    if not isinstance(metadata, dict):
        raise PackageValidationError(f"{metadata_path}: JSON root must be an object")
    factory = metadata.get("Factory Info")
    classes = metadata.get("Classes")
    if metadata.get("Name") != EXPECTED_NAME:
        raise PackageValidationError(f"{metadata_path}: plugin name mismatch")
    if metadata.get("Version") != expected_version:
        raise PackageValidationError(f"{metadata_path}: expected version {expected_version}")
    if not isinstance(factory, dict) or factory.get("Vendor") != EXPECTED_VENDOR:
        raise PackageValidationError(f"{metadata_path}: vendor mismatch")
    if not isinstance(classes, list) or len(classes) != 2:
        raise PackageValidationError(f"{metadata_path}: expected exactly two VST3 classes")

    categories: set[str] = set()
    class_ids: set[str] = set()
    for entry in classes:
        if not isinstance(entry, dict):
            raise PackageValidationError(f"{metadata_path}: class entry must be an object")
        category = entry.get("Category")
        class_id = entry.get("CID")
        if category not in EXPECTED_CLASSES:
            raise PackageValidationError(f"{metadata_path}: unexpected class category {category!r}")
        if not isinstance(class_id, str) or not CID_RE.fullmatch(class_id):
            raise PackageValidationError(f"{metadata_path}: invalid class ID {class_id!r}")
        normalized_id = class_id.upper()
        if normalized_id in class_ids:
            raise PackageValidationError(f"{metadata_path}: duplicate VST3 class ID {normalized_id}")
        if entry.get("Name") != EXPECTED_NAME or entry.get("Vendor") != EXPECTED_VENDOR or entry.get("Version") != expected_version:
            raise PackageValidationError(f"{metadata_path}: class name/vendor/version mismatch")
        class_ids.add(normalized_id)
        categories.add(category)
    if categories != EXPECTED_CLASSES:
        raise PackageValidationError(f"{metadata_path}: processor/controller class pair incomplete")

    # JUCE currently emits trailing commas. Normalize only the staged copy.
    metadata_path.write_text(normalized.rstrip() + "\n", encoding="utf-8", newline="\n")
    warnings = []
    if not factory.get("URL"):
        warnings.append("vendor URL is blank; owner decision required before commercial release")
    if not factory.get("E-Mail"):
        warnings.append("support email is blank; owner decision required before commercial release")
    return {
        "name": EXPECTED_NAME,
        "version": expected_version,
        "vendor": EXPECTED_VENDOR,
        "module": module.name,
        "module_bytes": module.stat().st_size,
        "module_sha256": sha256_file(module),
        "class_ids": sorted(class_ids),
        "metadata_warnings": warnings,
    }


def write_text(path: Path, content: str) -> None:
    path.write_text(content.rstrip() + "\n", encoding="utf-8", newline="\n")


def stage_package(source_dir: Path, package_dir: Path, expected_version: str = EXPECTED_VERSION) -> dict[str, Any]:
    if not source_dir.is_dir():
        raise PackageValidationError(f"source directory does not exist: {source_dir}")
    if source_dir.resolve() == package_dir.resolve():
        raise PackageValidationError("source and package directories must be different")
    bundles = sorted(path for path in source_dir.rglob("*.vst3") if path.is_dir())
    if len(bundles) != 1 or bundles[0].name != f"{EXPECTED_NAME}.vst3":
        names = [path.name for path in bundles]
        raise PackageValidationError(f"expected exactly one {EXPECTED_NAME}.vst3 bundle, found {names}")
    if package_dir.exists() or package_dir.is_symlink():
        raise PackageValidationError(f"package destination already exists: {package_dir}; choose a new output path to preserve the existing package")

    package_dir.mkdir(parents=True)
    staged_bundle = package_dir / f"{EXPECTED_NAME}.vst3"
    shutil.copytree(bundles[0], staged_bundle)
    plugin = validate_bundle(staged_bundle, expected_version)
    info = {
        "product": EXPECTED_NAME,
        "release_status": "preview-only; not a commercial release",
        "format": "VST3",
        "platform": "Windows x86_64",
        "version": expected_version,
        "vendor": EXPECTED_VENDOR,
        "source_commit": os.environ.get("GITHUB_SHA", "local-unrecorded"),
        "source_ref": os.environ.get("GITHUB_REF", "local-unrecorded"),
        "workflow_run_id": os.environ.get("GITHUB_RUN_ID", "local-unrecorded"),
        "workflow_run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", "local-unrecorded"),
        "plugins": [plugin],
        "release_gates": {
            "windows_build": "workflow-gated",
            "pluginval_strictness_5": "workflow-gated",
            "fl_studio_acceptance": "not verified by CI",
            "commercial_license": "owner approval required",
            "vendor_url_and_support_email": "owner approval required",
        },
    }
    write_text(package_dir / "build-info.json", json.dumps(info, indent=2, sort_keys=True))
    write_text(package_dir / "PREVIEW-ONLY.txt", """Checkmate Windows x64 VST3 preview package.
This is an unsigned engineering preview, not a commercial release or installer.
Do not redistribute or use as proof of a commercial license.
Back up any existing Checkmate.vst3 installation before manual testing.
The package contains one VST3 bundle only; no Standalone executable is included.
FL Studio host acceptance, licensing, vendor/support metadata and signing remain pending.
""")
    write_text(package_dir / "WINDOWS-VST3-INSTALL.txt", """Checkmate Windows VST3 preview — manual test install and rollback

1. Close FL Studio and any other VST host.
2. If Checkmate.vst3 already exists in the VST3 folder, copy the entire folder to a safe backup location first. Do not overwrite it.
3. Copy the complete Checkmate.vst3 folder from this package into:
   C:\\Program Files\\Common Files\\VST3
4. Start FL Studio and rescan plugins. Load Checkmate as an instrument.
5. Confirm MIDI input/output routing and audio before saving a project.

Rollback:
1. Close the DAW and remove only the preview Checkmate.vst3 folder you installed.
2. Restore your backed-up Checkmate.vst3 folder to its original location.
3. Rescan plugins in the DAW.

This is a manual preview procedure, not an installer. It does not authorize commercial redistribution.
""")
    checksum_lines = []
    for path in sorted(item for item in package_dir.rglob("*") if item.is_file()):
        checksum_lines.append(f"{sha256_file(path)}  {path.relative_to(package_dir).as_posix()}")
    write_text(package_dir / "SHA256SUMS.txt", "\n".join(checksum_lines))
    return info


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--package-dir", required=True, type=Path)
    parser.add_argument("--version", default=EXPECTED_VERSION)
    args = parser.parse_args()
    try:
        report = stage_package(args.source_dir, args.package_dir, args.version)
    except PackageValidationError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    plugin = report["plugins"][0]
    print(f"PASS {plugin['name']} {plugin['version']}: {plugin['module_bytes']:,} bytes; AMD64 PE32+; 2 unique class IDs")
    for warning in plugin["metadata_warnings"]:
        print(f"RELEASE WARNING: {warning}")
    print(f"Staged preview package: {args.package_dir}")
    print("Generated build-info.json, PREVIEW-ONLY.txt, install/rollback guide and SHA256SUMS.txt.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
