#!/bin/sh
# Runs every tests/Tnn.in through ./acaderp and compares with tests/Tnn.out.
# Usage:  ./run_tests.sh            (build first with: make)
# Prints one PASS/FAIL line per test and the pass count at the end.
HERE=$(cd "$(dirname "$0")" && pwd)
# The binary may sit next to this script (root make) or in src/ (cd src && make).
if   [ -x "$HERE/acaderp" ];     then BIN="$HERE/acaderp"
elif [ -x "$HERE/src/acaderp" ]; then BIN="$HERE/src/acaderp"
else echo "acaderp not built - run 'make' first"; exit 2; fi
WORK=$(mktemp -d)            # SAVE/LOAD tests write their files here, not in the repo
pass=0; fail=0
for input in "$HERE"/tests/T*.in; do
    name=$(basename "$input" .in)
    expected="$HERE/tests/$name.out"
    actual="$WORK/$name.actual"
    (cd "$WORK" && "$BIN" < "$input" > "$actual")
    if diff -u "$expected" "$actual" > "$WORK/$name.diff"; then
        pass=$((pass + 1))
        echo "PASS $name"
    else
        fail=$((fail + 1))
        echo "FAIL $name"
        head -20 "$WORK/$name.diff"
    fi
done
rm -rf "$WORK"
echo "----"
echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
