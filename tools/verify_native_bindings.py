"""Check local pinned firmware definitions, PLT hook sites, and Qt imports.

This checks symbol availability, not C++ signatures or runtime behavior.
"""
import argparse
from contextlib import ExitStack
import json
from pathlib import Path
import re
from elftools.elf.elffile import ELFFile


def verify(root):
    root = Path(root)
    firmware = root / 'firmware/4.38.23852'
    with ExitStack() as stack:
        def elf(path):
            return ELFFile(stack.enter_context(path.open('rb')))
        native = elf(firmware / 'libnickel.so.1.0.0')
        dyn = native.get_section_by_name('.dynsym')
        defined = {s.name for s in dyn.iter_symbols() if s['st_shndx'] != 'SHN_UNDEF'}
        plt = {dyn.get_symbol(r['r_info_sym']).name for r in native.get_section_by_name('.rel.plt').iter_relocations()}
        source = (root / 'src/probe.c').read_text()
        # Only inspect hooks compiled into this binary.
        if 'dtp_sudachi_backend' not in {s.name for s in elf(root / 'src/libkobodoubletapprobe.so').get_section_by_name('.dynsym').iter_symbols() if s['st_shndx'] != 'SHN_UNDEF'}:
            source = re.sub(r'#if DTP_USE_SUDACHI\n.*?#endif', '', source, flags=re.S)
        hooks = re.findall(r'\.sym\s*=\s*"([^"]+)"\s*,\s*\.sym_new\s*=\s*"([^"]+)"', source)
        resolutions = re.findall(r'\.name\s*=\s*"(_Z[^"]+)"\s*,\s*\.out', source)
        plugin = elf(root / 'src/libkobodoubletapprobe.so')
        symbols = list(plugin.get_section_by_name('.dynsym').iter_symbols())
        exports = {s.name for s in symbols if s['st_shndx'] != 'SHN_UNDEF' and s['st_other']['visibility'] == 'STV_DEFAULT'}
        errors = []
        for target, replacement in hooks:
            if target not in defined or target not in plt:
                errors.append('No defined PLT hook site: ' + target)
            if replacement not in exports:
                errors.append('Missing replacement export: ' + replacement)
        qt_exports = set()
        for name in ('libQtCore.so.4.6.2','libQtGui.so.4.6.2','libQtWidgets.so.4.6.2','libQtWebKitWidgets.so.4.6.2'):
            library = elf(firmware / name)
            qt_exports.update(s.name for s in library.get_section_by_name('.dynsym').iter_symbols()
                              if s['st_shndx'] != 'SHN_UNDEF')
        qt_imports = {s.name for s in symbols if s['st_shndx'] == 'SHN_UNDEF' and re.match(r'_ZNK?\d+Q', s.name)}
        errors += ['Missing native/Qt definition: ' + s for s in resolutions if s not in defined | qt_exports]
        errors += ['Missing target Qt import: ' + s for s in sorted(qt_imports - qt_exports)]
        if errors:
            raise ValueError('\n'.join(errors))
        return {'native_hooks_with_plt_sites': len(hooks), 'native_resolutions': len(resolutions),
                'matching_target_qt_imports': len(qt_imports), 'signature_validation': 'manual firmware inspection; not established by this check'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    rendered = json.dumps(verify(Path(__file__).resolve().parents[1]), indent=2) + '\n'
    if args.output:
        args.output.write_text(rendered, encoding='utf-8')
    print(rendered)
