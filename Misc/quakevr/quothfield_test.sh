# quothfield_test.sh: vr_ad_test 223 on Quoth's kellbase1 (vr_quoth_test.qc): its hurting togglewall draws its particle
# field and an armed enforcer behind it keeps off it. Needs Quoth in the test base (quoth_setup.py). Ends on
# "quothfield: N passed, M failed".
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
S="developer 0;sv_random_seed 7;skill 1;vr_campaign_native quoth;wait30;map kellbase1;wait30;god;vr_ad_test 223;wait3000;toggleconsole;quit"
bash "$K/run.sh" "$NAME" -Script "$S" -Filter "quothfield|Host_Error|SV_Error" -QuitOn "quothfield: [0-9]+ passed" -Timeout 300 "$@"
