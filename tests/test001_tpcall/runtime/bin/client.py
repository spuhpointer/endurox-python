import unittest
import endurox as e

class TestTpcall(unittest.TestCase):

    # TODO: we add some testing here
    # also check that memory leaks are not present.
    # intially probably with xmemck.
    def test_ubf_tpcall(self):
        for i in range(0,999999999):
            e.tpcall("UBFTEST", {
                "T_CHAR_2_FLD": ["X", "Y"],
                "T_STRING_FLD": "HELLO INPUT",
                },);
        #self.assertEqual('foo'.upper(), 'FOO')

if __name__ == '__main__':
    unittest.main()
