#!/bin/bash
# Run a test script on the Steam Deck from the PC and fetch its log and snaps (Git Bash).
# Usage: tools/deck/remote_run.sh <name> <script.txt> [extra --flags...]
#   SAVE=1 starts from the package's save_fresh_profile folder (copy of work/saves/fresh_profile).
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
[ "${SAVE:-}" = "1" ] && SAVE_ENV="-E SAVE=\$HOME/$DEST/save_fresh_profile"
ssh "$DECK" "cd ~/$DEST && rm -rf userdata-$NAME snaps/$NAME && \
  systemd-run --user --unit=fs3-$NAME --collect --working-directory=\$HOME/$DEST \
    -E DISPLAY=:0 -E XDG_RUNTIME_DIR=/run/user/\$(id -u) $SAVE_ENV \
    bash -c 'tools/deck/run.sh $NAME $SCRIPT$EXTRA > runs/$NAME.out 2>&1' >/dev/null && \
  for i in \$(seq 1 200); do systemctl --user is-active -q fs3-$NAME || break; sleep 3; done; \
  cat runs/$NAME.out"
mkdir -p work/runs "work/snaps/$NAME"
scp -q "$DECK:$DEST/runs/$NAME.log" "work/runs/$NAME.log" || true
scp -q -r "$DECK:$DEST/snaps/$NAME/." "work/snaps/$NAME/" 2>/dev/null || true
python -c "import glob,os;from PIL import Image;fs=glob.glob('work/snaps/$NAME/*.bmp');[(Image.open(f).save(f[:-4]+'.png'),os.remove(f)) for f in fs];print(f'snaps: {len(fs)}')"
