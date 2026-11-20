#!/bin/bash
# deploy.sh <ssh-host> [ssh-user] -- installs (or updates) Quake VR's room service on a Debian/Ubuntu server: copies
# qvr_rendezvous.py and the systemd unit over scp, then over ssh (sudo) creates the service's user, installs both under
# /opt/qvr-rendezvous and /etc/systemd/system, opens UDP 26100 in ufw if ufw is active, and (re)starts the service.
# Run it from your own machine; it asks for nothing but your ssh key / password and sudo password, stores nothing.
#
#   bash Misc/rendezvous/deploy.sh example.com            (ssh as yourself)
#   bash Misc/rendezvous/deploy.sh example.com admin      (ssh as admin)
#
# Then check it: ssh <host> 'systemctl status qvr-rendezvous; journalctl -u qvr-rendezvous -n 20'
set -euo pipefail

if [ $# -lt 1 ]; then
	sed -n '2,10p' "$0"
	exit 2
fi
HOST=$1
TARGET=$HOST
if [ $# -ge 2 ]; then
	TARGET="$2@$HOST"
fi
HERE=$(cd "$(dirname "$0")" && pwd)

python3 -m py_compile "$HERE/qvr_rendezvous.py" 2>/dev/null || python -m py_compile "$HERE/qvr_rendezvous.py"

echo "deploy: copying to $TARGET:/tmp/qvr-rendezvous-deploy/"
ssh "$TARGET" 'mkdir -p /tmp/qvr-rendezvous-deploy'
scp "$HERE/qvr_rendezvous.py" "$HERE/qvr-rendezvous.service" "$TARGET:/tmp/qvr-rendezvous-deploy/"

echo "deploy: installing (sudo)"
ssh -t "$TARGET" 'set -e
D=/tmp/qvr-rendezvous-deploy
command -v python3 >/dev/null || { echo "python3 is missing: sudo apt install python3"; exit 1; }
id qvr-rendezvous >/dev/null 2>&1 || sudo useradd --system --no-create-home --shell /usr/sbin/nologin qvr-rendezvous
sudo install -d -o root -g root -m 755 /opt/qvr-rendezvous
sudo install -o root -g root -m 644 "$D/qvr_rendezvous.py" /opt/qvr-rendezvous/qvr_rendezvous.py
sudo install -o root -g root -m 644 "$D/qvr-rendezvous.service" /etc/systemd/system/qvr-rendezvous.service
if command -v ufw >/dev/null && sudo ufw status | grep -q "Status: active"; then
	sudo ufw allow 26100/udp comment "Quake VR room service"
fi
sudo systemctl daemon-reload
sudo systemctl enable qvr-rendezvous
sudo systemctl restart qvr-rendezvous
sleep 1
systemctl --no-pager --lines=5 status qvr-rendezvous
rm -f "$D/qvr_rendezvous.py" "$D/qvr-rendezvous.service"
rmdir "$D"'
echo "deploy: done. Quake VR's default vr_mp_rendezvous is vittorioromeo.com:26100."
