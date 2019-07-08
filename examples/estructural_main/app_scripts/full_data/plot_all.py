import matplotlib.pyplot as plt
import os
import struct



def plot_join(list_texts, us_min, us_max, axis):
	if axis == "x":
		n = 1
	elif axis == "y":
		n = 2
	else:
		n = 3
	for txt in list_texts:
		epoch_list = list()
		axis_list = list()
		with open(txt, "r") as f:
			line = f.readline()
			line = f.readline()
			while line != "":
				params = line.split(";")
				e = int(params[0])
				a = int(params[n])
				if (us_min<=e<us_max):
					epoch_list.append(e)
					axis_list.append(a) 
				line = f.readline()

		plt.plot(epoch_list, axis_list)

	plt.grid(b=1)
	plt.show()

plot_join([":365_0.txt","bd07_0.txt","e45c_0.txt"], 0, 4000000000,"x")
