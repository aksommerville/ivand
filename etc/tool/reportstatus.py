#!/usr/bin/env python
import sys,os

barw=70
progress=0
status=0

def initbar():
  os.write(1,"["+"*"*progress+" "*(barw-progress)+"]\n")
  
def erasebar():
  os.write(1,"\x1b[1A  "+" "*barw+"\r")
  
def setprogress(p):
  try: p=int(p.split('.')[0])
  except: return
  if p<0: p=0
  elif p>100: p=100
  dotc=(p*barw)/100
  progress=dotc
  os.write(1,"\x1b[1A\x1b[1C"+"*"*dotc+" "*(barw-dotc)+"\x1b[1B\r")
  
initbar()

while True:
  line=sys.stdin.readline()
  if not line: break
  words=line.split("|||")
  if len(words)==3 and words[0].startswith("==="):
    prefix,template,content=words
    prefix=prefix[3:].strip()
    template=template.strip().replace("%%","%")
    content=content.strip()[1:-1].split()
    if prefix=="info" and template.startswith("Progress") and len(content)==1:
      setprogress(content[0])
    else:
      for i in xrange(len(content)):
        template=template.replace("{%d}"%(i,),content[i])
      erasebar()
      os.write(1,"%s\n"%(template,))
      initbar()
  else: # Doesn't look right? Whatever, dump it verbatim.
    erasebar()
    os.write(1,line)
    if line.startswith("exit status "):
      try: status=int(line[12:].strip())
      except: pass
    initbar()
    
erasebar()
sys.exit(status)
