
import endurox as e
import json

#
# VIEW echo test
#
tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "buftype":"VIEW", "subtype":"UBTESTVIEW2", "data":{
    "tshort1":5
    ,"tchar1":"a\x00"
}})

#json_object = json.dumps(retbuf, indent = 4) 
print(retbuf)

