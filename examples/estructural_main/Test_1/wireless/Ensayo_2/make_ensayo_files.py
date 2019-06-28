#!/usr/bin/env python3


import matplotlib.pyplot as plt
import os
import struct
import time 
node = "e45c"
node = "bd07"
node = ":365"
num = "0"

def bit20_to_int(num):
	if (num & 0x80000): # is signed
		return -((num^0xfffff)+1)
	else:
		return num


def append_raw(raw_text, list_x, list_y, list_z, list_e):
	for i in range(0, 260, 13):
		e = int.from_bytes(raw_text[i+0:i+4],byteorder='big')
		x = int.from_bytes(raw_text[i+4:i+7],byteorder='big')
		y = int.from_bytes(raw_text[i+7:i+10],byteorder='big')
		z = int.from_bytes(raw_text[i+10:i+13],byteorder='big')
		if (x == y == z):
			return 0
		list_e.append(e/1000000.0)
		list_x.append(bit20_to_int(x)*0.0000039)
		list_y.append(bit20_to_int(y)*0.0000039)
		list_z.append(bit20_to_int(z)*0.0000039)


def join_files_ot(foldername_data,name_procesed_txt):
	list_x = list()
	list_y = list()
	list_z = list()
	list_e = list()

	list_of_files = os.listdir("{}/".format(foldername_data))
	list_of_files.sort()
	i = 0
	for filename in list_of_files:
		with open("{}/{}".format(foldername_data, filename),'rb') as f:
			raw_text = f.read()
			append_raw(raw_text, list_x, list_y, list_z, list_e)


	with open(name_procesed_txt, "w") as f:
		f.write("epoch;x;y;z\r\n")
		for i in range(len(list_e)):
			f.write("{};{};{};{}\r\n".format(list_e[i]-list_e[0], list_x[i], list_y[i], list_z[i]))
	

if __name__ == "__main__":
	join_files_ot("dev0", "wireless_Ensayo_2_dev0")
	join_files_ot("dev1", "wireless_Ensayo_2_dev1")
	join_files_ot("dev2", "wireless_Ensayo_2_dev2")

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


