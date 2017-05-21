#!/bin/sh
## Script to run some checkout benchmarks.
## CD to this directory and run this script from here.
##################################################################

set -vx


. ./__setup.sh || exit 1


## A short name for this script. We use this to separate
## the log files from other benchmarks.
XX_BM=`basename $0 .sh`

## All log/output for this run will be in the following directory.
XX_LOG_DIR=$BM_LOGS_DIR/$XX_BM/`/bin/date +%Y%m%d.%H%M%S`
[ -d $XX_LOG_DIR ] || mkdir -p $XX_LOG_DIR

## Perf data goes here.  Subsequent runs are APPENDED to this file.
XX_LOG=$XX_LOG_DIR/run.log

## How many times to run the tests (passed into the benchmark).
XX_COUNT=3

## A list of commits to checkout and cycle thru.
XX_COMMITS="355d4e1 8004647 355d4e1 8004647"

## Also run benchmark using git.exe in the body of the test.
XX_USEGIT=--git

## Verbose setting within benchmark.
##XX_VERBOSE=-v

## How many times to call status after merge/checkout completes.
XX_STATUS=5

## Write details of the current libgit2 branch to the log before
## we run the benchmark.
bm_get_info >> $XX_LOG

date
time $BM_EXE $XX_VERBOSE -c $XX_COUNT --logfile $XX_LOG $XX_USEGIT checkoutn -s $XX_STATUS --autocrlf $BM_LURL $XX_COMMITS
date


##################################################################
## Dump the stats to the console so they don't have to look for it.

echo "################################################################"
echo "################################################################"
echo
cat $XX_LOG
