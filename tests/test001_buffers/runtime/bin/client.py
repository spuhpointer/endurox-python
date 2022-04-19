import unittest
import endurox as e
import exutils as u
import gc

class TestUbf(unittest.TestCase):

    # TODO: we add some testing here
    # also check that memory leaks are not present.
    # intially probably with xmemck.
    # TODO: Test all data types.
    def test_ubf_tpcall(self):
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < 30:
            tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "data":{
                "T_CHAR_FLD": ["X", "Y"],
                "T_SHORT_FLD": 32000,
                "T_LONG_FLD": 999999,
                "T_FLOAT_FLD": 55.9,
                "T_DOUBLE_FLD": 555.9,
                "T_STRING_FLD": "HELLO INPUT",
                "T_CARRAY_FLD": [b'\x00\x03\x05\x07', b'\x00\x00\x05\x07'],
                "T_UBF_FLD": {"T_CHAR_2_FLD":"X"},
                "T_VIEW_FLD": [ {}, {"vname":"UBTESTVIEW2", "data":{
                    "tshort1":5
                    ,"tcarray1":[b'\x00\x00', b'\x01\x01'] } }],
                "T_PTR_FLD":{"data":{"T_CHAR_FLD":"X"}},
                "T_PTR_FLD":{"data":"HELLO WORLD"},
    #            "T_PTR_2_FLD":[{"data":"HELLO WORLD"}, {"data":{"T_STRING_FLD":"HELLO WORLD"}}, {"buftype":"VIEW", "subtype":"UBTESTVIEW2", "data":{"tshort1":99}}],
                }},);
            self.assertEqual(tperrno, 0)
            self.assertEqual(tpurcode, 0)
            print(retbuf)
            self.assertEqual(retbuf["data"]["T_CHAR_FLD"][0], "X")
            self.assertEqual(retbuf["data"]["T_CHAR_FLD"][1], "Y")
            self.assertEqual(retbuf["data"]["T_SHORT_FLD"][0], 32000)
            self.assertEqual(retbuf["data"]["T_LONG_FLD"][0], 999999)
            self.assertAlmostEqual(retbuf["data"]["T_FLOAT_FLD"][0], 55.9, places=3, msg=None, delta=None)
            self.assertAlmostEqual(retbuf["data"]["T_DOUBLE_FLD"][0], 555.9, places=3, msg=None, delta=None)
            self.assertEqual(retbuf["data"]["T_STRING_FLD"][0], "HELLO INPUT")
            self.assertEqual(retbuf["data"]["T_VIEW_FLD"][1]["data"]["tshort1"][0], 5)
            self.assertEqual(retbuf["data"]["T_VIEW_FLD"][1]["data"]["tcarray1"][0], b'\x00\x00')
            self.assertEqual(retbuf["data"]["T_VIEW_FLD"][1]["data"]["tcarray1"][1], b'\x01\x01')
            #self.assertEqual(retbuf["data"]["T_STRING_FLD"][1], "hello 2")
            #self.assertEqual(retbuf["data"]["T_STRING_2_FLD"][0], "HELLO INPUT 2")
            #self.assertEqual(retbuf["data"]["T_UBF_FLD"][0]["T_SHORT_FLD"][0], 99)

if __name__ == '__main__':
    unittest.main()
