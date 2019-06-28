#!/usr/bin/env python3

# Eje x corresponde al movimiento de la mesa

# mesa 2 dispositivos:
# dev0 (solo eje x)
# dev1 (solo eje x)

# tromino 1 dispositivo
# dev0 (3 ejes)

# wireless 3 dispositivos:
# dev0 (3 ejes)
# dev1 (3 ejes)
# dev2 (3 ejes)



import matplotlib.pyplot as plt
import os


def plot_join(ensayo,comp, axis):
	if axis == "x":
		n = 1
	elif axis == "y":
		n = 2
	else:
		n = 3


	for c in comp:
		fpath = "{}/{}/{}_{}_{}".format(c[0],ensayo,c[0],ensayo, c[1])
		epoch_list = list()
		axis_list = list()
		with open(fpath, "r") as f:
			line = f.readline()
			line = f.readline()
			while line != "":
				params = line.split(";")
				e = float(params[0])
				a = float(params[n])
				epoch_list.append(e)
				axis_list.append(a) 
				line = f.readline()

		plt.plot(epoch_list, axis_list, alpha=0.7)

	plt.grid(b=1)
	plt.xlabel("Tiempo [s]")
	plt.ylabel("Acceleration [g]")
	plt.legend(comp)
	plt.show()

if __name__ == "__main__":
	ensayo_name = "Ensayo_2"  # Ensayo_1, Ensayo_2 o Ensayo_3
	eje = "x"  # eje x, y o z
	
	# Comparar diferentes dispositivos
	#comp = [("mesa","dev0"), ("wireless","dev2"), ("wireless","dev1"), ("tromino","dev0")]
	#comp = [("mesa","dev0"),("tromino","dev0")]
	#comp = [("mesa","dev0"), ("wireless","dev2"),("wireless","dev1")]
	comp = [("wireless","dev0"), ("wireless","dev1"),("wireless","dev2")]
	
	plot_join(ensayo_name,comp,eje)



