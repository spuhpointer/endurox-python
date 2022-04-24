#!/usr/bin/env python3

import sys

import endurox as e

class Server:

    def tpsvrinit(self, args):
        e.userlog('Server startup')
        e.tpadvertise('FAILSVC', 'FAILSVC', self.FAILSVC)
        e.tpadvertise('OKSVC', 'OKSVC', self.OKSVC)
        e.tpadvertise('FWDSVC', 'FWDSVC', self.FWDSVC)
        e.tpadvertise('EVSVC', 'EVSVC', self.EVSVC)

        # subscribe to TESTEV event.
        e.tplog_info("ev subs %d" % e.tpsubscribe('TESTEV', None, e.TPEVCTL(name1="EVSVC", flags=e.TPEVSERVICE)))
        
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
    # Forwarding service
    #
    def FWDSVC(self, args):
        # alter some data, in case if buffer type if UBF
        if args.data["buftype"]=="UBF":
            args.data["data"]["T_STRING_3_FLD"]=args.data["data"]["T_STRING_FLD"][0]
        return e.tpforward("OKSVC", args.data, 0)

    #
    # Just consume event, return NULL buffer.
    #
    def EVSVC(self, args):
        return e.tpreturn(e.TPSUCCESS, 0, {})

    #
    # TODO: run conversational data
    #

if __name__ == '__main__':
    e.run(Server(), sys.argv)

