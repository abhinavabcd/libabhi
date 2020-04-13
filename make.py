import os
import sys

import shlex
import subprocess
from functools import reduce as _reduce 

cur_dir = os.path.dirname(os.path.abspath(__file__))

def run_cmd(*cmd, callback=None, watch=False):
	if watch and not callback:
		raise RuntimeError('You must provide a callback when watching a process.')

	cmd = " ".join(cmd)
	print(cmd)
	output = None
	try:
		proc = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE)

		if watch:
			while proc.poll() is None:
				line = proc.stdout.readline()
				if line != "":
					callback(line)

			# Sometimes the process exits before we have all of the output, so
			# we need to gather the remainder of the output.
			remainder = proc.communicate()[0]
			if remainder:
				callback(remainder)
		else:
			output = proc.communicate()[0]
	except Exception as ex:
		err = str(sys.exc_info()[1]) + "\n" + str(ex) + "\n"
		output = err.encode()

	if callback and output is not None:
		callback(output)
		return None

	print(output.decode())
	return output



CFLAGS = "-std=c++17 -g -Wall -fPIC -fpermissive -DLIBCO_MP"
CC = "g++"

os.chdir(cur_dir + "/libco")
run_cmd(CC , CFLAGS , "-I." ,"-o", "libco ", "-c", "libco.c")


# to_join = []
# os.chdir(cur_dir + "/libco")
# for i in os.listdir("./"):
# 	if(i.endswith(".c")):
# 		file_name = i[ : i.rfind(".")]
# 		run_cmd(CC , CFLAGS , "-I." ,"-o", file_name + ".o ", "-c", i)
# 		to_join.append(file_name + ".o")

# run_cmd(CC , "-o" ,"libco",  CFLAGS, *to_join) 
# oc.chdir(cur_dir)



