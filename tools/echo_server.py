import socket

HOST = "0.0.0.0"  # Listen on all interfaces
PORT = 7        # Port to listen on

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    print(f"Server is listening on {HOST}:{PORT}")

    while True:
        conn, addr = s.accept()
        with conn:
            # Print the remote client's IP address and port
            print(f"Connected by {addr[0]}, Port: {addr[1]}")
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                conn.sendall(data)
                print(data)
