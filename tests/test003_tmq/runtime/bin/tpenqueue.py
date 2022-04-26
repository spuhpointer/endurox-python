import unittest
import endurox as e
import exutils as u
import gc

class TestTpenqueue(unittest.TestCase):

    # enqueue something and dequeue
    def test_tpenqueue(self):
        e.tpopen()
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < u.test_duratation():
            e.tpbegin(60, 0)
            qctl = e.tpenqueue("SAMPLESPACE", "TESTQ", e.TPQCTL(), {"data":"SOME DATA"})
            e.tpcommit(0)
            e.tpbegin(60, 0)
            qctl, retbuf = e.tpdequeue("SAMPLESPACE", "TESTQ", e.TPQCTL())
            e.tpcommit(0);
            self.assertEqual(retbuf["data"], "SOME DATA")

        e.tpclose()
    
    def test_tpenqueue_susp(self):
        e.tpopen()
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < u.test_duratation():

            e.tpbegin(60, 0)
            qctl = e.tpenqueue("SAMPLESPACE", "TESTQ", e.TPQCTL(), {"data":"SOME DATA"})
            t1 = e.tpsuspend();

            e.tpbegin(60, 0)
            qctl = e.tpenqueue("SAMPLESPACE", "TESTQ", e.TPQCTL(), {"data":"SOME DATA2"})
            e.tpcommit()

            e.tpbegin(60, 0)
            qctl, retbuf = e.tpdequeue("SAMPLESPACE", "TESTQ", e.TPQCTL())
            e.tpcommit(0);
            self.assertEqual(retbuf["data"], "SOME DATA2")

            e.tpresume(t1)
            e.tpcommit();

            qctl, retbuf = e.tpdequeue("SAMPLESPACE", "TESTQ", e.TPQCTL())
            self.assertEqual(retbuf["data"], "SOME DATA")

        e.tpclose()

    def test_tpenqueue_nomsg(self):
        e.tpopen()
        log = u.NdrxLogConfig()
        log.set_lev(e.log_always)
        w = u.NdrxStopwatch()
        while w.get_delta_sec() < u.test_duratation():
            try:
                qctl, retbuf = e.tpdequeue("SAMPLESPACE", "TESTQ", e.TPQCTL())
            except e.QmException as ex:
                self.assertEqual(ex.code,e.QMENOMSG)
            else:
                self.assertEqual(True,False)
        log.restore()

    # enq/deq by corrid + VIEW ?
    #def test_tpenqueue_corrid(self):
    
if __name__ == '__main__':
    unittest.main()

