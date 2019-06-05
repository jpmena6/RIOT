# run with python3

import matplotlib.pyplot as plt
import os
import struct
import time 
node = "e45c"
node = "bd07"
#node = ":365"
num = "9"

def bit20_to_int(num):
	if (num & 0x80000): # is signed
		return -((num^0xfffff)+1)
	else:
		return num


def append_raw(raw_text, list_x, list_y, list_z, list_e):
	for i in range(0, 260, 13):
		e = int.from_bytes(raw_text[i+0:i+4],byteorder='big')
		#e = float(int.from_bytes(raw_text[i+0:i+2],byteorder='big'))
		#e = e*2**16+float(int.from_bytes(raw_text[i+2:i+4],byteorder='big'))
		x = int.from_bytes(raw_text[i+4:i+7],byteorder='big')
		y = int.from_bytes(raw_text[i+7:i+10],byteorder='big')
		z = int.from_bytes(raw_text[i+10:i+13],byteorder='big')
		if (x == y == z):
			return 0
		list_e.append(e)
		list_x.append(bit20_to_int(x))
		list_y.append(bit20_to_int(y))
		list_z.append(bit20_to_int(z))

list_x = list()
list_y = list()
list_z = list()
list_e = list()

list_of_files = os.listdir("{}_{}/".format(node, num))
list_of_files.sort()
i = 0
for filename in list_of_files:
	with open("{}_{}/{}".format(node, num, filename),'rb') as f:
		#print("{}_{}/{}".format(node, num, filename))
		raw_text = f.read()
		append_raw(raw_text, list_x, list_y, list_z, list_e)


with open("{}_{}.txt".format(node, num), "w") as f:
	f.write("epoch;x;y;z\r\n")
	for i in range(len(list_e)):
		f.write("{};{};{};{}\r\n".format(list_e[i], list_x[i], list_y[i], list_z[i]))
	

#print(time.localtime(list_e[0]/1000000))
#print(max(list_e) - min(list_e))


plt.plot(list_e,list_x)
plt.plot(list_e,list_y)
plt.plot(list_e,list_z)
#plt.plot(list_e)
#plt.plot(list_x)
#plt.plot(list_y)
#plt.plot(list_z)
plt.grid(b=1)
plt.show()

plt.plot(list_e,'o')
plt.grid(b=1)
plt.show()


