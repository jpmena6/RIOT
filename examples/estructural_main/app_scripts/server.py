
# To use:
# Make router use fd11::/64 as prefix as ULA
# Asign fd11::100 to server
# ip -6 addr add fd11::100/64 dev wlan0
# wlp3s0
# Run on the server using python2

import socket
from datetime import datetime
import filecmp
import os
IP = "fd11::100"
PORT = 8888

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.bind((IP,PORT))

list_of_nodes = list()

class Node:
	def __init__(self, ipv6, sock, port):
		self.ipv6 = ipv6
		self.sock = sock
		self.port = port
		self.directory = self.ipv6[-4:]

	def move_data(self):
		try:
			os.mkdir("full_data")
		except Exception:
			pass
		
		new_dir = 0
		counter = 0
		while not new_dir:
			try:
				os.mkdir("full_data/{}_{}".format(self.directory, counter))
				new_dir = 1
			except Exception:
				counter += 1

		os.system("mv {}/* full_data/{}_{}/".format(self.directory,self.directory, counter))

	def missing_page(self):
		list_of_files = os.listdir(self.directory)
		list_of_files.sort()
		idealpage = 0
		for namefile in list_of_files:
			page = int(namefile[0:4])
			if (page > idealpage):
				return idealpage
			elif (page < idealpage): # duplication
				print "on file {}, node {} (DUP!)".format(page, self.directory)
				if filecmp.cmp("{}/{}".format(self.directory,namefile), "{}/{}".format(self.directory,lastfilename)):
					os.remove("{}/{}".format(self.directory,lastfilename))
				else:
					print "Have two files for same page but contents differ! manually select.."
					os.remove("{}/{}".format(self.directory,lastfilename)) # this should not go here
					#raise
			else:
				idealpage += 1

			lastfilename=namefile
			
		# check data and return missing page
		if (idealpage <= 2048):
			return idealpage	
		return None

	def restart_node(self):
		self.sock.sendto("R", (self.ipv6, self.port))
		return True

	def clear_memory(self):
		self.sock.sendto("F", (self.ipv6, self.port))
		return True

	def request_page(self, page):
		print("Req. page: S{} to node {}".format(page, self.directory))
		self.sock.sendto("S{}".format(page), (self.ipv6, self.port))
		return

	def manage_node(self):
		m = self.missing_page()
		#m = None # DEBUG ONLY
		if (m != None):
			self.request_page(m)
		else:
			print "Have all data for node {}, clearing memory and reseting..".format(self.directory)
			self.move_data()
			self.clear_memory()
			self.restart_node()

	def process(self, data):
		if (data[0] == 'S'):
			self.save(data)
		elif(data[0] == 'D'):
			self.manage_node()

	def save(self, data):
		page = data[1:5]
		now = datetime.now()
		date = now.strftime("%m-%d-%Y-%H-%M-%S")
		try:
			os.mkdir(self.directory)
		except Exception:
			pass
		d = "{}/{}_{}.txt".format(self.directory, page, date)
		with open(d, "w") as f:
			f.write(data[8:])
		return True


while True:
	data,addr = sock.recvfrom(1024)
	ipv6 = addr[0]
	gotnode = 0
	for node in list_of_nodes:
		if node.ipv6 == ipv6:
			node.process(data)
			gotnode = 1
	if not gotnode:
		new_node = Node(ipv6, sock, PORT)
		list_of_nodes.append(new_node)
		new_node.process(data)
		





