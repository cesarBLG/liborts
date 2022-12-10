#!/usr/bin/env python

import requests
import json
import time
import socket
import threading
import os

controlmap = {'SPEEDOMETER:0': 'controller::speedometer', 'AMMETER:0': 'controller::ammeter', 'LINE_VOLTAGE:0': 'controller::line_voltage'}
writecontrolmap = {'controller::brake::train': 'TRAIN_BRAKE', 'controller::brake::dynamic': 'DYNAMIC_BRAKE', 'controller::brake::engine': 'ENGINE_BRAKE', 'controller::throttle': 'THROTTLE', 'controller::direction': 'DIRECTION', 'cruise_speed': 'ORTS_SELECTED_SPEED_SELECTOR'}

c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect(('localhost', 5090))

for key in writecontrolmap:
    c.send(('register('+key+')\n').encode('utf-8'))

x = c.makefile("rb")

def TCPToWeb():
    while True:
        line = x.readline().decode('utf-8')
        spl = line.split('=')
        name = ""
        if spl[0] in writecontrolmap:
            name = writecontrolmap[spl[0]]
        elif spl[0] == "connected" or len(spl) < 2:
            continue
        else:
            name = spl[0]
        
        requests.post('http://localhost:2150/API/CABCONTROLS', json=[{
        "TypeName": name, "ControlIndex": 0, "Value": float(spl[1])}])
        
prevControls = None
TtW = threading.Thread(target=TCPToWeb)
TtW.start()
while True:
    response = requests.get('http://localhost:2150/API/CABCONTROLS')
    controls = json.loads(response.content)
    changed = dict()
    if prevControls == None or len(prevControls) != len(controls):
        prevControls = None
    ccount = dict()
    for i in range(1,len(controls)):
        ctrl = controls[i]
        name = ctrl['TypeName']
        num = 0
        if name in ccount:
            num = ccount[name]+1
        ccount[name] = num
        if prevControls == None or prevControls[i]['RangeFraction']!=controls[i]['RangeFraction']:
            changed[ctrl['TypeName']+':'+str(num)] = (ctrl['RangeFraction'],ctrl['MinValue'],ctrl['MaxValue'])
            
    for control in changed.items():
        if control[0] in controlmap:
            c.send((controlmap[control[0]]+'='+str(control[1][0]*(control[1][2]-control[1][1])+control[1][1])+'\n').encode('utf-8'))
        
    prevControls = controls
    time.sleep(0.1)

