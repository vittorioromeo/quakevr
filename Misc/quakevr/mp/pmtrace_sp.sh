# B11b: one single-player scenario run with vr_pmove_own 0 and 1, the pmtrace lines compared frame by frame.
# usage: bash pmtrace_sp.sh <label> "<script, with V for the cvar's value>" [extra run.sh args...]
K=C:/OHWorkspace/qvr-kit
W=${W:-C:/OHWorkspace/qvr-agents/mp}
label=$1
script=$2
shift 2
for v in 0 1; do
  s=${script//V/$v}
  bash $K/run.sh ${AGENT:-mp} -Script "$s" -Filter "pmtrace|exit=|TIMEOUT|ENGINE|b11b" -Timeout 300 "$@" > $W/scratch/b11b_${label}_$v.txt 2>&1
  grep -c pmtrace $W/scratch/b11b_${label}_$v.txt
  grep -E "exit=|TIMEOUT|ENGINE" $W/scratch/b11b_${label}_$v.txt | head -3
done
python $W/Misc/quakevr/mp/pmtrace_cmp.py $W/scratch/b11b_${label}_0.txt $W/scratch/b11b_${label}_1.txt $label
