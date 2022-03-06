#!/bin/bash

#
# @(#) Test 001 - Python tpcall tests
#

#
# Load system settings...
#
source ~/ndrx_home
export PYTHONPATH=`pwd`/../libs

TIMES=200
pushd .
rm -rf runtime/log
mkdir runtime/log

cd runtime

export NDRX_SILENT=Y
MACHINE_TYPE=`uname -m`
OS=`uname -s`

runbigmsg=0
#
# So we need to add some demo server
# We need to add server process here + we need to register ubftab (test.fd)
#
msgsizemax=56000

echo "OS=[$OS] matchine=[$MACHINE_TYPE]"
# Added - in front of freebsd, as currently not possible to use large messages with golang...
if [[ ( "X$OS" == "XLinux" || "X$OS" == "X-FreeBSD" ) && ( "X$MACHINE_TYPE" == "Xx86_64" || "X$MACHINE_TYPE" == "Xamd64" ) ]]; then
        echo "Running on linux => Using 1M message buffer"
        # set to 1M + 1024
        msgsizemax=1049600
	ulimit -s 30751
        runbigmsg=1
fi

echo "Message size: $msgsizemax bytes"

xadmin provision -d \
        -vaddubf=test.fd \
        -vtimeout=15 \
        -vinstallQ=n \
        -vmsgsizemax=$msgsizemax

cd conf

. settest1

# So we are in runtime directory
cd ../bin
# Be on safe side...
unset NDRX_CCTAG 
xadmin start -y
xadmin psc
#
# Generic exit function
#
function go_out {
    echo "Test exiting with: $1"
    xadmin stop -y
    xadmin down -y

    popd 2>/dev/null
    exit $1
}

################################################################################
echo "Running UBF test"
################################################################################

python3 -m unittest client.py

RET=$?

if [[ $RET != 0 ]]; then
    echo "testcl $COMMAND: failed"
    go_out -1
fi

###############################################################################
echo "Done"
###############################################################################

# go_out will shutdown
#xadmin stop -c -y


go_out 0


