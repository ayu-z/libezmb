import zmq
import time

context = zmq.Context()
pub_socket = context.socket(zmq.PUB)
pub_socket.connect("ipc:///tmp/ezmb_north.ipc")  # 连接到 proxy 的 XSUB

time.sleep(1)  # 等待订阅者连接

count = 0
try:
    while True:
        # 准备消息
        topic0 = b"port0_south_topic"  # 注意：使用字节串而不是字符串
        topic1 = b"port1_south_topic"
        msg0 = f"Message {count} from port0".encode('utf-8')
        msg1 = f"Message {count} from port1".encode('utf-8')

        # 发送第一组消息
        pub_socket.send_multipart([topic0, msg0])  # 使用 send_multipart 发送多帧
        
        time.sleep(0.5)  # 短暂间隔
        
        # 发送第二组消息
        pub_socket.send_multipart([topic1, msg1])

        print(f"Published:\n  [{topic0.decode()}] {msg0.decode()}\n  [{topic1.decode()}] {msg1.decode()}")
        count += 1
        time.sleep(1)
        
except KeyboardInterrupt:
    print("\nPublisher interrupted")
finally:
    pub_socket.close()
    context.term()