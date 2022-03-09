#!/usr/bin/env python3

import os
import sys
import gc

import endurox as e

class Server:
    def tpsvrinit(self, args):
        e.userlog('Server startup')
        e.tpadvertise('UBFTEST', 'UBFTEST', self.UBFTEST)
        return 0

    def tpsvrdone(self):
        e.userlog('Server shutdown')

    #
    # Run some echo
    #
    def UBFTEST(self, args):
#        e.userlog(args.name)
#        e.userlog(args.data['T_STRING_FLD'][0])
        args.data["data"]['T_STRING_FLD']=['HELLO FROM SERVER']
        args.data["data"]['T_STRING_FLD'].append('hello 2')
        args.data["data"]['T_CHAR_FLD']=args.data["data"]['T_CHAR_2_FLD']
#        gc.collect()

        return e.tpreturn(e.TPSUCCESS, 0, args.data)

if __name__ == '__main__':
    e.run(Server(), sys.argv)
