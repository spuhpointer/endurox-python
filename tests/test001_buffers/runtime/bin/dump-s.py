
import endurox as e
import json

#
# VIEW echo test
#
tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "buftype":"STRING", "data":"HELLO" })

#json_object = json.dumps(retbuf, indent = 4) 
print(retbuf)

