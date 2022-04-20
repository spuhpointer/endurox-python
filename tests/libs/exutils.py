#
# Test utils
#

import time

#
# Stopwatch used by tests
#
class NdrxStopwatch(object):

    w = time.time()
    
    #
    # get time spent
    # 
    def get_delta_sec(self):
        return time.time() - self.w

    #
    # reset time
    #
    def reset(self):
        self.w = time.time()

#
# Get test settings
#
#class TestSettings(object):

    #
    # Return current test settings
    #
#    def test_duratation(self):
#        os.getenv('NDRXPY_TEST_SEC')
