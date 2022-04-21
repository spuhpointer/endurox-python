
import endurox as e
import json

#
# VIEW echo test
#
tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "buftype":"VIEW", "subtype":"UBTESTVIEW2", "data":{
    "tshort1":[1,1,2,5]
    ,"tchar1":[0]
}})

#json_object = json.dumps(retbuf, indent = 4) 
print(retbuf)

