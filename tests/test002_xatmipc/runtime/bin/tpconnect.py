import unittest
import endurox as e
import exutils as u
import gc

class TestTpconv(unittest.TestCase):

    # Validate async call by cd
    def test_conversation(self):
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < u.test_duratation():
            cd = e.tpconnect("CONVSVC", {}, e.TPSENDONLY)
            rc, ev = e.tpsend(cd, {"data":{"T_STRING_FLD":"From client"}}, e.TPRECVONLY)
            self.assertEqual(rc, 0)
            self.assertEqual(ev, 0)

            rc, ev, tpurcode, retbuf = e.tprecv(cd)
            self.assertEqual(rc, 0)
            self.assertEqual(ev, 0)
            self.assertEqual(retbuf["data"]["T_STRING_FLD"][0], "From server")

            # receive again
            rc, ev, tpurcode, retbuf = e.tprecv(cd)
            self.assertEqual(rc, e.TPEEVENT)
            self.assertEqual(ev, e.TPEV_SVCSUCC)
            self.assertEqual(tpurcode, 6)
            self.assertEqual(retbuf["data"]["T_STRING_FLD"][0], "From server 2")

if __name__ == '__main__':
    unittest.main()

