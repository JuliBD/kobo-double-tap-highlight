"""Download the pinned upstream NickelTC Linux toolchain layer without Docker.

Writes only under .tools/nickeltc-image; extraction is a separate Linux step.
No registry credentials are required or stored.
"""
import hashlib
import json
from pathlib import Path
import urllib.request

REPOSITORY = 'pgaskin/nickeltc'
MANIFEST = 'sha256:3296eb400d270cfd26f5773ab09625d4469f85680b3a191951dfb78d8ade7840'
LAYER = 'sha256:862464e9e2622637c11fb49c036a98e99998c1328b130422a1dea6d4b45c9e21'
DESTINATION = Path(__file__).resolve().parent.parent / '.tools/nickeltc-image'


def digest_file(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return 'sha256:' + digest.hexdigest()


def main():
    DESTINATION.mkdir(parents=True, exist_ok=True)
    url = f'https://ghcr.io/token?service=ghcr.io&scope=repository:{REPOSITORY}:pull'
    with urllib.request.urlopen(url, timeout=60) as response:
        token = json.load(response)['token']
    headers = {
        'Authorization': 'Bearer ' + token,
        'Accept': 'application/vnd.docker.distribution.manifest.v2+json,application/vnd.oci.image.manifest.v1+json',
    }
    url = f'https://ghcr.io/v2/{REPOSITORY}/manifests/{MANIFEST}'
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as response:
        raw = response.read()
    if 'sha256:' + hashlib.sha256(raw).hexdigest() != MANIFEST:
        raise ValueError('Manifest checksum mismatch')
    manifest = json.loads(raw)
    layer = next(entry for entry in manifest['layers'] if entry['digest'] == LAYER)
    (DESTINATION / 'manifest.json').write_bytes(raw)
    metadata = {'repository': REPOSITORY, 'manifest': MANIFEST, 'toolchain_layer': LAYER}
    (DESTINATION / 'source.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    destination = DESTINATION / 'toolchain.tar.gz'
    if destination.exists() and digest_file(destination) == LAYER:
        print('Pinned toolchain layer already downloaded and verified.')
        return
    temporary = destination.with_suffix('.partial')
    url = f'https://ghcr.io/v2/{REPOSITORY}/blobs/{LAYER}'
    count = 0
    digest = hashlib.sha256()
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=120) as response:
        with temporary.open('wb') as target:
            for chunk in iter(lambda: response.read(1024 * 1024), b''):
                target.write(chunk)
                digest.update(chunk)
                count += len(chunk)
    if count != layer['size'] or 'sha256:' + digest.hexdigest() != LAYER:
        raise ValueError('Toolchain layer size/checksum mismatch')
    temporary.replace(destination)
    print(f'Downloaded and verified {count} bytes to {destination}')


if __name__ == '__main__':
    main()
