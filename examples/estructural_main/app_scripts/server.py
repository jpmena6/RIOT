import socket
from datetime import datetime
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
				pass
			else:
				idealpage += 1
			
		# check data and return missing page		
		return None

	def restart_node(self):
		self.sock.sendto("R", (self.ipv6, self.port))
		return True

	def clear_memory(self):
		self.sock.sendto("F", (self.ipv6, self.port))
		return True

	def request_page(self, page):
		print("Requesting page: S{}".format(page))
		self.sock.sendto("S{}".format(page), (self.ipv6, self.port))
		return

	def manage_node(self):
		m = self.missing_page()
		#m = None # DEBUG ONLY
		if (m != None):
			self.request_page(m)
		else:
			self.move_data()
			self.clear_memory()
			self.restart_node()

	def process(self, data):
		if (data[0] == 'S'):
			self.save(data)
		elif(data[0] == 'D'):
			print "Received Data ready !"
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
		





