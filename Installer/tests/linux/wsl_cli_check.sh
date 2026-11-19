#!/bin/bash
# Runs inside WSL archlinux: the self-contained linux-x64 qvr-setup on a small Linux test package.
# Package: the kit's WSL engine build (ironwail ELF) + a few files of the worktree's quakevr folder + manifest.json.
# Feed: a local latest.json with packages.linux-x64, served by qvr-setup serve on 127.0.0.1.
# Everything written goes under ~/qvr-xi2 (the Linux home: /var is refused as a system folder, so file modes are real).
W=${QVR_REPO:-/mnt/c/OHWorkspace/qvr-agents/testsplit}  # the repository, as WSL sees it
Q=${QVR_SETUP:-$W/scratch/xi2_linux_cli/qvr-setup}  # the published linux-x64 qvr-setup
T=$HOME/qvr-xi2/run-$(date +%s)
QUAKE=/mnt/c/OHWorkspace/qvr-kit/bases/testsplit/qbase
mkdir -p $T/pkg/quakevr/tools $T/www
cp /var/tmp/qvr-linux/linuxlink/b-default/ironwail $T/pkg/ironwail
cp $W/quakevr/*.cfg $T/pkg/quakevr/ 2>/dev/null
printf '#!/bin/sh\necho tool\n' > $T/pkg/quakevr/tools/hello.sh
chmod 644 $T/pkg/ironwail $T/pkg/quakevr/tools/hello.sh
echo "== manifest"; $Q manifest $T/pkg --version 9.9.9-linuxtest
( cd $T/pkg && python3 -c "import zipfile,os
z=zipfile.ZipFile('$T/www/QuakeVR-linux-x64.zip','w',zipfile.ZIP_DEFLATED)
[z.write(os.path.join(r,f),os.path.relpath(os.path.join(r,f),'.')) for r,_,fs in os.walk('.') for f in fs]
z.close()" )
SIZE=$(stat -c %s $T/www/QuakeVR-linux-x64.zip); SHA=$(sha256sum $T/www/QuakeVR-linux-x64.zip | cut -d' ' -f1)
PORT=18731
cat > $T/www/latest.json <<EOF
{"schema":1,"version":"9.9.9-linuxtest",
 "package":{"file":"QuakeVR.zip","size":1,"sha256":"$(printf 'a%.0s' $(seq 64))","urls":["http://127.0.0.1:$PORT/QuakeVR.zip"]},
 "packages":{"linux-x64":{"file":"QuakeVR-linux-x64.zip","size":$SIZE,"sha256":"$SHA","urls":["http://127.0.0.1:$PORT/QuakeVR-linux-x64.zip"]}}}
EOF
echo "== feed --file"; $Q feed --file $T/www/latest.json
$Q serve --dir $T/www --port $PORT --minutes 2 > $T/serve.log 2>&1 &
SERVER=$!
sleep 1
echo "== install (feed, sandbox)"
$Q install --feed http://127.0.0.1:$PORT/latest.json --sandbox $T/sb --quake $QUAKE --accept-statement --registry-file $T/sb/uninstall.json 2>&1 | tail -8
kill $SERVER 2>/dev/null
echo "== modes"; stat -c '%A %n' $T/sb/QuakeVR/ironwail $T/sb/QuakeVR/quakevr/tools/hello.sh
echo "== shortcuts"; find $T/sb/_shortcuts -name '*.desktop' | head; grep -h '^Exec=' $(find $T/sb/_shortcuts -name '*.desktop') | head -3
echo "== detect"; $Q detect --sandbox $T/sb | tail -2
echo "== verify"; $Q verify --target $T/sb/QuakeVR
echo "== uninstall"; $Q uninstall --target $T/sb/QuakeVR --registry-file $T/sb/uninstall.json
echo "== left"; ls $T/sb; ls $T/sb/_shortcuts 2>/dev/null
echo "== sandbox $T"
