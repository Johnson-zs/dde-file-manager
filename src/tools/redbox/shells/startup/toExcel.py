#!/bin/python3

import json;
import sys;

def indexCmp(a1):
    return int(a1['index'])

if __name__ == '__main__':
    file = open(sys.argv[1],'r')
    data = json.loads(file.read())
    record = []
    for obj in data:
        if 'topic' in obj and obj['topic'] == 'startup':
            record.append(obj)
    record.sort(key=indexCmp)
    if len(record) < 1:
        sys.exit(0)
    begin = int(record[0]['values']['time'])
    last = begin
    toWrite = []
    for obj in record:
        cur = int(obj['values']['time'])
        toWrite.append((obj['name'], cur - last, cur - begin))
        last = cur
    path = sys.argv[1] + ".csv";
    wf = open(path, 'w')
    for obj in toWrite:
        text = obj[0] + ',' + str(obj[1]) + ',' + str(obj[2]) + '\n'
        wf.write(text)

