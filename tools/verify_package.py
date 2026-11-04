"""Inspect the diagnostic installation package without extracting or running it."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile

from elftools.elf.elffile import ELFFile

TARGET = 'usr/local/Kobo/imageformats/libkobodoubletapprobe.so'


def verify(package, library):
    with tarfile.open(package, 'r:gz') as archive:
        members = archive.getmembers()
        if len(members) != 1:
            raise ValueError('Package must contain exactly one plugin file')
        member = members[0]
        if (PurePosixPath(member.name).as_posix() != TARGET or not member.isfile()
                or member.uid != 0 or member.gid != 0 or member.mode & 0o7022):
            raise ValueError('Unexpected installation path, file type, owner or permissions')
        data = archive.extractfile(member).read()
    if data != library.read_bytes():
        raise ValueError('Packaged plugin does not match the built library')
    elf = ELFFile(io.BytesIO(data))
    if (elf.elfclass != 32 or not elf.little_endian or elf['e_machine'] != 'EM_ARM'
            or elf['e_type'] != 'ET_DYN' or not elf['e_flags'] & 0x400):
        raise ValueError('Expected an ARM32 little-endian hard-float shared library')
    dynamic_symbols = elf.get_section_by_name('.dynsym')
    exports = {s.name for s in dynamic_symbols.iter_symbols()
               if s['st_shndx'] != 'SHN_UNDEF' and s['st_info']['bind'] == 'STB_GLOBAL'
               and s['st_other']['visibility'] == 'STV_DEFAULT'}
    required = {'NickelHook', 'dtp_process_tap', 'qt_plugin_instance', 'qt_plugin_query_metadata'}
    if not required <= exports:
        raise ValueError('Missing plugin exports: ' + ', '.join(sorted(required - exports)))
    metadata = elf.get_section_by_name('.qtmetadata')
    if metadata is None or b'nh.kobodoubletapprobe' not in metadata.data():
        raise ValueError('Missing unique Qt plugin metadata key')
    needed = [tag.needed for tag in elf.get_section_by_name('.dynamic').iter_tags()
              if tag.entry.d_tag == 'DT_NEEDED']
    versions = elf.get_section_by_name('.gnu.version_r')
    glibc = sorted({aux.name for _, auxiliaries in versions.iter_versions() for aux in auxiliaries
                    if aux.name.startswith('GLIBC_')}) if versions else []
    if any(tuple(map(int,name[6:].split('.'))) > (2,19) for name in glibc):
        raise ValueError('Plugin requires a libc newer than the Kobo target')
    return {
        'install_path': '/' + TARGET,
        'package_sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
        'library_sha256': hashlib.sha256(data).hexdigest(),
        'library_bytes': len(data),
        'architecture': 'ARM32 EABI hard-float',
        'required_exports': sorted(required),
        'annotation_actions': {'dtp_annotation_setup', 'dtp_annotation_save', 'dtp_annotation_delete'} <= exports,
        'gesture_actions': {'dtp_toggle_allow', 'dtp_toggle_set_current', 'dtp_toggle_page'} <= exports,
        'japanese_longpress': 'dtp_longpress_start' in exports,
        'sudachi_backend': 'dtp_sudachi_backend' in exports,
        'glibc_requirements': glibc,
        'needed_libraries': needed,
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('library', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = verify(args.package, args.library)
    rendered = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(rendered, encoding='utf-8')
    print(rendered)
