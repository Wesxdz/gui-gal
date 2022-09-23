import socket
import sys

host = "localhost"
port = 9999

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
remote_ip = socket.gethostbyname(host)
s.connect((remote_ip, port))
request = b"Example"
s.sendall(request)
reply = s.recv(4096)
print(reply)