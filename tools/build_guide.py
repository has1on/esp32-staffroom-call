"""Create a paginated Chinese install guide. Windows fonts by default; source remains editable."""
import argparse,html,re,json
from pathlib import Path
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.platypus import SimpleDocTemplate,Paragraph,Spacer,Table,TableStyle,PageBreak,Preformatted,KeepTogether,Image
from reportlab.lib.pagesizes import A4
ROOT=Path(__file__).resolve().parents[1]
INK=colors.HexColor('#143043');BLUE=colors.HexColor('#167A94');MUTED=colors.HexColor('#527080');PALE=colors.HexColor('#EDF4F6');RED=colors.HexColor('#AE3535')


def main(output):
 pdfmetrics.registerFont(TTFont('BodyCJK','C:/Windows/Fonts/simsun.ttc',subfontIndex=0))
 pdfmetrics.registerFont(TTFont('TitleCJK','C:/Windows/Fonts/simhei.ttf'))
 pdfmetrics.registerFontFamily('BodyCJK',normal='BodyCJK',bold='TitleCJK',italic='BodyCJK',boldItalic='TitleCJK')
 styles={
  'body':ParagraphStyle('body',fontName='BodyCJK',fontSize=10.1,leading=16.2,textColor=INK,wordWrap='CJK',spaceAfter=8),
  'title':ParagraphStyle('title',fontName='TitleCJK',fontSize=23,leading=31,textColor=INK,wordWrap='CJK',spaceAfter=20),
  'cover':ParagraphStyle('cover',fontName='TitleCJK',fontSize=31,leading=41,textColor=INK,wordWrap='CJK',spaceAfter=20),
  'cell':ParagraphStyle('cell',fontName='BodyCJK',fontSize=9,leading=13.4,textColor=INK,wordWrap='CJK'),
  'head':ParagraphStyle('head',fontName='TitleCJK',fontSize=9,leading=13.4,textColor=colors.white,wordWrap='CJK'),
  'note':ParagraphStyle('note',fontName='BodyCJK',fontSize=10,leading=16,textColor=INK,wordWrap='CJK'),
  'stop':ParagraphStyle('stop',fontName='BodyCJK',fontSize=10,leading=16,textColor=RED,wordWrap='CJK'),
  'code':ParagraphStyle('code',fontName='BodyCJK',fontSize=8.8,leading=12.5,textColor=INK,spaceAfter=0),
 }
 def inline(text):
  text=text.replace('–','-').replace('—','-').replace('\u2011','-')
  text=html.escape(text)
  text=re.sub(r'\[([^\]]+)\]\((https?://[^\s)]+)\)',r'<link href="\2" color="#167A94"><u>\1</u></link>',text)
  text=re.sub(r'\*\*(.+?)\*\*',r'<b>\1</b>',text)
  return re.sub(r'`([^`]+)`',r'<font color="#11627B">\1</font>',text)
 def para(text,kind='body'):return Paragraph(inline(text),styles[kind])
 source=(ROOT/'INSTALL_GUIDE.md').read_text(encoding='utf-8-sig')
 sections=source.split('<!-- PAGE -->');story=[];usable=A4[0]-84
 checks=[]
 for section_no,section in enumerate(sections):
  lines=section.strip().splitlines();i=0;page_flow=[]
  while i<len(lines):
   line=lines[i].strip()
   if not line:i+=1;continue
   if line.startswith('# '):page_flow.append(para(line[2:],'cover'));i+=1;continue
   if line.startswith('## '):page_flow.append(para(line[3:],'title'));i+=1;continue
   if line.startswith('```'):
    code=[];i+=1
    while i<len(lines) and not lines[i].strip().startswith('```'):code.append(lines[i]);i+=1
    i+=1;longest=max((pdfmetrics.stringWidth(s,'BodyCJK',8.8) for s in code),default=1)
    fontsize=min(8.8,8.8*(usable-24)/max(longest,1))
    style=ParagraphStyle('code',parent=styles['code'],fontSize=fontsize,leading=max(10.4,fontsize*1.4))
    pf=Preformatted('\n'.join(code),style)
    box=Table([[pf]],colWidths=[usable]);box.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),PALE),('BOX',(0,0),(-1,-1),0.5,colors.HexColor('#DAE7EA')),('LEFTPADDING',(0,0),(-1,-1),12),('RIGHTPADDING',(0,0),(-1,-1),12),('TOPPADDING',(0,0),(-1,-1),10),('BOTTOMPADDING',(0,0),(-1,-1),10)]))
    page_flow.extend([box,Spacer(1,10)]);continue
   if line.startswith('|'):
    rows=[]
    while i<len(lines) and lines[i].strip().startswith('|'):
     cells=[x.strip() for x in lines[i].strip().strip('|').split('|')]
     if not all(re.fullmatch(r'[:\-\s]+',x) for x in cells):rows.append(cells)
     i+=1
    cols=len(rows[0]);weights={2:[.28,.72],3:[.23,.43,.34],4:[.28,.24,.24,.24]}.get(cols,[1/cols]*cols)
    if section_no in (25,28) and cols==3:weights=[.25,.36,.39]
    if section_no==29 and cols==2:weights=[.57,.43]
    data=[[para(c,'head' if n==0 else 'cell') for c in row] for n,row in enumerate(rows)]
    tb=Table(data,colWidths=[usable*w for w in weights],hAlign='LEFT')
    tb.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),INK),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,PALE]),('LINEBELOW',(0,0),(-1,0),.5,INK),('LINEBELOW',(0,1),(-1,-1),.25,colors.HexColor('#DCE7EC')),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),9),('RIGHTPADDING',(0,0),(-1,-1),9),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7)]))
    page_flow.extend([tb,Spacer(1,11)]);continue
   if line.startswith('> '):
    note=line[2:];stop=note.startswith('[STOP]');note=note.replace('[STOP]','停步检查：').replace('[NOTE]','提示：')
    box=Table([[para(note,'stop' if stop else 'note')]],colWidths=[usable]);box.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),colors.HexColor('#FCF0EE') if stop else PALE),('LINEBEFORE',(0,0),(-1,-1),3,RED if stop else BLUE),('LEFTPADDING',(0,0),(-1,-1),12),('RIGHTPADDING',(0,0),(-1,-1),12),('TOPPADDING',(0,0),(-1,-1),9),('BOTTOMPADDING',(0,0),(-1,-1),9)]))
    page_flow.extend([box,Spacer(1,10)]);i+=1;continue
   match=re.match(r'!\[([^\]]+)\]\(([^)]+)\)',line)
   if match:
    from PIL import Image as PILImage
    path=ROOT/match[2];im=PILImage.open(path);width=min(usable,440);height=width*im.height/im.width
    if height>240:width*=240/height; height=240
    page_flow.extend([Image(str(path),width,height),Spacer(1,5),para(match[1]+'（同一绘图源码电脑生成，非实机照片）','cell'),Spacer(1,10)]);i+=1;continue
   if line.startswith('- ') or re.match(r'\d+\. ',line):
    page_flow.append(para(line));i+=1;continue
   parts=[line];i+=1
   while i<len(lines) and lines[i].strip() and not re.match(r'(^\||^```|^>|^!\[|^##|^- |^\d+\. )',lines[i].strip()):parts.append(lines[i].strip());i+=1
   page_flow.append(para(' '.join(parts)))
  # Record total estimated height to detect unintended page spill.
  total=0
  for f in page_flow:
   w,h=f.wrap(usable,2000);total+=h+getattr(f,'getSpaceBefore',lambda:0)()+getattr(f,'getSpaceAfter',lambda:0)()
  checks.append({'section':section_no,'height':round(total,1),'available':A4[1]-134})
  if section_no:story.append(PageBreak())
  story.extend(page_flow)
 if any(c['height']>c['available'] for c in checks):
  raise ValueError('Page overflow: '+str([c for c in checks if c['height']>c['available']]))
 output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
 def footer(canvas,doc):
  canvas.saveState();w,h=A4
  canvas.setFillColor(INK);canvas.rect(0,h-9,w,9,fill=1,stroke=0)
  canvas.setFont('BodyCJK',8);canvas.setFillColor(MUTED);canvas.drawString(42,h-33,'教员室召唤系统 / 现场交货操作手册')
  canvas.drawRightString(w-42,h-33,'逐项执行 / 现场验收待填写')
  canvas.setStrokeColor(colors.HexColor('#D8E4E9'));canvas.line(42,39,w-42,39)
  canvas.drawString(42,24,'v0.3 | 2026-10-10 | 先备份，再分模块测试，最后交货')
  canvas.drawRightString(w-42,24,str(doc.page));canvas.restoreState()
 doc=SimpleDocTemplate(str(output),pagesize=A4,leftMargin=42,rightMargin=42,topMargin=66,bottomMargin=56,title='教员室召唤系统 - 现场安装调试与交货操作手册',author='Staffroom implementation kit',pageCompression=1)
 doc.build(story,onFirstPage=footer,onLaterPages=footer)
 return checks
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',default=str(ROOT.parent/'output/pdf/staffroom_call_system_install_guide.pdf'));a=p.parse_args()
 checks=main(a.output);print(json.dumps(checks,ensure_ascii=False,indent=2));print('Created:',a.output)
