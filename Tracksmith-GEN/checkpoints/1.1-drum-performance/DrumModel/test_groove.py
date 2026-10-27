import tempfile
import unittest
from pathlib import Path
from drum_baseline import generate
from guitar_accents import choose_kicks
from test_maps import events


class GrooveTests(unittest.TestCase):
    def make(self,root,name,**options):
        target=root/name
        generate(root/'no-model.json',target,'all',140,8,42,downbeat=240/140,drum_map='ggd-okw-metal',pattern='groove',**options)
        return [event for event in events(target) if event[1]==153]

    def test_swing_only_moves_cymbal_offbeats(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);straight=self.make(root,'straight',swing=50);swung=self.make(root,'swing',swing=58)
            self.assertEqual([e for e in straight if e[2] in (24,26)],[e for e in swung if e[2] in (24,26)])
            expected=[]
            for event in straight:
                if event[2] in (43,46) and (event[0]-1920)%480==240:expected.append((event[0]+38,*event[1:]))
                else:expected.append(event)
            self.assertEqual(sorted(expected),sorted(swung))
            triplet=self.make(root,'triplet',swing=66.6666667)
            self.assertIn((1920+320,153,43,65),triplet)
            self.assertEqual(min(e[0] for e in swung),1920)

    def test_optional_instruments_and_phrase_fills(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);plain=self.make(root,'basic',density=0,fill_every=0)
            self.assertTrue({e[2] for e in plain}<={24,26,43,46})
            full=self.make(root,'full',density=2,fill_every=4,crashes=True,timekeeper='ride',swing=58)
            self.assertTrue({24,26,33,34,35,52,62}<={e[2] for e in full})
            self.assertEqual([e[0] for e in full if e[2]==52],[1920,1920+4*1920])
            for event in full:
                if event[2] in (33,34,35):self.assertIn(event[0]-1920,[bar*1920+tick for bar in (3,7) for tick in (1560,1680,1800)])

    def test_accent_guidance_is_sparse_and_deduplicated(self):
        downbeat=240/140
        def attack(tick,strength):return {'seconds':downbeat+tick/480*60/140,'strength':strength}
        attacks=[attack(720,.6),attack(1200,1),attack(1440,2),attack(1920+240,1.5),attack(1920+720,.3),attack(8*1920,2)]
        selected=choose_kicks(attacks,140,downbeat,downbeat+8*240/140,8)
        self.assertEqual(selected,[1200,2160])
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);hits=self.make(root,'guided',density=0,accent_kicks=selected,swing=62)
            kicks=[e[0] for e in hits if e[2]==24]
            self.assertEqual(len(kicks),16+2)
            self.assertTrue(all((tick-1920)%240==0 for tick in kicks))
            duplicated=self.make(root,'duplicate',density=1,accent_kicks=[720])
            locations=[(e[0],e[2]) for e in duplicated]
            self.assertEqual(len(locations),len(set(locations)))

    def test_partial_phrase_has_no_truncated_fill(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);end=(1+3.9)*240/140
            hits=self.make(root,'partial',fill_every=4,crashes=True,duration=end)
            self.assertFalse(any(e[2] in (33,34,35) for e in hits))
            self.assertTrue(all(e[0]<round(end*140/60*480) for e in hits))

    def test_invalid_swing_never_writes_output(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for value in (49,76,float('nan')):
                with self.assertRaises(ValueError):self.make(root,'invalid',swing=value)
                self.assertFalse((root/'invalid').exists())


if __name__=='__main__':unittest.main()
