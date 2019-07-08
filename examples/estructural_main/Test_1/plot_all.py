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

def plot_separate(ensayo,comp,axis):
	fig, axs = plt.subplots(len(comp))	

	if axis == "x":
		n = 1
	elif axis == "y":
		n = 2
	else:
		n = 3

	counter = 0
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

		axs[counter].plot(epoch_list, axis_list)
		axs[counter].grid(b=1)
		axs[counter].set_ylim([-1,1])
		axs[counter].set_xlim(0,200)
		plt.xlabel("Tiempo [s]")
		plt.ylabel("Acceleration [g]")
		axs[counter].set_title(c)
		
		counter += 1
	
	
	plt.show()
	
	pass


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
	ensayo_name = "Ensayo_3"  # Ensayo_1, Ensayo_2 o Ensayo_3
	eje = "x"  # eje x, y o z
	
	# Comparar diferentes dispositivos
	#comp = [("mesa","dev0"), ("wireless","dev2"), ("wireless","dev1"), ("tromino","dev0")]
	#comp = [("mesa","dev0"),("tromino","dev0")]
	comp = [("mesa","dev0"),("mesa","dev1"),("tromino","dev0"), ("wireless","dev0"),("wireless","dev1"),("wireless","dev2")]
	#comp = [("mesa","dev0"),("wireless","dev0")]
	#comp = [("wireless","dev0"),("tromino","dev0")]
	#comp = [("mesa","dev1"), ("wireless","dev0"),("tromino","dev0")]
	#comp = [("wireless","dev0"), ("wireless","dev1"),("wireless","dev2")]
	
	#plot_join(ensayo_name,comp,eje)
	plot_separate(ensayo_name, comp, eje)


