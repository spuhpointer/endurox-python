#
# Test utils
#

import time
import os

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
# Return current test settings
# 
def test_duratation():
    duratation = os.getenv('NDRXPY_TEST_DURATATION') or '30'
    return int(duratation)

