import zmq
import time

context = zmq.Context()
pub_socket = context.socket(zmq.PUB)
pub_socket.connect("ipc:///tmp/ezmb_north.ipc")

time.sleep(1)

count = 0
try:
    while True:
        topic0 = b"port0_south_topic"
        topic1 = b"port1_south_topic"
        msg0 = f"Message {count} from port0".encode('utf-8')
        msg1 = f"Message {count} from port1".encode('utf-8')

        pub_socket.send_multipart([topic0, msg0])
        
        time.sleep(0.5)
        
        pub_socket.send_multipart([topic1, msg1])

        print(f"Published:\n  [{topic0.decode()}] {msg0.decode()}\n  [{topic1.decode()}] {msg1.decode()}")
        count += 1
        time.sleep(1)
        
except KeyboardInterrupt:
    print("\nPublisher interrupted")
finally:
    pub_socket.close()
    context.term()