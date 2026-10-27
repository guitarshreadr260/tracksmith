import json
import struct
import tempfile
import unittest
from pathlib import Path
from drum_baseline import generate, vlq


def events(path):
    data=Path(path).read_bytes()
    track=data[22:]
    pos=tick=0
    result=[]
    while pos<len(track):
        delta,pos=vlq(track,pos);tick+=delta
        status=track[pos];pos+=1
        if status==255:
            kind=track[pos];pos+=1
            length,pos=vlq(track,pos);payload=track[pos:pos+length];pos+=length
            if kind!=3:result.append((tick,status,kind,payload))
        else:
            result.append((tick,status,*track[pos:pos+2]));pos+=2
    return result


class MappingTests(unittest.TestCase):
    def test_full_voice_export_preserves_event_positions_velocities_and_tempo(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            # All voices on all steps to exercise every emitted on/off pair.
            stats={'bars':1,'hits':[[1]*9 for _ in range(16)],
                   'velocity':[[100]*9 for _ in range(16)],'offset':[[.03]*9 for _ in range(16)]}
            model=root/'model.json';model.write_text(json.dumps({'groups':{'all':stats}}))
            generate(model,root/'gm.mid','all',140,3,7,downbeat=1,duration=5,drum_map='gm',pattern='learned')
            generate(model,root/'ggd.mid','all',140,3,7,downbeat=1,duration=5,drum_map='ggd-okw-metal',pattern='learned')
            expected={36:24,38:26,42:43,46:46,45:35,47:34,50:33,49:52,51:62}
            gm,ggd=events(root/'gm.mid'),events(root/'ggd.mid')
            self.assertEqual(len(gm),len(ggd))
            for source,target in zip(gm,ggd):
                if source[1]&240 in (128,144):
                    self.assertEqual(target,(source[0],source[1],expected[source[2]],source[3]))
                else:self.assertEqual(source,target)
            self.assertEqual({e[2] for e in ggd if e[1]==153},set(expected.values()))

    def test_unknown_map_leaves_output_unwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'bad.mid'
            with self.assertRaises(ValueError):generate(Path(directory)/'missing.json',output,'all',140,4,7,drum_map='unknown')
            self.assertFalse(output.exists())


if __name__=='__main__':unittest.main()
