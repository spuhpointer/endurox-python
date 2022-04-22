#!/usr/bin/env python3

import sys

import endurox as e

class Server:

    def tpsvrinit(self, args):
        e.userlog('Server startup')
        e.tpadvertise('FAILSVC', 'FAILSVC', self.FAILSVC)
        e.tpadvertise('OKSVC', 'OKSVC', self.OKSVC)
        return 0

    def tpsvrdone(self):
        e.userlog('Server shutdown')

    # 
    # return failure + tpurcode
    #
    def FAILSVC(self, args):
        if args.data["buftype"]=="UBF":
            args.data["data"]["T_STRING_2_FLD"]=args.data["data"]["T_STRING_FLD"][0]
        return e.tpreturn(e.TPFAIL, 5, args.data)

    #
    # return some ok
    #
    def OKSVC(self, args):
        # alter some data, in case if buffer type if UBF
        if args.data["buftype"]=="UBF":
            args.data["data"]["T_STRING_2_FLD"]=args.data["data"]["T_STRING_FLD"][0]
        return e.tpreturn(e.TPSUCCESS, 5, args.data)

    #
    # TODO: Check service failure
    #

    #
    # TODO: Check events...
    #

    #
    # TODO: run conversational data
    #

if __name__ == '__main__':
    e.run(Server(), sys.argv)

