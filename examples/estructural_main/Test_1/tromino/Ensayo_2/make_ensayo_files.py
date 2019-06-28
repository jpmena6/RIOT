#!/usr/bin/env python3


def make_file_tromino(dataname,name_procesed_txt):
	list_x = list()
	list_y = list()
	list_z = list()
	list_e = list()
	t = 0
	with open("dev0/EqualizedFile.dat", "r") as f:
		[f.readline() for i in range(38)] # headers
		line = f.readline()
		#print(line.split(" "))
		while line != "":
			list_line = line.split(" ")
			list_x.append(float(list_line[6])) #intercambiados
			list_y.append(float(list_line[7]))
			list_z.append(float(list_line[8]))
			t = t + 1/512.0 # time not included
			list_e.append(t)
			line = f.readline()


	with open(name_procesed_txt, "w") as f:
		f.write("epoch;x;y;z\r\n")
		for i in range(len(list_e)):
			f.write("{};{};{};{}\r\n".format(list_e[i], list_x[i], list_y[i], list_z[i]))
	

if __name__ == "__main__":
	make_file_tromino("EqualizedFile.dat", "tromino_Ensayo_2_dev0")

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


