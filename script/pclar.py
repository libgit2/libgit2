#!/usr/bin/env python

from __future__ import print_function, unicode_literals

"""Parallel runner for clar
"""

import sys, time
from os import path
import subprocess

def parse_suites(exe):
    suites = subprocess.check_output([exe, '-l'])
    return map(lambda s: s.split(' ')[1], # get the name from each line of output
               filter(lambda s: s, # Remove any empty lines
                      map(lambda s: s.strip(), # remove leading whitespace
                          suites.split('\n')[3:]))) # skip first three which are info messages

def start_process(exe, suites):
    argv = [exe]
    for suite in suites:
        argv.append("-s{}".format(suite))

    return subprocess.Popen(argv, stdout=subprocess.PIPE)

if __name__ == '__main__':
    from argparse import ArgumentParser

    parser = ArgumentParser(description="Parallel runner for clar")
    parser.add_argument('clar', type=unicode, help="Clar runner")
    parser.add_argument('-j', '--jobs', type=int, default=1,
                        help="Number of parallel clars to run")
    args = parser.parse_args()

    if not args.jobs > 0:
        print("fatal: umber of jobs must be positive", file=sys.stderr)
        exit(1)

    if not path.isfile(args.clar):
        print("fatal: clar runner not found", file=sys.stderr)
        exit(1)

    suites = parse_suites(args.clar)
    plural = "" if args.jobs == 1 else "es"
    print("Loaded {} suites, using {} process{}".format(len(suites), args.jobs, plural))

    # Split the suites into groups so we can give the list to the
    # runners. The last one gets a few more in case the number of
    # suites does not split evenly into the runners
    indices = range(0, len(suites)+1, len(suites) / args.jobs)
    indices[-1] = len(suites)

    procs = []
    for i in range(1, len(indices)):
        lower = indices[i-1]
        upper = indices[i]

        procs.append(start_process(args.clar, suites[lower:upper]))

    # Processes have been started
    print("Started processes: {}".format(map(lambda p: p.pid, procs)))

    while True:
        running = len(filter(lambda p: p.poll() is None, procs))
        if running == 0:
            break

        sys.stdout.write("\r{}Running {}".format("\033[K", running))
        sys.stdout.flush()
        time.sleep(1)

    failures = len(filter(lambda p: p.poll() < 0, procs))
    errors = sum(filter(lambda p: p.poll() > 0, procs))
    successes = filter(lambda p: p.poll() == 0, procs)

    print("")
    print("errors: {}, failures: {}".format(errors, failures))
