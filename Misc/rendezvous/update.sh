#!/bin/bash
# update.sh -- updates Quake VR's room service on its server in one go, checked before and after:
#   1. the service's own tests (test_rendezvous.py) run here first: a failure deploys nothing;
#   2. deploy.sh installs it and restarts it (ssh, sudo there);
#   3. the server is asked which protocol version it now runs (must be the one just deployed) and whether the service
#      is active, and its last log lines are shown (sudo there).
# Run it from your own machine, in the repository. It asks for nothing but your ssh key / password and sudo password.
#
#   bash Misc/rendezvous/update.sh vittorioromeo.com                    the service in this checkout
#   bash Misc/rendezvous/update.sh vittorioromeo.com --user admin       ssh as admin
#   bash Misc/rendezvous/update.sh vittorioromeo.com --rev b3c6066ba~1  the service as of that commit (a rollback:
#                                                                        b3c6066ba~1 is protocol 1, before version 2)
#   bash Misc/rendezvous/update.sh vittorioromeo.com --check            only step 3: what the server runs now
#   bash Misc/rendezvous/update.sh vittorioromeo.com --dry-run          steps 1 and what would be deployed; no ssh
#
# Version 2 refuses version-1 games (no room code; joins by address still work): deploy it with the game's release.
set -euo pipefail

usage() {
	sed -n '2,16p' "$0"
	exit 2
}

HOST=""
SSH_USER=""
REV=""
MODE=deploy
while [ $# -gt 0 ]; do
	case "$1" in
		--user) [ $# -ge 2 ] || usage; SSH_USER=$2; shift 2 ;;
		--rev) [ $# -ge 2 ] || usage; REV=$2; shift 2 ;;
		--check) MODE=check; shift ;;
		--dry-run) MODE=dry; shift ;;
		-h|--help) usage ;;
		-*) echo "update: unknown option $1"; usage ;;
		*) [ -z "$HOST" ] || usage; HOST=$1; shift ;;
	esac
done
[ -n "$HOST" ] || usage
TARGET=$HOST
[ -z "$SSH_USER" ] || TARGET="$SSH_USER@$HOST"
HERE=$(cd "$(dirname "$0")" && pwd)
REMOTE_FILE=/opt/qvr-rendezvous/qvr_rendezvous.py

# The protocol version a qvr_rendezvous.py speaks (its "VERSION = n" line).
version_of() {
	sed -n 's/^VERSION = \([0-9][0-9]*\).*/\1/p' "$1" | head -1
}

# Step 3: what the server runs.
check_server() {
	local want=${1:-}
	echo "update: asking $TARGET what it runs"
	local remote active
	remote=$(ssh "$TARGET" "sed -n 's/^VERSION = \\([0-9][0-9]*\\).*/\\1/p' $REMOTE_FILE | head -1")
	active=$(ssh "$TARGET" 'systemctl is-active qvr-rendezvous' || true)
	echo "update: the server's room service: protocol version ${remote:-unknown}, $active"
	echo "update: its last log lines (sudo there):"
	ssh -t "$TARGET" 'sudo journalctl -u qvr-rendezvous -n 15 --no-pager' || echo "update: (its log could not be read)"
	if [ "$active" != "active" ]; then
		echo "update: FAILED: the service is not active ($active)"
		return 1
	fi
	if [ -n "$want" ] && [ "$remote" != "$want" ]; then
		echo "update: FAILED: the server runs version ${remote:-unknown}, $want was deployed"
		return 1
	fi
	echo "update: OK${want:+ (version $want running)}"
}

if [ "$MODE" = check ]; then
	check_server
	exit $?
fi

# The files deployed: this checkout's, or --rev's (staged in a folder of their own, with deploy.sh and the unit).
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
if [ -n "$REV" ]; then
	REPO=$(git -C "$HERE" rev-parse --show-toplevel)
	git -C "$REPO" rev-parse --verify --quiet "$REV^{commit}" >/dev/null || { echo "update: no commit $REV"; exit 1; }
	for f in qvr_rendezvous.py test_rendezvous.py qvr-rendezvous.service; do
		git -C "$REPO" show "$REV:Misc/rendezvous/$f" > "$STAGE/$f"
	done
	cp "$HERE/deploy.sh" "$STAGE/deploy.sh"  # (today's installer: it installs whichever files sit beside it)
	SOURCE="commit $(git -C "$REPO" rev-parse --short "$REV")"
else
	cp "$HERE/qvr_rendezvous.py" "$HERE/test_rendezvous.py" "$HERE/qvr-rendezvous.service" "$HERE/deploy.sh" "$STAGE/"
	SOURCE="this checkout"
fi
WANT=$(version_of "$STAGE/qvr_rendezvous.py")
[ -n "$WANT" ] || { echo "update: no VERSION line in the service from $SOURCE"; exit 1; }
echo "update: the room service from $SOURCE: protocol version $WANT"

# Step 1: its own tests, on these very files (-I: nothing of this folder's or the user's environment imported but them).
echo "update: running its tests (test_rendezvous.py)"
if ! (cd "$STAGE" && python3 -I test_rendezvous.py); then
	echo "update: FAILED: the tests failed; nothing deployed"
	exit 1
fi

if [ "$MODE" = dry ]; then
	echo "update: dry run: would deploy version $WANT to $TARGET with deploy.sh, then check it; nothing sent"
	exit 0
fi

# Step 2: install and restart.
bash "$STAGE/deploy.sh" "$HOST" ${SSH_USER:+"$SSH_USER"}

# Step 3: the server's own answer.
check_server "$WANT"
