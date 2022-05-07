import unittest
import endurox as e
import exutils as u

class TestTpencrypt(unittest.TestCase):


    # Test data encryption
    def test_tpencrypt_ok(self):
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < u.test_duratation():
            
            buf=e.tpencrypt(b'\x00\x00\xff')
            self.assertNotEqual(buf, '\x00\x00\xff')

            buf_org=e.tpdecrypt(buf)
            self.assertNotEqual(buf_org, '\x00\x00\xff')

if __name__ == '__main__':
    unittest.main()

