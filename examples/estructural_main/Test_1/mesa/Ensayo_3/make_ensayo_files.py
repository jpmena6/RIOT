#!/usr/bin/env python3
# -*- coding: latin-1 -*-

# mesa file has two different devices
def make_file_mesa(name_procesed_1,name_procesed_2):
	list_dev0 = list()
	list_dev1 = list()
	list_e = list()

	with open("devx/ensayo.csv", "r") as f:
		[f.readline() for i in range(50)] # headers
		line = f.readline()
		while line != "":
			list_line = line.split(";")
			list_dev0.append(float(list_line[1]))
			list_dev1.append(float(list_line[2]))
			list_e.append(float(list_line[0]))
			line = f.readline()


	with open(name_procesed_1, "w") as f:
		f.write("epoch;x;y;z\r\n")
		for i in range(len(list_e)):
			f.write("{};{};{};{}\r\n".format(list_e[i], list_dev0[i], 0, 0))

	with open(name_procesed_2, "w") as f:
		f.write("epoch;x;y;z\r\n")
		for i in range(len(list_e)):
			f.write("{};{};{};{}\r\n".format(list_e[i], list_dev1[i], 0, 0))
	

if __name__ == "__main__":
	make_file_mesa("mesa_Ensayo_3_dev0", "mesa_Ensayo_3_dev1")

#print(time.localtime(list_e[0]/1000000))
#print(max(list_e) - min(list_e))


#plt.plot(list_e,list_x)
#plt.plot(list_e,list_y)
#plt.plot(list_e,list_z)
#plt.plot(list_e)
#plt.plot(list_x)
#plt.plot(list_y)
#plt.plot(list_z)
#plt.grid(b=1)
#plt.show()

#plt.plot(list_e,'o')
#plt.grid(b=1)
#plt.show()


