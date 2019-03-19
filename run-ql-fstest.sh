#!/bin/sh

set -e

CWD=$(realpath $(dirname $0))
PID=$$

theusage()
{
    cat <<EOF
run-ql-fstest.sh [options ] -d <targetdir>
options:
-d <target-dir>         Dir to run ql-fstest in
-e                      Stop on error - fail immediately
-n <num_processes>      Number of processes to start, defaults to 1
-p <fill-level>         File system fill level, ql-fstest will write data up
                        to that level (approximate, with multiple processes),
                        defaults to 90
-t <max-run-time>       How log to run (default: unlimited)
-h                      This help
EOF
    exit 1
}

opts=""

targetdir=""
nproc=1

while getopts "d:en:p:t:h" opt; do
    case $opt in
        d)
            targetdir=$OPTARG
            ;;
        e)
            opts="$opts --error-stop"
            ;;
        n)
            nproc=$OPTARG
            ;;
        p)
            opts="$opts -p $OPTARG"
            ;;
        t)
            opts="$opts --timeout=$OPTARG"
            ;;
        h)
            theusage
            ;;
    esac
done


if [ -z "${targetdir}" ]; then
    echo
    echo "Missing required argument: -d <targetdir"
    echo
    theusage
fi

mkdir -p ${targetdir}

dir=`dirname $0`
cd $dir
export PATH=$PATH:$dir

if [ -z "$1" ]; then
	echo
	echo "usage: `basename $0` <directory>"
	echo
	exit 1
fi

#make

#tmp script, run from screen
tmprun="$targetdir/tmp-ql-fstest-run-$$.sh"
cat <<EOF >${tmprun}
${CWD}/fstest $opts $targetdir 2>${targetdir}/fstest-\$\$.err | tee ${targetdir}/fstest-\$\$.log
EOF
chmod +x ${tmprun}

# create a screenrc with the given number of ql-fstest processes
screenrc="${targetdir}/tmp-screenrc_ql_fstest-$$"
cp ${dir}/screenrc_ql_fstest.in ${screenrc}

# add commands to the screenrc
for i in $(seq 1 $nproc); do
cat <<EOF >> $screenrc
screen -t ql-fstest ${tmprun}
EOF
done

echo "Starting tests in screen session"
screen -S ql_fstests -dm -c $screenrc

