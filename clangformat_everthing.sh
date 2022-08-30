#!/bin/bash
for f in $(find ./Quake | grep pp | grep -v json | grep -v bak); do
    echo $f
    clang-format -i $f &
done

wait
echo "Done"

# find ./Quake | grep pp | grep -v bak | xargs clang-format -i
