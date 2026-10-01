import unittest
from tools.qa.measure_reference import summarize, decode_packet, Remote

class MetricsTests(unittest.TestCase):
    def test_game_logic_and_dispatch_are_distinct_from_vi_and_graphics(self):
        a={'vi':0,'common_updates':0,'graphics_done':0,'os_ticks':0,
           'logic_ticks':100,'state_updates':100,'mode_id':5}
        b={'vi':600,'common_updates':600,'graphics_done':300,'os_ticks':468750000,
           'logic_ticks':700,'state_updates':400,'mode_id':5}
        result=summarize(a,b,10)
        self.assertEqual(result['rates_per_wall_second']['logic_ticks'],60)
        self.assertEqual(result['rates_per_wall_second']['state_updates'],30)
        b['logic_ticks']=100
        self.assertEqual(summarize(a,b,10)['rates_per_wall_second']['logic_ticks'],0)

    def test_game_counter_reset_or_mode_change_is_not_a_wrap(self):
        a={'vi':0,'common_updates':0,'graphics_done':0,'os_ticks':0,
           'logic_ticks':100,'state_updates':100,'mode_id':5}
        for ticks, mode in [(1,5),(101,4)]:
            b=dict(a,logic_ticks=ticks,mode_id=mode)
            result=summarize(a,b,1)
            self.assertFalse(result['valid_game_window'])
            self.assertIsNone(result['rates_per_wall_second']['logic_ticks'])

    def test_vi_logic_and_graphics_remain_separate(self):
        a={'vi':10,'common_updates':20,'graphics_done':30,'os_ticks':1000}
        b={'vi':610,'common_updates':620,'graphics_done':330,'os_ticks':468751000}
        result=summarize(a,b,10.0)
        self.assertEqual(result['rates_per_wall_second'],{'vi':60.0,'common_updates':60.0,'graphics_done':30.0})
        self.assertEqual(result['emulated_seconds'],10.0)
        self.assertEqual(result['emulated_to_wall_ratio'],1.0)
    def test_counter_wrap(self):
        a={'vi':0xfffffffe,'common_updates':0xfffffffe,'graphics_done':0xfffffffe,'os_ticks':100}
        b={'vi':2,'common_updates':2,'graphics_done':0,'os_ticks':93750100}
        self.assertEqual(summarize(a,b,2.0)['deltas'],{'vi':4,'common_updates':4,'graphics_done':2})
    def test_pause_does_not_erase_vi(self):
        a={'vi':0,'common_updates':10,'graphics_done':0,'os_ticks':0}
        b={'vi':600,'common_updates':10,'graphics_done':300,'os_ticks':468750000}
        self.assertEqual(summarize(a,b,10)['rates_per_wall_second']['common_updates'],0)
        self.assertEqual(summarize(a,b,10)['rates_per_wall_second']['vi'],60)
    def test_zero_duration_rejected(self):
        with self.assertRaises(ValueError):summarize({}, {}, 0)
    def test_packet_checksums(self):
        self.assertEqual(decode_packet(b'$OK#9a'),'OK')
        with self.assertRaises(ValueError):decode_packet(b'$OK#00')
    def test_malformed_packet_rejected(self):
        for data in [b'OK',b'$OK#z!',b'$OK#9']:
            with self.subTest(data=data):
                with self.assertRaises(ValueError):decode_packet(data)

    def test_os_clock_wrap(self):
        a={'vi':0,'common_updates':0,'graphics_done':0,'os_ticks':2**64-46875000}
        b={'vi':0,'common_updates':0,'graphics_done':0,'os_ticks':0}
        self.assertEqual(summarize(a,b,1)['emulated_seconds'],1)

    def test_wrong_live_code_rejected(self):
        remote=Remote.__new__(Remote)
        remote.read=lambda address,size: bytes(size)
        with self.assertRaisesRegex(ValueError,'Live code'):
            remote.verify_code()

    def test_fragmented_packet_and_ack(self):
        class Socket:
            def __init__(self):
                self.parts=iter([b'+$O',b'K#',b'9',b'a'])
                self.sent=[]
            def recv(self,size):return next(self.parts)
            def sendall(self,data):self.sent.append(data)
        remote=Remote.__new__(Remote)
        remote.socket=Socket()
        remote.buffer=b''
        self.assertEqual(remote.receive(),'OK')
        self.assertEqual(remote.socket.sent,[b'+'])

    def test_snapshot_big_endian_fields(self):
        remote=Remote.__new__(Remote)
        common=bytearray(0x7d8)
        common[:4]=(0x12345678).to_bytes(4,'big')
        common[0x7d4:]=(0x87654321).to_bytes(4,'big')
        vi=(0x123456789abcdef0).to_bytes(8,'big')+bytes(4)+(0xabcdef01).to_bytes(4,'big')
        game=bytearray(0x32)
        game[:4]=(600).to_bytes(4,'big')
        game[8:12]=(300).to_bytes(4,'big')
        game[48:50]=(5).to_bytes(2,'big')
        blocks={0x800df714:bytes(common),0x80163b50:vi,0x800e4478:bytes(game)}
        remote.read=lambda address,size: blocks[address]
        sample,midpoint,cost=remote.snapshot()
        self.assertEqual(sample,{'graphics_done':0x12345678,'common_updates':0x87654321,'vi':0xabcdef01,'os_ticks':0x123456789abcdef0,
                                'logic_ticks':600,'state_updates':300,'mode_id':5})
        self.assertGreater(midpoint,0)
        self.assertGreaterEqual(cost,0)
