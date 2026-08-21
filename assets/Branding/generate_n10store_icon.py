from PIL import Image, ImageDraw, ImageFilter
from pathlib import Path

out=Path(__file__).resolve().parent
S=1024
im=Image.new('RGBA',(S,S),(0,0,0,0))
# soft shadow
shadow=Image.new('RGBA',(S,S),(0,0,0,0)); sd=ImageDraw.Draw(shadow)
sd.rounded_rectangle((115,155,909,930),radius=150,fill=(0,0,0,125))
shadow=shadow.filter(ImageFilter.GaussianBlur(30)); im.alpha_composite(shadow)
# deep Nebula bag body with vertical gradient
mask=Image.new('L',(S,S),0); md=ImageDraw.Draw(mask)
md.rounded_rectangle((105,135,899,910),radius=140,fill=255)
grad=Image.new('RGBA',(S,S))
p=grad.load()
for y in range(S):
    t=max(0,min(1,(y-135)/775))
    c=(int(20-10*t),int(192-87*t),int(224-50*t),255)
    for x in range(S): p[x,y]=c
im.alpha_composite(Image.composite(grad,Image.new('RGBA',(S,S)),mask))
d=ImageDraw.Draw(im)
# top rim and handle
d.rounded_rectangle((106,135,898,910),radius=140,outline=(181,249,255,255),width=18)
d.arc((290,-30,714,390),180,360,fill=(220,252,255,255),width=42)
d.arc((332,15,672,345),180,360,fill=(10,65,105,255),width=24)
# orbit motif behind apps
d.ellipse((215,345,809,825),outline=(124,241,255,150),width=18)
d.arc((215,345,809,825),205,25,fill=(255,255,255,220),width=22)
d.ellipse((763,475,807,519),fill=(255,255,255,255))
# app tile panel
d.rounded_rectangle((282,366,742,770),radius=62,fill=(8,42,75,230),outline=(220,253,255,255),width=16)
colors=[(154,101,214,255),(20,184,167,255),(255,143,10,255),(230,68,20,255),(76,190,16,255),(13,127,205,255)]
positions=[(320,405),(470,405),(620,405),(320,565),(470,565),(620,565)]
for (x,y),c in zip(positions,colors):
    d.rounded_rectangle((x,y,x+112,y+112),radius=22,fill=c)
# tiny star accent
d.polygon([(175,305),(188,338),(223,350),(188,363),(175,397),(162,363),(127,350),(162,338)],fill=(255,255,255,235))

png=out/'N10Store.png'; ico=out/'N10Store.ico'
im.resize((256,256),Image.Resampling.LANCZOS).save(png)
im.save(ico,format='ICO',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
print(png); print(ico)
