#!/usr/bin/env python3
import os
import re
import cgi

form = cgi.FieldStorage()

script_dir = os.path.dirname(os.path.realpath(__file__))
dir_path = os.path.join(script_dir, 'data')

if not os.path.isdir(dir_path):
	os.makedirs(dir_path, exist_ok=True)

name = form.getfirst('name', '')
age = form.getfirst('age', '')

name = re.sub(r'[^a-zA-Z0-9_-]', '', name)

try:
	age_int = int(age)

except ValueError:
	age_int = 0

if name and age_int > 0:
	with open(os.path.join(dir_path, name + '.name'), 'w') as f:
		f.write(str(age_int))

print("Status: 302 Found\r\nLocation: /\r\nContent-type: text/html\r\n\r\n")
