#! /bin/sh

set -e
set -u

if ! test -f README.md
then
    echo "run from the 9cc base directory." >&2
    exit 1
fi

if ! test -f ./9cc
then
    echo "please build 9cc first." >&2
    exit 1
fi

run9cc="./9cc"
if which valgrind > /dev/null
then
    # Detect bad memory accesses, but ignore leaks.
    run9cc="valgrind --leak-check=no --quiet ./9cc"
else
     echo "not using valgrind..." >&2
fi

testcount=0
passcount=0

# Run an exec test:
# Check the runs with gcc, save+cache output.
# Check the test compiles (with valgrind).
# Check the test assembles.
# Check the test returns with rc=0.
# Check the test output matches gcc.
exectest () {
    t="$1"
    echo -n "$t: "

    if ! test -f "$t.expected"
    then
        if ! gcc -g -O0 "$t" -o "$t.gccbin" 2>/dev/null
        then
            echo "fail(didn't compile with gcc)"
            return 1
        fi

        if ! "$t.gccbin" > "$t.expected"
        then
            rm "$t.expected"
            echo "fail(test didn't have rc=0 when built with gcc)"
            return 1
        fi
    fi

    if ! $run9cc "$t" 2> /dev/null > "$t.s"
    then
        echo "fail(didn't compile with 9cc)"
        return 1
    fi

    if ! gcc -o "$t.bin" "$t.s"
    then
        echo "fail(9cc output didn't assemble)"
        return 1
    fi

    if ! "$t.bin" > "$t.output"
    then
        echo "fail(bad exit rc=$?)"
        return 1
    fi

    if ! diff -u "$t.expected" "$t.output" > "$t.diff"
    then
        echo "fail(output differs)"
        cat "$t.diff"
        return 1
    fi

    echo "ok"
}

counttest () {
    if $1 $2
    then
        passcount=$(($passcount+1))
    fi
    testcount=$(($testcount+1))
}

testdir="./test/exec"
for t in $testdir/*.c
do
    counttest exectest $t
done

echo "$0 passed $passcount/$testcount"
