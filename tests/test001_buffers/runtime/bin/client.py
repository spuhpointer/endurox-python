import unittest
import endurox as e
import exutils as u
import gc

class TestTpcall(unittest.TestCase):

    # TODO: we add some testing here
    # also check that memory leaks are not present.
    # intially probably with xmemck.
    # TODO: Test all data types.
    def test_ubf_tpcall(self):
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < 30:
            tperrno, tpurcode, retbuf = e.tpcall("UBFTEST", { "data":{
                "T_CHAR_2_FLD": ["X", "Y"],
                "T_STRING_FLD": "HELLO INPUT",
                "T_STRING_2_FLD": "HELLO INPUT 2",
                "T_UBF_FLD": {"T_SHORT_FLD":99, "T_UBF_FLD":{"T_LONG_2_FLD":1000091}}
                }},);
            self.assertEqual(tperrno, 0)
            self.assertEqual(tpurcode, 0)
            self.assertEqual(retbuf["data"]["T_STRING_FLD"][0], "HELLO FROM SERVER")
            self.assertEqual(retbuf["data"]["T_STRING_FLD"][1], "hello 2")
            self.assertEqual(retbuf["data"]["T_STRING_2_FLD"][0], "HELLO INPUT 2")
            self.assertEqual(retbuf["data"]["T_UBF_FLD"][0]["T_SHORT_FLD"][0], 99)

if __name__ == '__main__':
    unittest.main()
