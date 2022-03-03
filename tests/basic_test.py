import unittest
import endurox

class MainTest(unittest.TestCase):

    def test_tpterm(self):
        try:
            endurox.tpterm()
        except endurox.XatmiException:
            self.fail("tpterm() raised AtmiException")

if __name__ == '__main__':
    unittest.main()
