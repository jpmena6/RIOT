import socket

IP = "fd11::100"
PORT = 8888

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)

sock.bind((IP,PORT))
while True:
	data,addr = sock.recvfrom(1024)
	print "received: {}, from {}".format(data,addr)
