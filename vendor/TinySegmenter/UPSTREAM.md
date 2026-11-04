# TinySegmenter 0.2

Original author: Taku Kudo, 2008. Original code/model: BSD 3-Clause; see
LICENSE-BSD, retrieved from http://chasen.org/~taku/software/TinySegmenter/LICENCE.txt.

Unmodified JavaScript mirrored by leungwensen/tiny-segmenter, commit
174c5e71578459cbac67deb7c25d8a53566c6120:
https://github.com/leungwensen/tiny-segmenter/tree/174c5e71578459cbac67deb7c25d8a53566c6120

- tinysegmenter.js from lib/index.js:
  SHA256 4f51ee99e4fdbccacdd3c11542e6bf6528d1718d79d1a424f747e09045959a15
- LICENSE-MIT from LICENSE (mirror/package attribution):
  SHA256 afb804e4ef33cdafae12af8bcfc43ff2c817c47fdd5b8f00c7b2efc599ec8b71

The bundle generator removes only the CommonJS export line and wraps the source
in an IIFE with the selection adapter. The vendored source stays unchanged.
This is a small statistical segmenter, not a dictionary or a guarantee of
linguistically exact boundaries. The model can split compounds and auxiliaries.
