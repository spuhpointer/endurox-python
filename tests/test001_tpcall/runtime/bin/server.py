#!/usr/bin/env python3

import os
import sys
import gc

import endurox as e
import json

class Server:
    def tpsvrinit(self, args):
        e.userlog('Server startup')
        e.tpadvertise('UBFTEST', 'UBFTEST', self.UBFTEST)
        e.tpadvertise('ECHO', 'ECHO', self.ECHO)
        return 0

    def tpsvrdone(self):
        e.userlog('Server shutdown')

    #
    # Run some echo
    #
    def UBFTEST(self, args):
        args.data["data"]['T_STRING_FLD']=['HELLO FROM SERVER']
        args.data["data"]['T_STRING_FLD'].append('hello 2')
        args.data["data"]['T_CHAR_FLD']=args.data["data"]['T_CHAR_2_FLD']

        return e.tpreturn(e.TPSUCCESS, 0, args.data)

    # 
    # return the same request data back
    #
    def ECHO(self, args):
        #json_object = json.dumps(args.data, indent = 4) 
        #e.userlog(json_object+"")
        print(args.data)
        return e.tpreturn(e.TPSUCCESS, 0, args.data)

if __name__ == '__main__':
    e.run(Server(), sys.argv)
