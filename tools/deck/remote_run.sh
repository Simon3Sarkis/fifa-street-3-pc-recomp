#!/bin/bash
# Run a test script on the Steam Deck from the PC and fetch its log and snaps (Git Bash).
# Usage: tools/deck/remote_run.sh <name> <script.txt> [extra --flags...]
#   SAVE=1 starts from the package's save_fresh_profile folder (copy of work/saves/fresh_profile).
#   RUN=run_native.sh DEST=fifastreet3-native runs the native Linux build instead of Proton (the
#   native package uses the Proton package's save_fresh_profile and game/).
# The run is a transient user service (systemd-run --user), so it survives the SSH session ending
# (setsid nohup does not). Results land in work/runs/<name>.log and work/snaps/<name>/.
set -eu
cd "$(dirname "$0")/../.."
[ -f tools/deck/deck.local ] && . tools/deck/deck.local
DECK="${DECK:-deck@192.168.1.82}"
DEST="${DEST:-fifastreet3}"
NAME="$1"; SCRIPT="$2"; shift 2
EXTRA=$(printf ' %q' "$@")
SAVE_ENV=""
[ "${SAVE:-}" = "1" ] && SAVE_ENV="-E SAVE=\$HOME/fifastreet3/save_fresh_profile"
# Leftover wineserver/winedevice/xalia from the previous run in our own prefix keep ~50% of a core busy
# and skew benchmarks: kill only processes whose environment names our prefix (fifastreet3-proton).
ssh "$DECK" 'for p in $(pgrep -u "$(id -u)"); do tr "\0" "\n" 2>/dev/null < /proc/$p/environ | grep -q "^STEAM_COMPAT_DATA_PATH=.*fifastreet3-proton" && kill -9 $p 2>/dev/null; done; pkill -9 -u "$(id -u)" -x fifastreet3 2>/dev/null; sleep 2' || true
ssh "$DECK" "cd ~/$DEST && rm -rf userdata-$NAME snaps/$NAME && \
  systemd-run --user --unit=fs3-$NAME --collect --working-directory=\$HOME/$DEST \
    -E DISPLAY=:0 -E XDG_RUNTIME_DIR=/run/user/\$(id -u) $SAVE_ENV ${CACHE_ROOT:+-E CACHE_ROOT=$CACHE_ROOT} ${RUN_ENV:+-E $RUN_ENV} \
    bash -c 'tools/deck/${RUN:-run.sh} $NAME $SCRIPT$EXTRA > runs/$NAME.out 2>&1' >/dev/null && \
  for i in \$(seq 1 200); do systemctl --user is-active -q fs3-$NAME || break; sleep 3; done; \
  cat runs/$NAME.out"
mkdir -p work/runs "work/snaps/$NAME"
scp -q "$DECK:$DEST/runs/$NAME.log" "work/runs/$NAME.log" || true
scp -q "$DECK:$DEST/runs/$NAME.mon.txt" "work/runs/$NAME.mon.txt" 2>/dev/null || true
scp -q -r "$DECK:$DEST/snaps/$NAME/." "work/snaps/$NAME/" 2>/dev/null || true
python -c "import glob,os;from PIL import Image;fs=glob.glob('work/snaps/$NAME/*.bmp');[(Image.open(f).save(f[:-4]+'.png'),os.remove(f)) for f in fs];print(f'snaps: {len(fs)}')"
