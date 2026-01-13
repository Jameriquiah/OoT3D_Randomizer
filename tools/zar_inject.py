import argparse
import json
import os
import struct


MANIFEST_NAME = "zar_manifest.json"
TAIL_NAME = "__zar_tail.bin"


def read_header(data):
    if len(data) < 0x20:
        raise ValueError("File too small for ZAR header")
    raw = struct.unpack_from("<I", data, 0x08)[0]
    count = raw >> 16
    low16 = raw & 0xFFFF
    entry_off = struct.unpack_from("<I", data, 0x10)[0]
    return count, low16, entry_off


def read_entries(data, count, entry_off):
    end = entry_off + count * 8
    if end > len(data):
        raise ValueError(f"Entry table out of bounds: {hex(end)} > {hex(len(data))}")
    entries = []
    for i in range(count):
        data_off, name_off = struct.unpack_from("<II", data, entry_off + i * 8)
        entries.append((data_off, name_off))
    return entries


def read_name(data, name_off):
    if name_off >= len(data):
        return None
    end = data.find(b"\x00", name_off)
    if end == -1:
        end = len(data)
    return data[name_off:end].decode("ascii", errors="replace")


def align4(buf):
    while len(buf) % 4 != 0:
        buf += b"\x00"


def compute_ranges(entries, data_size):
    data_offsets = sorted({data_off for data_off, _ in entries})
    ranges = {}
    for i, data_off in enumerate(data_offsets):
        end = data_offsets[i + 1] if i + 1 < len(data_offsets) else data_size
        ranges[data_off] = end
    return ranges


def extract_all(zar_path, out_dir):
    with open(zar_path, "rb") as f:
        data = f.read()

    count, low16, entry_off = read_header(data)
    entries = read_entries(data, count, entry_off)

    os.makedirs(out_dir, exist_ok=True)

    ranges = compute_ranges(entries, len(data))
    order = []

    # Identify tail metadata block (mads/mmad/txpt/strt) to avoid bundling it into the last entry.
    tag_positions = []
    for tag in (b"mads", b"mmad", b"txpt", b"strt"):
        idx = data.find(tag)
        if idx != -1:
            tag_positions.append(idx)
    tail_start = min(tag_positions) if tag_positions else None
    if tail_start is not None and tail_start < 0:
        tail_start = None

    for data_off, name_off in entries:
        name = read_name(data, name_off)
        if not name:
            raise ValueError(f"Missing name at offset {hex(name_off)}")

        order.append(name)
        end = ranges[data_off]
        if tail_start is not None and end > tail_start and data_off < tail_start:
            end = tail_start
        if end <= data_off:
            raise ValueError(f"Invalid data range for {name}")

        rel_path = os.path.join(out_dir, *name.split("/"))
        os.makedirs(os.path.dirname(rel_path), exist_ok=True)
        with open(rel_path, "wb") as f:
            f.write(data[data_off:end])

    prefix = data[:entry_off]
    # Write tail metadata block, if present.
    if tail_start is not None:
        tail_path = os.path.join(out_dir, TAIL_NAME)
        with open(tail_path, "wb") as f:
            f.write(data[tail_start:])

    manifest = {
        "base_zar_path": zar_path,
        "prefix_hex": prefix.hex(),
        "low16": low16,
        "entry_off": entry_off,
        "order": order,
        "tail_present": tail_start is not None,
    }

    manifest_path = os.path.join(out_dir, MANIFEST_NAME)
    with open(manifest_path, "w", encoding="ascii", newline="\n") as f:
        json.dump(manifest, f, indent=2)

    print("Extracted", count, "entries to", out_dir)
    print("Wrote manifest", manifest_path)


def collect_files(root_dir):
    files = {}
    for base, _dirs, names in os.walk(root_dir):
        for name in names:
            if name in (MANIFEST_NAME, TAIL_NAME):
                continue
            full_path = os.path.join(base, name)
            rel_path = os.path.relpath(full_path, root_dir)
            rel_path = rel_path.replace("\\", "/")
            files[rel_path] = full_path
    return files


