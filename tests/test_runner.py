import unittest
import endurox
import os
import subprocess

class MainTest(unittest.TestCase):

    def test001_buffers(self):
        testdir=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test001_buffers');
        subprocess.check_call(os.path.join(testdir, 'run.sh'), cwd=testdir)

    def test002_xatmiipc(self):
        testdir=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test002_xatmiipc');
        subprocess.check_call(os.path.join(testdir, 'run.sh'), cwd=testdir)

    def test003_tmq(self):
        testdir=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test003_tmq');
        subprocess.check_call(os.path.join(testdir, 'run.sh'), cwd=testdir)

    def test004_util(self):
        testdir=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test004_util');
        subprocess.check_call(os.path.join(testdir, 'run.sh'), cwd=testdir)

if __name__ == '__main__':
    unittest.main()
