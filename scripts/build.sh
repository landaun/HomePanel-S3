#!/usr/bin/env bash
set -euo pipefail

with-idf idf.py --preview set-target esp32s3
with-idf idf.py fullclean build
