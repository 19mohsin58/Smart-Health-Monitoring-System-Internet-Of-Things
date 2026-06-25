import db
import mqtt_client
import coap_client
import threading
import sys
import time

def print_help():
    print("\n--- Smart Health Control CLI ---")
    print("Available commands:")
    print("  list                       - List all registered nodes in the database directory")
    print("  telemetry                  - Display the last 10 telemetry records from MySQL")
    print("  control-log                - Display the last 10 closed-loop control decisions")
    print("  get <node_id>              - Query a node's live status via direct CoAP GET")
    print("  alert <node_id> <on|off>   - Manually control node alarm/LED via CoAP POST")
    print("  help                       - Show this menu")
    print("  exit / quit                - Close the application")
    print("---------------------------------")

def cmd_list():
    conn = db.get_connection()
    if conn is None:
         return
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT node_id, protocol, registered_at FROM nodes_directory")
        rows = cursor.fetchall()
        print("\n=== Registered Nodes Directory ===")
        print(f"{'Node ID':<25} | {'Protocol':<12} | {'Registered At':<20} | {'IP Address':<20}")
        print("-" * 88)
        for row in rows:
            ip = mqtt_client.node_id_to_ip(row[0])
            print(f"{row[0]:<25} | {row[1]:<12} | {str(row[2]):<20} | {ip:<20}")
        print("==================================")
    except db.mysql.connector.Error as err:
        print(f"[DB ERROR] {err}")
    finally:
        cursor.close()
        conn.close()

def cmd_telemetry():
    conn = db.get_connection()
    if conn is None:
         return
    cursor = conn.cursor()
    try:
        cursor.execute("""
        SELECT node_id, heart_rate, anomaly, alert, received_at 
        FROM telemetry_history 
        ORDER BY received_at DESC 
        LIMIT 10
        """)
        rows = cursor.fetchall()
        print("\n=== Recent Telemetry Logs (MySQL) ===")
        print(f"{'Time':<20} | {'Node ID':<22} | {'HR (BPM)':<8} | {'Anomaly':<7} | {'Alert':<5}")
        print("-" * 74)
        for row in rows:
            print(f"{str(row[4]):<20} | {row[0]:<22} | {row[1]:<8} | {row[2]:<7} | {row[3]:<5}")
        print("=====================================")
    except db.mysql.connector.Error as err:
        print(f"[DB ERROR] {err}")
    finally:
        cursor.close()
        conn.close()

def cmd_control_log():
    conn = db.get_connection()
    if conn is None:
         return
    cursor = conn.cursor()
    try:
        cursor.execute("""
        SELECT node_id, trigger_event, action_taken, executed_at 
        FROM closed_loop_control_log 
        ORDER BY executed_at DESC 
        LIMIT 10
        """)
        rows = cursor.fetchall()
        print("\n=== Closed-Loop Control Actions Log (MySQL) ===")
        print(f"{'Time':<20} | {'Node ID':<22} | {'Trigger Event':<35} | {'Action Taken':<40}")
        print("-" * 125)
        for row in rows:
            print(f"{str(row[3]):<20} | {row[0]:<22} | {row[1]:<35} | {row[2]:<40}")
        print("==================================================")
    except db.mysql.connector.Error as err:
        print(f"[DB ERROR] {err}")
    finally:
        cursor.close()
        conn.close()

def main():
    print("Starting Smart Health Hub...")
    
    # 1. Initialize DB schema
    db.init_db()
    
    # 2. Start MQTT Subscriber in background thread
    client = mqtt_client.start_mqtt_client()
    if client is None:
        print("[MQTT ERROR] Failed to start MQTT broker thread. Exiting.")
        sys.exit(1)
        
    mqtt_thread = threading.Thread(target=client.loop_forever, daemon=True)
    mqtt_thread.start()
    print("[MQTT] Background subscriber thread started.")
    
    # Small pause to let thread establish connection print
    time.sleep(1)
    
    # 3. Launch CLI loop in the main thread
    print_help()
    
    while True:
        try:
            user_input = input("\nhealth-hub> ").strip()
            if not user_input:
                continue
                
            parts = user_input.split()
            cmd = parts[0].lower()
            
            if cmd in ("exit", "quit"):
                print("Exiting Smart Health Hub. Goodbye!")
                break
                
            elif cmd == "help":
                print_help()
                
            elif cmd == "list":
                cmd_list()
                
            elif cmd == "telemetry":
                cmd_telemetry()
                
            elif cmd == "control-log":
                cmd_control_log()
                
            elif cmd == "get":
                if len(parts) < 2:
                    print("Error: Missing node identifier (e.g. 'health-node-0002')")
                    continue
                node_id = parts[1]
                ip = mqtt_client.node_id_to_ip(node_id)
                print(f"[COAP] Sending GET status request to {node_id} at [{ip}]...")
                result = coap_client.coap_get_status(ip)
                print(f"[COAP RESPONSE] {result}")
                
            elif cmd == "alert":
                if len(parts) < 3:
                    print("Error: Usage: 'alert <node_id> <on|off>'")
                    continue
                node_id = parts[1]
                mode = parts[2].lower()
                if mode not in ("on", "off"):
                    print("Error: Mode must be 'on' or 'off'")
                    continue
                ip = mqtt_client.node_id_to_ip(node_id)
                print(f"[COAP] Sending POST actuator request to {node_id} ({ip}) with mode={mode}...")
                result = coap_client.coap_set_actuator(ip, mode)
                print(f"[COAP RESPONSE] Status: {result}")
                
            else:
                print(f"Unknown command: '{cmd}'. Type 'help' for options.")
                
        except KeyboardInterrupt:
            print("\nExiting Smart Health Hub. Goodbye!")
            break
        except Exception as e:
            print(f"[CLI ERROR] {e}")

if __name__ == '__main__':
    main()
