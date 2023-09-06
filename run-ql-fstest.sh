#!/bin/sh

set -e

CWD=$(realpath $(dirname $0))
PID=$$
nproc=8

theusage()
{
    cat <<EOF
run-ql-fstest.sh [options ] -d <targetdir>
options:
-d <target-dir>         Dir to run ql-fstest in
-D                      random direct IO
-e                      Stop on error - fail immediately
-n <num_processes>      Number of processes to start, defaults to ${nproc}
-p <fill-level>         File system fill level, ql-fstest will write data up
                        to that level (approximate, with multiple processes),
                        defaults to 90
-t <max-run-time>       How log to run (default: unlimited)
-h                      This help
-l <log-dir>            Dir to write log files to
EOF
    exit 1
}

opts=""

targetdir=""

while getopts "d:Den:p:t:hl:" opt; do
    case $opt in
        d)
            targetdir=$OPTARG
            ;;
        D) opts="$opts --directIO"
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
        l) logdir="${OPTARG}"
            ;;
    esac
done


if [ -z "${targetdir}" ]; then
    echo
    echo "Missing required argument: -d <targetdir"
    echo
    theusage
fi

if [ -z "${logdir}" ]; then
    logdir="${targetdir}";
fi

mkdir -p ${targetdir}
mkdir -p ${logdir}

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
${CWD}/fstest $opts $targetdir 2>${logdir}/fstest-\$\$.err | tee ${logdir}/fstest-\$\$.log
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

