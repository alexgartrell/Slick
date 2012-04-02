#!/usr/bin/env python

import sender
import receiver
import time

if __name__ == '__main__':
    r = receiver.Receiver([10000])
    r.start()
    s = sender.Sender(('127.0.0.1', 10000), [1], 10000, 10000, 20)
    s.start()
    s.join()
    time.sleep(2)
    r.stop()
    r.join()
    trials = r.get_trials()
    print trials
    print receiver.process_trial(trials[0]), 'bps'
