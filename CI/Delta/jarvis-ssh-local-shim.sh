#!/bin/bash
# =============================================================================
# jarvis ssh shim for Delta: run LOCAL-host commands without ssh.
#
# Delta compute-node sshd denies key auth even to localhost / the node's own
# name ("Permission denied ()"), so jarvis's PsshExec (which ssh's to every
# host in the hostfile, including localhost, with no local short-circuit) fails
# to start the clio runtime/CTE daemons and to mkdir the output dirs.
#
# jarvis builds each remote launch as:
#     <ssh_cmd> -o BatchMode=yes -o ... <host> '<remote command>'
# and lets the pipeline override <ssh_cmd> at the top level. Point ssh_cmd
# (and pssh_cmd) at THIS script: when <host> is the local node it execs the
# command directly (bash -lc), otherwise it falls through to real ssh with the
# original arguments untouched.
#
# Wire it into the loaded pipeline's pipeline.yaml (top level):
#     ssh_cmd:  /u/hxu13/software/coeus-adapter/CI/Delta/jarvis-ssh-local-shim.sh
#     pssh_cmd: /u/hxu13/software/coeus-adapter/CI/Delta/jarvis-ssh-local-shim.sh
# =============================================================================
set -uo pipefail

# Preserve the exact original argv for the real-ssh fallback.
declare -a orig=("$@")

# Names that mean "this machine"
me_fqdn="$(hostname)"
me_short="$(hostname -s)"
is_local_host() {
    case "$1" in
        localhost|127.0.0.1|::1|"$me_fqdn"|"$me_short") return 0 ;;
        *) [[ "$1" == "$me_short".* ]] && return 0 ;;   # cn024.delta... == cn024
    esac
    return 1
}

# Walk the ssh-style argv: skip options, find the first bare token (the host);
# everything after it is the remote command (jarvis passes it as one arg).
host=""
declare -a rest=()
while (($#)); do
    case "$1" in
        -o|-p|-i|-l|-F|-c|-b|-e) shift 2; continue ;;   # option that takes a value
        -*) shift; continue ;;                          # bare flag
        *)
            host="$1"; shift
            rest=("$@")
            break
            ;;
    esac
done

if [[ -z "$host" ]]; then
    echo "jarvis-ssh-local-shim: no host in args" >&2
    exit 2
fi

if is_local_host "$host"; then
    # Local: run the remote command directly (jarvis already injects the
    # needed cd/env prefix into the command string). rest is normally one arg.
    exec bash -c "${rest[*]}"
else
    # Remote: Delta denies node-to-node ssh, but we're inside a SLURM
    # allocation, so run the command on the target node via an OVERLAPPING
    # srun step (shares CPUs with the app step; no ssh). Used e.g. to start the
    # clio runtime/CTE daemon on a second producer node. The daemon runs in the
    # foreground of clio_run, so the srun step stays alive holding it.
    JID="${SLURM_JOB_ID:-$(squeue -u "$USER" -h -o '%i' 2>/dev/null | head -1)}"
    exec srun --jobid="$JID" --nodes=1 --ntasks=1 --nodelist="$host" \
         --overlap --mem=0 bash -c "${rest[*]}"
fi
