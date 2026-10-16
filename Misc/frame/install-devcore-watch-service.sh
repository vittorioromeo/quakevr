#!/bin/sh
# One-time setup for the Quake VR GPU-dump watcher, run as root on the Steam Frame.
# It installs a boot-time service that copies each devcoredump into
# /home/steamos/devcore-dumps and frees it before the kernel's ~5 minute expiry.
# This file contains no password. You run it yourself and type your sudo password at the prompt:
#   ssh -t frame 'sudo sh -c "$(cat ~/install-devcore-watch.sh)"'
# SteamOS keeps the root filesystem read-only. If the writes below fail, run
#   sudo steamos-readonly disable
# first, then `sudo steamos-readonly enable` after the install.
# To remove the service later:
#   sudo systemctl disable --now qvr-devcore-watch.service
#   sudo rm /etc/systemd/system/qvr-devcore-watch.service /usr/local/sbin/qvr-devcore-watch.sh
set -eu

cat > /usr/local/sbin/qvr-devcore-watch.sh <<'WATCH'
#!/bin/sh
out=/home/steamos/devcore-dumps
mkdir -p "$out"
chown steamos:steamos "$out"
while true; do
  for d in /sys/class/devcoredump/devcd*; do
    [ -e "$d/data" ] || continue
    name="$(date +%Y%m%d-%H%M%S)-$(basename "$d")"
    if cat "$d/data" > "$out/$name.bin" && echo 1 > "$d/data"; then
      chown steamos:steamos "$out/$name.bin"
      echo "saved $out/$name.bin"
    else
      echo "FAILED to save $d" >&2
    fi
  done
  sleep 1
done
WATCH
chmod 755 /usr/local/sbin/qvr-devcore-watch.sh

cat > /etc/systemd/system/qvr-devcore-watch.service <<'UNIT'
[Unit]
Description=Copy and free GPU devcoredumps (Quake VR crash investigation)
After=local-fs.target

[Service]
Type=simple
ExecStart=/usr/local/sbin/qvr-devcore-watch.sh
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
UNIT

systemctl daemon-reload
systemctl enable --now qvr-devcore-watch.service
systemctl is-active qvr-devcore-watch.service
