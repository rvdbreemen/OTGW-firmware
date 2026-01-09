
import sys
import socket
import threading
import webbrowser
import time

# Configuration
LOCAL_PORT = 8080
BUFFER_SIZE = 4096

def handle_client(client_socket, target_host, target_port):
    try:
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.connect((target_host, target_port))
        
        # Start bidirectional forwarding
        t1 = threading.Thread(target=forward, args=(client_socket, server_socket))
        t2 = threading.Thread(target=forward, args=(server_socket, client_socket))
        t1.start()
        t2.start()
        
        t1.join()
        t2.join()
    except Exception as e:
        pass
    finally:
        try:
            client_socket.close()
        except: pass
        try:
            server_socket.close()
        except: pass

def forward(source, destination):
    try:
        while True:
            data = source.recv(BUFFER_SIZE)
            if not data:
                break
            destination.sendall(data)
    except:
        pass
    finally:
        try:
            destination.shutdown(socket.SHUT_RDWR)
        except: pass
        try:
            destination.close()
        except: pass

def start_proxy(target_ip, target_port=80):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server.bind(('127.0.0.1', LOCAL_PORT))
    except Exception as e:
        print(f"Error binding to local port {LOCAL_PORT}: {e}")
        return

    server.listen(5)
    print(f"\n[+] Proxy started on localhost:{LOCAL_PORT}")
    print(f"[+] Forwarding to {target_ip}:{target_port}")
    print(f"[+] Secure Context: ACTIVE (Supported by Browser)")
    
    # Open browser automatically
    print("[+] Opening browser...")
    webbrowser.open(f"http://localhost:{LOCAL_PORT}")
    
    try:
        while True:
            client_sock, addr = server.accept()
            proxy_thread = threading.Thread(
                target=handle_client,
                args=(client_sock, target_ip, target_port)
            )
            proxy_thread.daemon = True
            proxy_thread.start()
    except KeyboardInterrupt:
        print("\n[!] Stopping proxy...")
        server.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python webui_launcher.py <OTGW_IP_ADDRESS>")
        print("Example: python webui_launcher.py 192.168.1.50")
        
        # Interactive mode
        target = input("\nEnter OTGW IP Address: ").strip()
        if target:
             start_proxy(target)
    else:
        start_proxy(sys.argv[1])
