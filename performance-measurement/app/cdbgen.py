#!/usr/bin/env python

import sys
import time
import math

def print_config(if_str):
    print("""<?xml version="1.0" encoding="UTF-8"?>
    <config xmlns="http://tail-f.com/ns/config/1.0">
    <sys xmlns="http://tail-f.com/ns/example/routing">
    <interfaces>%s
    </interfaces>
    </sys>
    </config>"""%(if_str))

def init(n):
    if_str = ""
    s_str = ""
    for i in range(0,n):
        if_str += """
      <interface>
        <name>eth%d</name>
      </interface>"""%(i)

    print_config(if_str)

def parse_num(str):
    if str[:2] == '2^':
        return pow(2,parse_num(str[2:]))
    return int(str)

init(parse_num(sys.argv[1]))
