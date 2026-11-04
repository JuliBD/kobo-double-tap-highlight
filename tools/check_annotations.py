"""Compare offline SQLite snapshots without printing book text or note contents."""
from contextlib import closing
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3


def fingerprint(row):
    parts = [row.get(key) or '' for key in ('ContentID', 'StartContainerPath', 'EndContainerPath', 'Text')]
    parts += [str(int(row.get(key) or 0)) for key in
              ('StartContainerChildIndex', 'StartOffset', 'EndContainerChildIndex', 'EndOffset')]
    digest = hashlib.sha256()
    for part in parts:
        data = str(part).encode('utf-8')
        digest.update(str(len(data)).encode() + b':' + data)
    return digest.hexdigest()


def read_rows(path):
    with closing(sqlite3.connect(Path(path).resolve().as_uri() + '?mode=ro', uri=True)) as connection:
        connection.row_factory = sqlite3.Row
        if connection.execute('PRAGMA quick_check').fetchall()[0][0] != 'ok':
            raise ValueError('Snapshot integrity check failed')
        return {row['BookmarkID']: dict(row) for row in connection.execute('SELECT * FROM Bookmark')}


def active(row):
    return str(row.get('Hidden') or '').lower() not in ('1', 'true')


def plain(row):
    return 'highlight' in str(row.get('Type') or '').lower() and not row.get('Annotation') and \
        'note' not in str(row.get('Type') or '').lower()


def compare(before_path, after_path, registry_path=None):
    before, after = read_rows(before_path), read_rows(after_path)
    new = [row for key, row in after.items() if key not in before]
    protected = [key for key, row in before.items()
                 if 'highlight' in str(row.get('Type') or '').lower() or row.get('Annotation')]
    changed = sum(key not in after or
                  (fingerprint(before[key]), before[key].get('Annotation'), active(before[key]), before[key].get('Type')) !=
                  (fingerprint(after[key]), after[key].get('Annotation'), active(after[key]), after[key].get('Type'))
                  for key in protected)
    tracked = []
    if registry_path:
        lines = Path(registry_path).read_text(encoding='utf-8').splitlines()
        if not lines or lines[0] not in ('DTP-TEST-HIGHLIGHTS-1', 'DTP-TEST-HIGHLIGHTS-2') or len(lines) > 4097:
            raise ValueError('Invalid test annotation registry')
        for line in lines[1:]:
            key, expected = line.split('\t')
            row = after.get(key)
            tracked.append({
                'id_sha256': hashlib.sha256(key.encode()).hexdigest(),
                'existed_before_test': key in before,
                'exists_now': row is not None,
                'active_now': bool(row and active(row)),
                'plain_highlight': bool(row and plain(row)),
                'range_hash_matches': bool(row and fingerprint(row) == expected),
            })
    return {'new_active_plain_highlights': sum(active(row) and plain(row) for row in new),
            'new_active_notes': sum(active(row) and bool(row.get('Annotation')) for row in new),
            'existing_annotations_changed': changed, 'tracked_test_records': tracked}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--registry', type=Path)
    args = parser.parse_args()
    print(json.dumps(compare(args.before, args.after, args.registry), indent=2))
