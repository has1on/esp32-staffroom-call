import sys,unittest,tempfile,csv,wave,struct,json,threading,urllib.request,urllib.error,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'demo')]
from validate_sd import inspect_wav,validate
from prepare_wav import compose
from make_rosters import generate
from server import Preview,create_server

def wav(path,rate=16000,ch=1,samples=128):
 with wave.open(str(path),'wb') as f:f.setnchannels(ch);f.setsampwidth(2);f.setframerate(rate);f.writeframes(struct.pack('<'+'h'*(samples*ch),*([1000,-1000]*(samples*ch//2))))

def card(root):
 (root/'students').mkdir();(root/'teachers').mkdir()
 (root/'students.csv').write_text('studentId,class,maskedName,displayName,audioFile\n00001,"2,A",陳○文,陳大文,/students/00001.wav\n',encoding='utf-8-sig')
 (root/'teachers.csv').write_text('teacherId,displayName,audioFile\n01,李老師,/teachers/01.wav\n',encoding='utf-8')
 wav(root/'students/00001.wav');wav(root/'teachers/01.wav')

class Tools(unittest.TestCase):
 def setUp(self):
  base=ROOT.parent/'tmp/test-runs';base.mkdir(parents=True,exist_ok=True)
  self.temp=tempfile.TemporaryDirectory(prefix='staffroom-test-',dir=base);self.root=Path(self.temp.name)
 def tearDown(self):self.temp.cleanup()
 def test_valid_card_bom_quotes_zeros(self):card(self.root);r=validate(self.root);self.assertTrue(r['ok'],r['errors']);self.assertEqual(r['studentCount'],1)
 def test_missing_wav(self):card(self.root);(self.root/'students/00001.wav').unlink();self.assertFalse(validate(self.root)['ok'])
 def test_duplicate_id(self):card(self.root);p=self.root/'students.csv';p.write_text(p.read_text(encoding='utf-8-sig')+'00001,2A,陳○文,陳大文,/students/00001.wav\n',encoding='utf-8');self.assertTrue(any('重複' in e for e in validate(self.root)['errors']))
 def test_traversal(self):card(self.root);p=self.root/'students.csv';p.write_text(p.read_text(encoding='utf-8-sig').replace('/students/00001.wav','/../secret.wav'),encoding='utf-8');self.assertFalse(validate(self.root)['ok'])
 def test_fake_mp3(self):p=self.root/'fake.wav';p.write_bytes(b'ID3notwav');self.assertRaises(ValueError,inspect_wav,p)
 def test_stereo_supported(self):p=self.root/'test.wav';wav(p,ch=2);self.assertEqual(inspect_wav(p)['channels'],2)
 def test_bad_samplerate(self):p=self.root/'test.wav';wav(p,rate=44100);self.assertRaises(ValueError,inspect_wav,p)
 def test_metadata_chunk(self):p=self.root/'test.wav';wav(p);b=p.read_bytes();chunk=b'JUNK'+struct.pack('<I',3)+b'abc\x00';b=b[:12]+chunk+b[12:];b=b[:4]+struct.pack('<I',len(b)-8)+b[8:];p.write_bytes(b);self.assertEqual(inspect_wav(p)['sampleRate'],16000)
 def test_truncated_file(self):p=self.root/'test.wav';wav(p);p.write_bytes(p.read_bytes()[:-2]);self.assertRaises(ValueError,inspect_wav,p)
 def test_join_creates_new(self):a=self.root/'a.wav';b=self.root/'b.wav';out=self.root/'out.wav';wav(a);wav(b);original=a.read_bytes();seconds=compose([a,b],out);self.assertAlmostEqual(seconds,.096);self.assertEqual(a.read_bytes(),original);self.assertRaises(ValueError,compose,[a],a)
 def test_no_naive_downsample(self):a=self.root/'a.wav';wav(a,rate=44100);self.assertRaises(ValueError,compose,[a],self.root/'out.wav')
 def test_rosters_mask_and_preserve_zero(self):m=self.root/'m.csv';t=self.root/'t.csv';m.write_text('studentId,class,displayName\n00001,2A,陳大文\n',encoding='utf-8');t.write_text('teacherId,displayName\n01,李老師\n',encoding='utf-8');out=generate(m,t,self.root/'out');door=(out/'DOOR/students.csv').read_text(encoding='utf-8');self.assertIn('00001',door);self.assertNotIn('陳大文',door);self.assertIn('陳○文',door)
 def test_no_roster_overwrite(self):m=self.root/'m.csv';t=self.root/'t.csv';m.write_text('studentId,class,displayName\n00001,2A,陳大文\n',encoding='utf-8');t.write_text('teacherId,displayName\n01,李老師\n',encoding='utf-8');generate(m,t,self.root/'out');self.assertRaises(FileExistsError,generate,m,t,self.root/'out')
 def test_door_privacy(self):card(self.root);r=validate(self.root,'door');self.assertTrue(any('displayName' in x for x in r['errors']))

class FontAndIds(unittest.TestCase):
 def test_cjk_ink_not_clipped(self):
  from generate_fonts import render_glyph
  from PIL import ImageFont
  fontpath=ROOT/'assets/NotoSansCJKsc-Regular.otf'
  for size in (16,28):
   font=ImageFont.truetype(str(fontpath),size-2)
   for c in '陳文李老師同學找○jg':
    actual,advance=render_glyph(fontpath,size,c)
    reference=__import__('PIL.Image',fromlist=['Image']).new('L',(80,80),0)
    __import__('PIL.ImageDraw',fromlist=['ImageDraw']).Draw(reference).text((20,5),c,font=font,fill=255)
    self.assertEqual(sum(x>=96 for x in actual.getdata()),sum(x>=96 for x in reference.getdata()),(size,c))
    self.assertGreater(advance,0);self.assertLessEqual(advance,size)
 def test_lowercase_student_rejected(self):
  with tempfile.TemporaryDirectory(dir=ROOT.parent/'tmp/test-runs') as d:
   root=Path(d);m=root/'m.csv';t=root/'t.csv'
   m.write_text('studentId,class,displayName\nstud01,2A,陳大文\n',encoding='utf-8')
   t.write_text('teacherId,displayName\n01,李老師\n',encoding='utf-8')
   self.assertRaises(ValueError,generate,m,t,root/'out')

class Queue(unittest.TestCase):
 def setUp(self):self.now=100.;self.app=Preview(capacity=2,clock=lambda:self.now)
 def d(self,rid='test-1',student='00001',teacher='01'):return {'requestId':rid,'studentId':student,'teacherId':teacher}
 def test_idempotent(self):self.assertEqual(self.app.submit(self.d())[0],202);self.assertEqual(self.app.submit(self.d())[0],200);self.assertEqual(len(self.app.queue),1)
 def test_request_collision(self):self.app.submit(self.d());self.assertEqual(self.app.submit(self.d(teacher='02'))[0],409)
 def test_queue_order(self):self.app.submit(self.d());self.app.submit(self.d('test-2','00002'));self.assertEqual([r['requestId'] for r in self.app.queue],['test-1','test-2'])
 def test_queue_full(self):self.app.submit(self.d());self.app.submit(self.d('test-2','00002'));self.assertEqual(self.app.submit(self.d('test-3','99999'))[0],429)
 def test_cooldown(self):self.app.submit(self.d());self.assertEqual(self.app.submit(self.d('test-2'))[0],429)
 def test_unknown_student(self):self.assertEqual(self.app.submit(self.d(student='BAD'))[0],404)
 def test_bad_requestid(self):self.assertEqual(self.app.submit(self.d('../bad'))[0],400)
 def test_play_completion(self):self.app.play_seconds=.02;t=threading.Thread(target=self.app.worker,daemon=True);t.start();self.app.submit(self.d());time.sleep(.12);self.app.closed.set();t.join(1);self.assertEqual(self.app.history['test-1']['state'],'done');self.assertFalse(self.app.playing)

class HTTP(unittest.TestCase):
 def setUp(self):self.server,self.app=create_server(0);self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.url='http://127.0.0.1:'+str(self.server.server_port)
 def tearDown(self):self.app.closed.set();self.server.shutdown();self.server.server_close();self.thread.join(1)
 def get(self,path):return urllib.request.urlopen(self.url+path)
 def post(self,path,body,extra=None):return urllib.request.urlopen(urllib.request.Request(self.url+path,data=json.dumps(body).encode(),headers={'Content-Type':'application/json',**(extra or {})}))
 def test_health(self):self.assertTrue(json.load(self.get('/api/health'))['ok'])
 def test_lookup_is_masked(self):data=json.load(self.post('/api/lookup',{'studentId':'00001'}));self.assertNotIn('displayName',data);self.assertEqual(data['maskedName'],'陳○文')
 def test_no_private_file_route(self):
  with self.assertRaises(urllib.error.HTTPError) as e:self.get('/../sd_templates/AUDIO/students.csv')
  self.assertEqual(e.exception.code,404)
 def test_wrong_origin(self):
  with self.assertRaises(urllib.error.HTTPError) as e:self.post('/api/lookup',{'studentId':'00001'},{'Origin':'https://example.invalid'})
  self.assertEqual(e.exception.code,403)
 def test_invalid_json(self):
  with self.assertRaises(urllib.error.HTTPError) as e:self.post('/api/lookup',[])
  self.assertEqual(e.exception.code,400)
 def test_post_idempotent(self):
  d={'requestId':'http-test','studentId':'00001','teacherId':'01'}
  self.assertEqual(self.post('/api/announce',d).status,202);self.assertTrue(json.load(self.post('/api/announce',d))['duplicate']);self.assertEqual(len(self.app.queue),1)
if __name__=='__main__':unittest.main(verbosity=2)
