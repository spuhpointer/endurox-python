import unittest
import endurox as e
import os
import time


#
# Check text exists in file
#
def chk_file(fname, t):
    with open(fname) as f:
        if t in f.read():
            return 1
    return 0

class TestTplog(unittest.TestCase):

    def test_tplog_ok(self):
        # load the env...
        e.tpinit()

        filename = "%s/tplog_ok" % e.tuxgetenv('NDRX_ULOG')
        os.remove(filename) if os.path.exists(filename) else None
        # set logfile output
        e.tplogconfig(e.LOG_FACILITY_TP, e.log_info, None, "TEST", filename)

        e.tplog_error("HELLO ERROR")
        self.assertEqual(chk_file(filename, "HELLO ERROR"), 1)

        e.tplog_always("HELLO ALWAYS")
        self.assertEqual(chk_file(filename, "HELLO ALWAYS"), 1)

        e.tplog_warn("HELLO WARNING")
        self.assertEqual(chk_file(filename, "HELLO WARNING"), 1)

        e.tplog_info("HELLO INFO")
        self.assertEqual(chk_file(filename, "HELLO INFO"), 1)

        e.tplog_debug("HELLO DEBUG")
        self.assertEqual(chk_file(filename, "HELLO DEBUG"), 0)

        e.tplog(e.log_error, "HELLO ERR2")
        self.assertEqual(chk_file(filename, "HELLO ERR2"), 1)

        e.tplogconfig(e.LOG_FACILITY_TP, -1, "tp=5", None, None)
        e.tplog_debug("HELLO DEBUG")
        self.assertEqual(chk_file(filename, "HELLO DEBUG"), 1)

        e.tpterm()

    # request logging...
    def test_tplog_reqfile(self):
        e.tpinit()
        filename_def = "%s/tplog_req_def" % e.tuxgetenv('NDRX_ULOG')
        filename_th = "%s/tplog_req_th" % e.tuxgetenv('NDRX_ULOG')
        filename = "%s/tplog_req" % e.tuxgetenv('NDRX_ULOG')
        os.remove(filename) if os.path.exists(filename) else None
        os.remove(filename_def) if os.path.exists(filename_def) else None

        e.tplogconfig(e.LOG_FACILITY_TP, e.log_info, "file=%s" % filename_def, "TEST", None)

        out = e.tplogsetreqfile({"data":{"EX_NREQLOGFILE":filename}}, None, None)
        self.assertEqual(e.tploggetreqfile(), filename)
        self.assertEqual(e.tploggetbufreqfile(out), filename)
        out = e.tplogdelbufreqfile(out)

        with self.assertRaises(e.XatmiException):
            e.tploggetbufreqfile(out)

        e.tplog_error("HELLO ERROR")
        self.assertEqual(chk_file(filename, "HELLO ERROR"), 1)

        e.tplogclosereqfile()
        self.assertEqual(e.tploggetreqfile(), "")
        
        e.tplogsetreqfile_direct(filename)
        e.tplog_error("HELLO ERROR2")
        self.assertEqual(chk_file(filename, "HELLO ERROR2"), 1)

        out = e.tplogsetreqfile(None, filename, None)
        self.assertEqual(out["buftype"], "NULL")
        e.tplogclosereqfile()


        # set thread logger

        # close thread logger

        # log some stuff...
        e.tpterm()
        
        
if __name__ == '__main__':
    unittest.main()

