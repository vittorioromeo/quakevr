#!/bin/sh
# Sequential verification runs (one spacewarp-ref instance at a time). Start it detached:
#   (setsid nohup ./verify.sh > /dev/null 2>&1 < /dev/null &)
# then poll runs/verify/all.done; per-run results are appended to runs/verify/results.txt.
cd "$(dirname "$0")" || exit 1
BIN=$(pwd)/spacewarp-ref
OUT=runs/verify
mkdir -p "$OUT" && cd "$OUT" || exit 1
rm -f results.txt all.done
VC=$HOME/.local/share/Steam/logs/vrcompositor.txt
for spec in \
  "none:--api none --seconds 10" \
  "ext:--api ext --seconds 10" \
  "fb:--api fb --seconds 10" \
  "load20:--api ext --load-ms 20 --seconds 10" \
  "dump:--api ext --seconds 8 --dump-motion dump" \
  "dumpcur:--api ext --mv-fov cur --seconds 8 --dump-motion dumpcur"; do
  name=${spec%%:*}
  args=${spec#*:}
  before=$(grep -c "Using motion encoding AppMVs" "$VC" 2>/dev/null)
  # shellcheck disable=SC2086
  "$BIN" $args --log "$name.log" --summary "$name.json" > "$name.out" 2>&1
  rc=$?
  after=$(grep -c "Using motion encoding AppMVs" "$VC" 2>/dev/null)
  echo "$name rc=$rc appmvs_before=$before appmvs_after=$after" >> results.txt
  sleep 2
done
echo done > all.done
