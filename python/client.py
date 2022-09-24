import socket
import os
from venv import create
import threading

def create_server():
    # os.system("python ../deps/stable-diffusion/optimizedSD/simple_txt2img.py")
    os.system("python server.py")

# class thread(threading.Thread):
#     def __init__(self, thread_name, thread_ID):
#         threading.Thread.__init__(self)
#         self.thread_name = thread_name
#         self.thread_ID = thread_ID
 
#         # helper function to execute the threads
#     def run(self):
#         print(str(self.thread_name) +" "+ str(self.thread_ID));
#         create_server()

# def request_image(prompt):
#     s.sendall(request)
#     request = b"Example"
#     reply = s.recv(4096)
#     print(reply)

# thread = thread("server", 0)
# create_server()
host = "localhost"
port = 9999

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
remote_ip = socket.gethostbyname(host)
s.connect((remote_ip, port))
s.sendall(b'I have always loved chocolate!')
data = s.recv(1024)
print('Received', repr(data))