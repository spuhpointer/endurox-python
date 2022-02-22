"""Root module of your package"""

import sys
import os
import time
import ctypes

flags = sys.getdlopenflags()

# Need as Enduro/X XA drivers are dynamically loaded, and
# they need to see Enduro/X runtime.
sys.setdlopenflags(flags | ctypes.RTLD_GLOBAL)
from ._endurox import *
# Restore original flags
sys.setdlopenflags(flags)

#__all__ = ['endurox']
