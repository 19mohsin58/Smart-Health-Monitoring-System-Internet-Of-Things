#!/usr/bin/env python3
"""
==============================================================================
               Smart Health Backend Controller (main.py)
==============================================================================

Objective:
  Launches the operator's shell CLI, starts background subscriber scripts,
  and runs the performance monitoring thread that enforces closed-loop
  congestion control.

Functions & Logic:
  - Command CLI Loop: Exposes list, telemetry, metrics, and remote control pings.
  - Active Neighbors Filter: Queries only the nodes active in the last 1 minute.
  - Congestion Detector Thread: Automates remote CoAP switches dynamically.
==============================================================================
"""

import db
import mqtt_client
import coap_client
import threading
import sys
import time

# Global variable to track whether network-wide congestion is currently active
congestion_active = False

def print_help():
    """Prints the help menu with all available CLI commands."""
    print("\n--- Smart Health Control CLI ---")
    print("Available commands:")
    print("  list                       - List all registered nodes in the database directory")
    print("  telemetry                  - Display the last 10 telemetry records from MySQL")
    print("  control-log                - Display the last 10 closed-loop control decisions")
    print("  metrics                    - Display the last 10 network performance metric records")
    print("  get <node_id>              - Query a node's live status via direct CoAP GET")
    print("  alert <node_id> <on|off>   - Manually control node alarm/LED via CoAP POST")
    print("  help                       - Show this menu")
    print("  exit / quit                - Close the application")
    print("---------------------------------")


def cmd_metrics():
    """Queries and displays the last 10 network metrics records from the MySQL database."""
    conn = db.get_connection()
    if conn is None:
        return
    cursor = conn.cursor()
    try:
        cursor.execute("""
        SELECT timestamp, pdr, latency_ms, congestion_active 
        FROM network_metrics 
        ORDER BY timestamp DESC 
        LIMIT 10
        """)
        rows = cursor.fetchall()
        print("\n=== Recent Network Performance Metrics (MySQL) ===")
        print(f"{'Time':<20} | {'PDR (%)':<10} | {'Latency (ms)':<15} | {'Congestion Active':<18}")
        print("-" * 72)
        for row in rows:
            print(f"{str(row[0]):<20} | {row[1]:<10.2f} | {row[2]:<15.2f} | {'ON' if row[3] == 1 else 'OFF':<18}")
        print("==================================================")
    except db.mysql.connector.Error as err:
        print(f"[DB ERROR] {err}")
    finally:
        cursor.close()
        conn.close()


def metrics_background_worker():
    """
    Periodic background thread that measures network latency, PDR,
    and updates the closed-loop congestion control state.
    """
    global congestion_active
    
    # Pause initially to allow nodes to boot and register
    time.sleep(15)
    
    while True:
        try:
            conn = db.get_connection()
            if conn is None:
                time.sleep(10)
                continue
                
            cursor = conn.cursor()
            
            # Fetch nodes that have sent telemetry in the last 1 minute to avoid offline timeouts
            cursor.execute("""
                SELECT DISTINCT node_id 
                FROM telemetry_history 
                WHERE received_at >= NOW() - INTERVAL 1 MINUTE
            """)
            nodes = [row[0] for row in cursor.fetchall()]
            cursor.close()
            conn.close()
            
            # If no nodes have sent telemetry recently, skip evaluation
            if not nodes:
                time.sleep(5)
                continue
                
            latencies = []
            for node_id in nodes:
                ip = mqtt_client.node_id_to_ip(node_id)
                if ip == "unknown":
                    continue
                    
                # Measure CoAP GET latency synchronously
                start_time = time.time()
                res = coap_client.coap_get_status(ip)
                elapsed = (time.time() - start_time) * 1000.0
                
                # Check for timeouts or connection errors
                if res is None or (isinstance(res, str) and res.startswith("Error")):
                    latencies.append(2000.0)  # Default 2-second timeout placeholder
                else:
                    latencies.append(elapsed)
            
            # Compute averages
            avg_latency = sum(latencies) / len(latencies) if latencies else 0.0
            avg_pdr = mqtt_client.get_average_pdr()
            
            # CLOSED-LOOP TRIGGER: Enable congestion mode if latency > 300ms or PDR < 85%
            if (avg_pdr < 85.0 or avg_latency > 300.0) and not congestion_active:
                congestion_active = True
                print(f"\n[CLOSED-LOOP CONGESTION SIGNAL] Network Congestion Detected (PDR: {avg_pdr:.1f}%, Latency: {avg_latency:.1f} ms). Remotely enabling Value-Based Reporting (Mechanism 2) on all nodes!")
                # Alert all nodes
                for node_id in nodes:
                    ip = mqtt_client.node_id_to_ip(node_id)
                    coap_client.coap_set_actuator(ip, "congestion_on")
                    
            # CLOSED-LOOP RECOVERY: Disable congestion mode if latency <= 200ms and PDR >= 90%
            elif (avg_pdr >= 90.0 and avg_latency <= 200.0) and congestion_active:
                congestion_active = False
                print(f"\n[CLOSED-LOOP CONGESTION CLEARED] Network Congestion Cleared (PDR: {avg_pdr:.1f}%, Latency: {avg_latency:.1f} ms). Disabling Value-Based Reporting on all nodes.")
                # Revert all nodes
                for node_id in nodes:
                    ip = mqtt_client.node_id_to_ip(node_id)
                    coap_client.coap_set_actuator(ip, "congestion_off")
            
            # Write metrics payload log to MySQL
            conn = db.get_connection()
            if conn:
                cursor = conn.cursor()
                cursor.execute("""
                INSERT INTO network_metrics (pdr, latency_ms, congestion_active)
                VALUES (%s, %s, %s)
                """, (avg_pdr, avg_latency, 1 if congestion_active else 0))
                conn.commit()
                cursor.close()
                conn.close()
                
        except Exception as e:
            # Catch all thread exceptions to prevent thread exit
            pass
            
        # Poll every 10 seconds
        time.sleep(10)


