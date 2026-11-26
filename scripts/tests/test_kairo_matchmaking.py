"""Test expiry, owner/seat lifetime and independent-process seat races."""
import concurrent.futures
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from kairo_matchmaking import Directory


class MatchmakingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'sessions.db'
        self.now = 100.
        self.directory = Directory(self.path, lambda: self.now)

    def test_lifecycle(self):
        d = self.directory
        s = d.create('arena', 12345, 1, 10)
        self.assertEqual(d.list()[0]['joined'], 0)
        m = d.join(s['id'], 'alice')
        self.assertEqual(m['port'], 12345)
        with self.assertRaises(ValueError): d.join(s['id'], 'bob')
        with self.assertRaises(ValueError): d.join(s['id'], 'alice')
        self.assertFalse(d.close(s['id'], 'wrong'))
        self.assertTrue(d.leave(m['token']))
        self.assertFalse(d.leave(m['token']))
        d.join(s['id'], 'bob')
        self.now = 110.
        self.assertEqual(d.list(), [])
        with self.assertRaises(ValueError): d.join(s['id'], 'charlie')
        new = d.create('new', 12345)
        self.assertTrue(d.close(new['id'], new['owner']))
        self.assertEqual(d.list(), [])

    def test_concurrent_processes(self):
        d = Directory(self.path)
        s = d.create('race', 12345, 4, 60)
        script = Path(__file__).with_name('kairo_matchmaking.py')
        # In the repository this test lives under scripts/tests/.
        if not script.exists(): script = Path(__file__).resolve().parents[1] / script.name
        def join(i):
            return subprocess.run([sys.executable, str(script), str(self.path), 'join', s['id'], f'p{i}'], capture_output=True, text=True, timeout=15)
        with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
            results = list(pool.map(join, range(24)))
        accepted = [json.loads(p.stdout) for p in results if p.returncode == 0]
        self.assertEqual(len(accepted), 4)
        self.assertEqual(d.list()[0]['joined'], 4)
        self.assertEqual(len({m['token'] for m in accepted}), 4)
        self.assertTrue(d.close(s['id'], s['owner']))
        self.assertEqual(d.list(), [])

    def test_protected_lifecycle(self):
        d = self.directory
        tickets = ('1'*64, '2'*64)
        for bad in (('1'*64,'1'*64'), ('bad','2'*64'), ('1'*64,), 'x'*64):
            with self.assertRaises(ValueError):
                d.create('protected',12345,tickets=bad)
        with self.assertRaises(ValueError):
            d.create('protected',12345,capacity=3,tickets=tickets)
        s = d.create('protected',12345,ttl=10,tickets=tickets)
        self.assertEqual(d.list()[0]['joined'],0)
        self.assertNotIn(tickets[0],json.dumps(d.list()))
        a = d.join(s['id'],'alice')
        self.assertEqual((a['seat'],a['token']),(0,tickets[0]))
        self.assertTrue(d.leave(a['token']))
        self.assertEqual(d.list()[0]['joined'],1)
        with self.assertRaises(ValueError): d.join(s['id'],'alice')
        b = d.join(s['id'],'bob')
        self.assertEqual((b['seat'],b['token']),(1,tickets[1]))
        self.assertTrue(d.leave(b['token']))
        with self.assertRaises(ValueError): d.join(s['id'],'charlie')
        self.assertEqual(d.list()[0]['joined'],2)
        self.now=110.
        self.assertEqual(d.list(),[])
        self.assertFalse(d.leave(b['token']))
        self.assertFalse(d.close(s['id'],s['owner']))

    def test_protected_concurrent_processes(self):
        d = Directory(self.path)
        tickets = ('a'*64,'b'*64)
        s = d.create('protected',12345,tickets=tickets)
        script = Path(__file__).resolve().parents[1] / 'kairo_matchmaking.py'
        def join(i):
            return subprocess.run([sys.executable,str(script),str(self.path),'join',s['id'],f'p{i}'],
                                  capture_output=True,text=True,timeout=15)
        with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
            results=list(pool.map(join,range(24)))
        accepted=[json.loads(p.stdout) for p in results if p.returncode==0]
        self.assertEqual(len(accepted),2)
        self.assertEqual({(m['seat'],m['token']) for m in accepted},set(enumerate(tickets)))
        self.assertEqual(d.list()[0]['joined'],2)
        self.assertTrue(d.close(s['id'],s['owner']))
        self.assertEqual(d.list(),[])

    def test_invalid(self):
        for args in [('bad label', 1), ('ok', 0), ('ok', 1, 0), ('ok', 1, 1, 0)]:
            with self.assertRaises(ValueError): self.directory.create(*args)
        self.directory.create('ok', 123)
        with self.assertRaises(ValueError): self.directory.create('other', 123)


if __name__ == '__main__':
    unittest.main()
