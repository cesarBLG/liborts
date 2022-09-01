#!/usr/bin/env python

import requests
import json
import time
import socket

controlmap = {'SPEEDOMETER:0': 'controller::speedometer', 'AMMETER:0': 'controller::ammeter', 'LINE_VOLTAGE:0': 'controller::line_voltage'}

c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect(('localhost', 5090))

prevControls = None
while True:
    response = requests.get('http://localhost:2150/API/CABCONTROLS')
    controls = json.loads(response.content)
    changed = dict()
    if prevControls == None or len(prevControls) != len(controls):
        prevControls = None
    ccount = dict()
    for i in range(1,len(controls)):
        c = controls[i]
        name = c['TypeName']
        num = 0
        if name in ccount:
            num = ccount[name]+1
        ccount[name] = num
        if prevControls == None or prevControls[i]['RangeFraction']!=controls[i]['RangeFraction']:
            changed[c['TypeName']+':'+str(num)] = (c['RangeFraction'],c['MinValue'],c['MaxValue'])
            
    for control in changed.items():
        if control[0] in controlmap:
            c.writeline(controlmap[control[0]]+'='+str(control[1][0]*(control[1][2]-control[1][1])+control[1][1]))
        
    prevControls = controls
    time.sleep(0.1)