def cmd_list():
    """Queries and displays all registered nodes in the database directory."""
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
    """Queries and displays the last 10 telemetry records from telemetry_history."""
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
    """Queries and displays the last 10 closed-loop control log records from MySQL."""
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
        print("\n=== Recent Closed-Loop Control Decisions ===")
        print(f"{'Time':<20} | {'Node ID':<22} | {'Trigger Event':<28} | {'Action Executed':<22}")
        print("-" * 98)
        for row in rows:
            print(f"{str(row[3]):<20} | {row[0]:<22} | {row[1]:<28} | {row[2]:<22}")
        print("============================================")
    except db.mysql.connector.Error as err:
        print(f"[DB ERROR] {err}")
    finally:
        cursor.close()
        conn.close()


def main():
    """Application entry point: initializes database, starts threads, and processes user inputs."""
    print("Starting Smart Health Hub...")
    db.init_db()
    
    # Initialize background MQTT subscriber thread
    mqtt_client.start_mqtt_client()
    
    # Start metrics background monitoring thread
    metrics_thread = threading.Thread(target=metrics_background_worker, daemon=True)
    metrics_thread.start()
    print("[METRICS] Background metrics logger thread started.")
    
    # Print the CLI options help menu
    print_help()
    
    # Enter operator interactive input processing loop
    while True:
        try:
            line = input("health-hub> ").strip()
            if not line:
                continue
            
            parts = line.split()
            cmd = parts[0].lower()
            
            if cmd in ['exit', 'quit']:
                print("Exiting Smart Health Hub. Goodbye!")
                break
                
            elif cmd == 'help':
                print_help()
                
            elif cmd == 'list':
                cmd_list()
                
            elif cmd == 'telemetry':
                cmd_telemetry()
                
            elif cmd == 'control-log':
                cmd_control_log()
                
            elif cmd == 'metrics':
                cmd_metrics()
                
            elif cmd == 'get':
                # Direct CoAP GET status query to a node
                if len(parts) < 2:
                    print("Usage: get <node_id>")
                    continue
                node_id = parts[1]
                ip = mqtt_client.node_id_to_ip(node_id)
                if ip == "unknown":
                    print(f"Unknown node ID: {node_id}")
                    continue
                print(f"Sending CoAP GET request to {node_id} at [{ip}]...")
                res = coap_client.coap_get_status(ip)
                print(f"Response: {res}")
                
            elif cmd == 'alert':
                # Direct CoAP POST alert modification command to a node
                if len(parts) < 3 or parts[2].lower() not in ['on', 'off']:
                    print("Usage: alert <node_id> <on|off>")
                    continue
                node_id = parts[1]
                mode = parts[2].lower()
                ip = mqtt_client.node_id_to_ip(node_id)
                if ip == "unknown":
                    print(f"Unknown node ID: {node_id}")
                    continue
                print(f"Sending CoAP POST payload mode={mode} to {node_id} at [{ip}]...")
                res = coap_client.coap_set_actuator(ip, mode)
                print(f"Response Status: {res}")
                
            else:
                print(f"Unknown command: '{cmd}'. Type 'help' for options.")
                
        except (KeyboardInterrupt, EOFError):
            print("\nExiting Smart Health Hub. Goodbye!")
            break


if __name__ == '__main__':
    main()
