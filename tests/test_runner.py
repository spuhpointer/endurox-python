import unittest
import endurox
import os
import subprocess

class MainTest(unittest.TestCase):

    def test001_tpcall(self):

        testdir=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test001_tpcall');
        subprocess.check_call(os.path.join(testdir, 'run.sh'), cwd=testdir)

if __name__ == '__main__':
    unittest.main()