def repack(in_dir, out_zar):
    manifest_path = os.path.join(in_dir, MANIFEST_NAME)
    if not os.path.exists(manifest_path):
        raise ValueError(f"Missing manifest: {manifest_path}")

    with open(manifest_path, "r", encoding="ascii") as f:
        manifest = json.load(f)

    base_zar_path = manifest.get("base_zar_path")
    if not base_zar_path or not os.path.exists(base_zar_path):
        raise ValueError("Missing or invalid base_zar_path in manifest")

    with open(base_zar_path, "rb") as f:
        base_data = f.read()

    prefix = bytes.fromhex(manifest["prefix_hex"])
    entry_off = int(manifest["entry_off"])
    low16 = int(manifest["low16"])
    order = list(manifest["order"])

    if len(prefix) != entry_off:
        raise ValueError("Prefix length does not match entry_off")

    files = collect_files(in_dir)

    # Treat files not in the original order as new entries to append.
    extras = sorted([name for name in files if name not in order])
    if extras:
        print("Found extra files to append:", ", ".join(extras))
    else:
        print("No extra files found to append.")

    # Build entry list from the original base archive.
    base_count, _base_low16, base_entry_off = read_header(base_data)
    if base_entry_off != entry_off:
        raise ValueError("Entry offset mismatch between manifest and base ZAR")
    base_entries = read_entries(base_data, base_count, base_entry_off)
    base_names = [read_name(base_data, name_off) for _data_off, name_off in base_entries]
    if base_names != order:
        raise ValueError("Manifest order does not match base ZAR entries")

    # Prepare the new order: original entries + new extras.
    full_order = list(order) + extras
    count = len(full_order)
    entry_table_size = count * 8

    # Read block descriptors from the base header.
    def read_block(desc_off):
        cnt = struct.unpack_from("<I", base_data, desc_off)[0]
        start = struct.unpack_from("<I", base_data, desc_off + 4)[0]
        end = struct.unpack_from("<I", base_data, desc_off + 8)[0]
        return cnt, start, end

    block0 = read_block(0x40)
    block1 = read_block(0x50)
    block2 = read_block(0x60)

    block0_count, block0_start, block0_end = block0
    block1_count, block1_start, block1_end = block1
    block2_count, block2_start, block2_end = block2

    # Build new block2 list (append new indices).
    new_block2_count = block2_count + len(extras)
    new_block2_end = block2_start + new_block2_count * 4

    block0_list = base_data[block0_start:block0_end]
    block1_list = base_data[block1_start:block1_end]
    gap0 = base_data[block0_end:block1_start]
    gap1 = base_data[block1_end:block2_start]
    block2_list = bytearray(base_data[block2_start:block2_end])

    for idx in range(base_count, count):
        block2_list += struct.pack("<I", idx)

    # Faceb header (8 bytes) sits immediately after block2 list.
    faceb_header = base_data[block2_end:block2_end + 8]
    if faceb_header[:5] != b"faceb":
        raise ValueError("Unexpected faceb header in base ZAR")

    # Build name table.
    name_offsets = {}
    name_table = bytearray()
    for name in full_order:
        name_offsets[name] = len(name_table)
        name_table += name.encode("ascii") + b"\x00"
    align4(name_table)

    # Assemble prefix (header + tables) using the base header as template.
    header_prefix = bytearray(base_data[:block0_start])
    # Update block descriptors.
    struct.pack_into("<I", header_prefix, 0x40, block0_count)
    struct.pack_into("<I", header_prefix, 0x44, block0_start)
    struct.pack_into("<I", header_prefix, 0x48, block0_end)
    struct.pack_into("<I", header_prefix, 0x50, block1_count)
    struct.pack_into("<I", header_prefix, 0x54, block1_start)
    struct.pack_into("<I", header_prefix, 0x58, block1_end)
    struct.pack_into("<I", header_prefix, 0x60, new_block2_count)
    struct.pack_into("<I", header_prefix, 0x64, block2_start)
    struct.pack_into("<I", header_prefix, 0x68, new_block2_end)

    out = bytearray()
    out += header_prefix
    out += block0_list
    out += gap0
    out += block1_list
    out += gap1
    out += block2_list
    out += faceb_header

    entry_off_new = len(out)

    # Reserve entry table space.
    out += b"\x00" * entry_table_size

    # Append name table.
    name_table_start = len(out)
    out += name_table
    name_table_end = len(out)

    # Append entry data in order.
    data_offsets = {}
    for name in full_order:
        while len(out) % 4 != 0:
            out += b"\x00"
        data_offsets[name] = len(out)
        with open(files[name], "rb") as f:
            out += f.read()

    # Append preserved tail metadata block if present.
    if manifest.get("tail_present"):
        tail_path = os.path.join(in_dir, TAIL_NAME)
        if not os.path.exists(tail_path):
            raise ValueError(f"Missing tail file: {tail_path}")
        while len(out) % 4 != 0:
            out += b"\x00"
        with open(tail_path, "rb") as f:
            out += f.read()

    # Write entry table.
    for i, name in enumerate(full_order):
        data_off = data_offsets[name]
        name_off = name_table_start + name_offsets[name]
        struct.pack_into("<II", out, entry_off_new + i * 8, data_off, name_off)

    # Update header fields that depend on table sizes and file size.
    struct.pack_into("<I", out, 0x08, (count << 16) | low16)
    struct.pack_into("<I", out, 0x10, entry_off_new)
    struct.pack_into("<I", out, 0x14, name_table_end)
    struct.pack_into("<I", out, 0x04, len(out))

    with open(out_zar, "wb") as f:
        f.write(out)

    print("Repacked", count, "entries to", out_zar)


def main():
    parser = argparse.ArgumentParser(description="Extract/repack OoT3D ZAR archives")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_extract = sub.add_parser("extract", help="Extract all entries to a folder")
    p_extract.add_argument("--zar", required=True)
    p_extract.add_argument("--out-dir", required=True)

    p_repack = sub.add_parser("repack", help="Repack a folder (with manifest) into a ZAR")
    p_repack.add_argument("--in-dir", required=True)
    p_repack.add_argument("--out-zar", required=True)

    args = parser.parse_args()
    if args.cmd == "extract":
        extract_all(args.zar, args.out_dir)
    elif args.cmd == "repack":
        repack(args.in_dir, args.out_zar)


if __name__ == "__main__":
    main()
