# sub.py
import zmq

context = zmq.Context()
sub_socket = context.socket(zmq.SUB)
sub_socket.connect("ipc:///tmp/ezmb_south.ipc")  # 连接到 proxy 的 XPUB 端

# 订阅多个主题
sub_socket.setsockopt_string(zmq.SUBSCRIBE, "port0_north_topic")
sub_socket.setsockopt_string(zmq.SUBSCRIBE, "port1_north_topic")

print("Subscriber started and waiting for messages...")

try:
    while True:
        topic = sub_socket.recv_string()
        payload = sub_socket.recv_string()
        print(f"[RECV] Topic: {topic}, Message: {payload}")
except KeyboardInterrupt:
    pass
finally:
    sub_socket.close()
    context.term()
