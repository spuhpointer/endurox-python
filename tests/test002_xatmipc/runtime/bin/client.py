import unittest
import endurox as e
import exutils as u
import gc

class TestTpcall(unittest.TestCase):

    # Validate tperrno, tpurcode, server UBF processing
    def test_tpcall_ok(self):
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < u.test_duratation():
            print("Into test_tpcall_ok\n")
            tperrno, tpurcode, retbuf = e.tpcall("OKSVC", { "data":{"T_STRING_FLD":"Hi Jim"}})
            self.assertEqual(tperrno, 0)
            self.assertEqual(tpurcode, 5)
            self.assertEqual(retbuf["data"]["T_STRING_2_FLD"][0], "Hi Jim")

    # validate error handling
    def test_tpcall_fail(self):
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < u.test_duratation():
            print("Into test_tpcall_fail\n")
            tperrno, tpurcode, retbuf = e.tpcall("FAILSVC", { "data":{"T_STRING_FLD":"Hi Jim"}})
            self.assertEqual(tperrno, e.TPESVCFAIL)
            self.assertEqual(tpurcode, 5)
            # check that still data is returned
            self.assertEqual(retbuf["data"]["T_STRING_2_FLD"][0], "Hi Jim")
            

if __name__ == '__main__':
    unittest.main()

