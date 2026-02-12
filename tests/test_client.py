import socket
import time

def test_server():
    try:
        # Create a socket object
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Connect to the server
        print("Connecting to server...")
        s.connect(('localhost', 9090))
        
        # Send data
        message = "Testing Phase 1 Connectivity"
        print(f"Sending: {message}")
        s.sendall(message.encode())
        
        # Close
        s.close()
        print("Test Complete.")
        
    except ConnectionRefusedError:
        print("Error: Connection refused. Is the server running?")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_server()