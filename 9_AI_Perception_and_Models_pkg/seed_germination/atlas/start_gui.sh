#!/usr/bin/env bash
set -euo pipefail
cd "${SEEDGERM_ATLAS_ROOT:-/root/seedgerm_atlas}"
source /usr/local/Ascend/ascend-toolkit/set_env.sh
exec python3 app_seedgerm_gui.py
