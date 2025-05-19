import socket
import sys

def test_echo_server(host, port):
    try:
        # Resolve the host's IP address
        ip_address = socket.gethostbyname(host)
        print(f"Resolved IP address of {host}: {ip_address}")

        # Create a TCP connection to the echo server
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                print(f"Connecting to {ip_address}:{port}")
                s.connect((ip_address, port))
                print("Connection successful!")
            except socket.error as e:
                print(f"Failed to connect to {ip_address}:{port} - {e}")
                return

            # Define the message
            message = "Hello, Echo Server!"

            try:
                # Print the message being sent
                print(f"Sending message: {message}")
                s.sendall(message.encode())
                print("Message sent successfully!")
            except socket.error as e:
                print(f"Failed to send message - {e}")
                return

            # Receive the response
            try:
                data = s.recv(1024)
                print(f"Received: {data.decode()}")
            except socket.error as e:
                print(f"Failed to receive data - {e}")
    except socket.gaierror:
        print(f"Failed to resolve host: {host}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <host> <port>")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2])
    test_echo_server(host, port)
