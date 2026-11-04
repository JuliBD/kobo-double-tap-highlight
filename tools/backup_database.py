"""Create a consistent SQLite snapshot via a read-only source connection."""
import argparse
from contextlib import closing
import hashlib
from pathlib import Path
import sqlite3
import time


def backup(source, destination):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if source == destination or destination.exists():
        raise ValueError('Backup destination must be a new, separate file')
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + '.partial')
    if partial.exists():
        raise ValueError('An incomplete backup already exists at the destination')
    started = time.monotonic()

    def progress(status, remaining, total):
        if time.monotonic() - started > 30:
            raise TimeoutError('Database backup did not complete within 30 seconds')

    with closing(sqlite3.connect(source.as_uri() + '?mode=ro', uri=True, timeout=10)) as original:
        with closing(sqlite3.connect(partial)) as snapshot:
            original.backup(snapshot, pages=128, progress=progress, sleep=.05)
            if snapshot.execute('PRAGMA quick_check').fetchall() != [('ok',)]:
                raise ValueError('SQLite snapshot integrity check failed')
            snapshot.commit()
    partial.replace(destination)
    return hashlib.sha256(destination.read_bytes()).hexdigest().upper()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    print(backup(args.source, args.destination))
