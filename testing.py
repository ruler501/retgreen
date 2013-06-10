#! /usr/bin/python
import os
import glob

print glob.glob("*.png")
for item in glob.glob("*.png"):
	print item
	os.system(os.getcwd()+"/OpenCVTests "+item)