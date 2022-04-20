import unittest
import endurox as e
import exutils as u
import gc
import pprint

class TestView(unittest.TestCase):

    # Test no space.
    def test_view_nospace(self):
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < 30:
            try:
                tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "buftype":"VIEW", "subtype":"UBTESTVIEW2", "data":{
                    "tstring1": "HELLO WORLD ANOTHER WORLD..............."
                    }},);
            except e.ubfException as ex:
                self.assertEqual(ex.code,4)
                self.assertTrue("No space in" in ex.args[0]) 
            else:
                self.assertEqual(True,False)
if __name__ == '__main__':
    unittest.main()
