
import endurox as e
import json

#
# Support for VIEWS
# Support for PTR, when sending out -> allocate bunch of PTR buffers
# PTR leak? On receiving rsp same buffer?
# When parsed rsp in -> free all ptr. Also free all ptr original buffers.
#
tperrno, tpurcode, retbuf = e.tpcall("ECHO", {})

#tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "buftype":"CARRAY", "data":"HELLO WORLD"})
#tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "data":{}})
print(retbuf)

