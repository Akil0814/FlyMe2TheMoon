"""Generate QA mouse routes from imported geometry; never modifies game state.

Uses the bundled Python Pillow for an occupancy raster. Moving mechanisms still
need runtime observations and route tuning; an A* route is not proof of a clear.
"""
import argparse, heapq, json, math
from pathlib import Path
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[2]
STEP=8

def plan(level):
    data=json.loads((ROOT/f'assets/configs/fly/levels/{level:03}.json').read_text(encoding='utf8'))
    width,height=math.ceil(data['width']/STEP),math.ceil(data['height']/STEP)
    mask=Image.new('L',(width,height));draw=ImageDraw.Draw(mask)
    for line in data['edges']:
        draw.line([(x/STEP,y/STEP) for x,y in line],fill=255,width=7)
    for e in data['entities']:
        if e['item'] or e['kind'] in [8,24,25,101,115] or 105<=e['kind']<=110 or e['kind']==117:continue
        # Static hazard only. Laser rays / projectiles are evaluated by the game.
        x,y=e['x']/STEP,e['y']/STEP;rx=e['w']/STEP/2+4;ry=e['h']/STEP/2+4
        draw.ellipse((x-rx,y-ry,x+rx,y+ry),fill=255)
    cells=mask.load()
    start=(round((data['born'][0]+25)/STEP),round((data['born'][1]-29)/STEP))
    moon=next(e for e in data['entities'] if e['kind']==101)
    goal=(round(moon['x']/STEP),round((moon['y']-13)/STEP))
    def nearest(p):
        for radius in range(15):
            options=[(dx*dx+dy*dy,(p[0]+dx,p[1]+dy)) for dx in range(-radius,radius+1) for dy in range(-radius,radius+1) if 0<=p[0]+dx<width and 0<=p[1]+dy<height and cells[p[0]+dx,p[1]+dy]==0]
            if options:return min(options)[1]
        raise ValueError('No free start/goal cell')
    start,goal=nearest(start),nearest(goal)
    portals=[e for e in data['entities'] if e['kind']==115]
    portal_links={}
    for portal in portals:
        if 'TargetPortalID' not in portal['info']:continue
        identifier=int(portal['info']['TargetPortalID'])
        target=next((e for e in portals if int(e['info'].get('PortalID',-1))==identifier),None)
        if target is None:target=next(e for e in portals if e['id']==identifier)
        origin=nearest((round(portal['x']/STEP),round((portal['y']-13)/STEP)))
        destination=nearest((round(target['x']/STEP),round((target['y']-13)/STEP)))
        portal_links[origin]=destination
    cost={start:0};parent={};queue=[(math.dist(start,goal),start)];closed=set()
    while queue:
        _,p=heapq.heappop(queue)
        if p in closed:continue
        if p==goal:break
        closed.add(p)
        if p in portal_links:
            q=portal_links[p];value=cost[p]+1
            if value<cost.get(q,float('inf')):cost[q]=value;parent[q]=p;heapq.heappush(queue,(value,q))
        for dx,dy in [(1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,1),(1,-1),(-1,-1)]:
            q=p[0]+dx,p[1]+dy
            if not (0<=q[0]<width and 0<=q[1]<height) or cells[q] or cells[p[0]+dx,p[1]] or cells[p[0],p[1]+dy]:continue
            value=cost[p]+math.hypot(dx,dy)
            if value<cost.get(q,float('inf')):cost[q]=value;parent[q]=p;heapq.heappush(queue,(value+(0 if portals else math.dist(q,goal)),q))
    if goal not in cost:raise ValueError('No conservative static route; inspect terrain / portal mechanics')
    path=[goal]
    while path[-1]!=start:path.append(parent[path[-1]])
    path.reverse()
    def visible(a,b):
        n=max(abs(a[0]-b[0]),abs(a[1]-b[1]))*2
        return all(not cells[round(a[0]+(b[0]-a[0])*i/max(1,n)),round(a[1]+(b[1]-a[1])*i/max(1,n))] for i in range(n+1))
    route=[];i=0
    while i<len(path)-1:
        j=i+1
        while j+1<len(path) and math.dist(path[i],path[j+1])*STEP<=100 and visible(path[i],path[j+1]):j+=1
        route.append([path[j][0]*STEP,path[j][1]*STEP]);i=j
    image=mask.convert('RGB');pen=ImageDraw.Draw(image);pen.line([(x/STEP,y/STEP) for x,y in route],fill='cyan',width=2)
    out=ROOT/'out/analysis/routes';out.mkdir(parents=True,exist_ok=True);image.resize((width*2,height*2)).save(out/f'{level:03}.png')
    return route

if __name__=='__main__':
    args=argparse.ArgumentParser();args.add_argument('levels',nargs='+',type=int);options=args.parse_args()
    for level in options.levels:
        try:
            route=plan(level);name=f'level{level}-route01';scenario={'name':name,'level':level,'duration':max(75,len(route)*1.5+30),'stop_after_success':2,'events':[],'pilot':route,'expect':{'level':level,'success':True}}
            (ROOT/f'game/testing/{name}.json').write_text(json.dumps(scenario,indent=2),encoding='utf8');print(level,len(route),'waypoints')
        except ValueError as error:print(level,str(error))
