"""Embed the pinned offline tokenizer and ES5 DOM selection adapter."""
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def bundle(request='null', tiny=True):
    source = (ROOT / 'vendor/TinySegmenter/tinysegmenter.js').read_bytes()
    if hashlib.sha256(source).hexdigest() != '4f51ee99e4fdbccacdd3c11542e6bf6528d1718d79d1a424f747e09045959a15':
        raise ValueError('Pinned TinySegmenter source checksum differs')
    tokenizer = source.decode('utf-8').replace('module.exports = TinySegmenter;', '')
    adapter = (ROOT / 'src/japanese-selection.js').read_text(encoding='utf-8')
    notices = '\n'.join((ROOT / 'vendor/TinySegmenter' / name).read_text(encoding='utf-8')
                        for name in ('LICENSE-BSD', 'LICENSE-MIT'))
    return ('/*\n' + notices + '\n*/\n(function(dtpRequest){\n' + (tokenizer if tiny else '') + '\n' + adapter + '\n})('+request+')').encode('utf-8')


if __name__ == '__main__':
    data = bundle()
    output = ROOT / 'build/japanese_bundle.h'
    output.parent.mkdir(exist_ok=True)
    payloads={'dtp_japanese_script':data,
              'dtp_japanese_context_script':bundle('{"mode":"context"}',False),
              'dtp_japanese_apply_script':bundle('__DTP_APPLY_REQUEST__',False)}
    definitions=[]
    for name,payload in payloads.items():
        rows=[','.join(str(v) for v in payload[i:i+24]) for i in range(0,len(payload),24)]
        definitions.append('static const unsigned char '+name+'[] = {\n'+',\n'.join(rows)+'\n};\n')
    output.write_text('/* Generated. Tiny retained; Sudachi uses context/apply adapters. */\n'+''.join(definitions),encoding='ascii')
    (ROOT / 'build/japanese-bundle.js').write_bytes(data)
    (ROOT / 'build/japanese-context.js').write_bytes(payloads['dtp_japanese_context_script'])
    (ROOT / 'build/japanese-apply.js').write_bytes(payloads['dtp_japanese_apply_script'])
    print('Embedded offline Japanese selection script:', len(data), 'bytes')
