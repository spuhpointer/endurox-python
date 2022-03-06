"""Python3 bindings for writing Endurox clients and servers"""

import sys
import ctypes
#import pkgutil
#import imp

flags = sys.getdlopenflags()

# Need as Enduro/X XA drivers are dynamically loaded, and
# they need to see Enduro/X runtime.
sys.setdlopenflags(flags | ctypes.RTLD_GLOBAL)

from .endurox import *

#import endurox.endurox
#sys.modules['endurox'] = endurox.endurox
#sys.modules['endurox'].__name__='endurox';

# change module name... for importeds symbols
sys.setdlopenflags(flags)

__all__ = ['endurox']
