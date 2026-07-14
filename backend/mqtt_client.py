import paho.mqtt.client as mqtt
import json
import db
import threading
import time
from coap_client import coap_set_actuator

# ==============================================================================
# MQTT Client Callback & Telemetry Handler (mqtt_client.py)
# ==============================================================================
# Objective:
#   Runs in a background thread to manage MQTT broker connections, subscribe
#   to incoming patient telemetry, compute packet stats (PDR), scrape IPv6
#   addresses from the Border Router, and log telemetry inputs to MySQL.
# ==============================================================================

# Global sequence number tracker for computing packet delivery ratio (PDR)
# Format: { node_id: { "received_seqs": set(), "first_seq": int, "last_seq": int, "pdr": float, "last_seen": float } }
node_seq_tracker = {}

# Locks to synchronize concurrent thread access to shared global states
seq_tracker_lock = threading.Lock()
ip_cache_lock = threading.Lock()

def track_pdr(node_id, seq):
    """
    Tracks sequence gaps to calculate the Packet Delivery Ratio (PDR) for a node.
    Detects reboots and sequence resets automatically.
    """
    if not node_id:
        return 100.0
    
    global node_seq_tracker
    
    with seq_tracker_lock:
        if node_id not in node_seq_tracker:
            node_seq_tracker[node_id] = {
                "received_seqs": set(),
                "first_seq": seq,
                "last_seq": seq,
                "pdr": 100.0,
                "last_seen": time.time()
            }
        
        tracker = node_seq_tracker[node_id]
        tracker["last_seen"] = time.time()
        
        # If the sequence restarts (indicates mote rebooted/reflashed)
        if seq < tracker["last_seq"]:
            tracker["received_seqs"] = set()
            tracker["first_seq"] = seq
            tracker["last_seq"] = seq
            
        # Append the sequence number to received set
        tracker["received_seqs"].add(seq)
        tracker["last_seq"] = max(tracker["last_seq"], seq)
        
        # Expected vs. actual received count
        expected = tracker["last_seq"] - tracker["first_seq"] + 1
        received = len(tracker["received_seqs"])
        
        if expected <= 0:
            pdr = 100.0
        else:
            pdr = (received / expected) * 100.0
            
        tracker["pdr"] = pdr
        return pdr

def get_average_pdr():
    """Computes the overall average PDR across active nodes within the last minute."""
    global node_seq_tracker
    with seq_tracker_lock:
        now = time.time()
        active_pdrs = []
        for tracker in node_seq_tracker.values():
            # Only include nodes active in the last 1 minute (60 seconds)
            if now - tracker.get("last_seen", 0) <= 60.0:
                active_pdrs.append(tracker["pdr"])
        if not active_pdrs:
            return 100.0
        return sum(active_pdrs) / len(active_pdrs)

# Global IP cache to avoid repeated slow HTTP GET requests to the Border Router
ip_cache = {}

def get_real_ip_from_br(suffix):
    """
    Dynamic physical IP scraper:
    Connects to the Border Router webserver at fd00::f6ce:364a:bcf9:e639,
    extracts the active neighbor links, and searches for the IP matching the suffix.
    """
    import urllib.request
    import re
    
    # Border Router physical address
    br_ips = ["fd00::f6ce:364a:bcf9:e639"]
    for br_ip in br_ips:
        try:
            url = f"http://[{br_ip}]/"
            req = urllib.request.Request(url, method="GET")
            with urllib.request.urlopen(req, timeout=1.0) as response:
                html = response.read().decode('utf-8')
                # Parse all IPv6 link addresses in page HTML
                ips = re.findall(r'fd00::[0-9a-fA-F:]+', html)
                for ip in ips:
                    if ip.endswith(suffix):
                        return ip
        except Exception:
            pass
    return None

def node_id_to_ip(node_id):
    """
    Maps node IDs (e.g. 'health-node-4d7c') to IPv6 addresses.
    Utilizes dynamic Border Router lookup first, falling back to a safe
    formatting model to prevent parsing crashes in CoAP libraries.
    """
    global ip_cache
    with ip_cache_lock:
        if node_id in ip_cache:
            return ip_cache[node_id]

    try:
        parts = node_id.split("-")
        suffix = parts[-1]  # Suffix e.g. '4d7c' or '0002'
        
        # Try scraping dynamic physical IP from Border Router
        real_ip = get_real_ip_from_br(suffix)
        with ip_cache_lock:
            if real_ip:
                ip_cache[node_id] = real_ip
                return real_ip
                
            # Fallback to Cooja-style static mapping for compatibility (preserving low IDs for Cooja)
            val = int(suffix, 16)
            if val < 256:
                # Low values (e.g. 2 -> fd00::202:2:2:2) match Cooja node topologies
                ip_cache[node_id] = f"fd00::20{val}:{val}:{val}:{val}"
            else:
                # High hexadecimal values (MAC addresses) are mapped to format-safe IPv6 addresses
                ip_cache[node_id] = f"fd00::2000:{suffix}:{suffix}:{suffix}"
            return ip_cache[node_id]
    except Exception:
        return "unknown"

def on_connect(client, userdata, flags, rc):
    """Callback executed when the client establishes connection with broker."""
    print(f"[MQTT] Connected to broker with result code {rc}")
    # Subscribe to health telemetry wildcard topic
    client.subscribe("health/node/#")

def on_message(client, userdata, msg):
    """Decodes JSON packets and updates MySQL databases accordingly."""
    try:
        topic = msg.topic
        payload_str = msg.payload.decode('utf-8')
        
        try:
            data = json.loads(payload_str)
        except json.JSONDecodeError:
            print(f"[MQTT ERROR] Failed to parse JSON: {payload_str}")
            return
            
        conn = db.get_connection()
        if conn is None:
            return
            
        cursor = conn.cursor()
        try:
            if topic == "health/node/register":
                # Handle Dynamic Registration Topic
                node_id = data.get("node")
                node_type = data.get("type", "sensor")
                domain = data.get("domain", "smart-health")
                sensor_type = data.get("sensor", "heart-rate")
                
                cursor.execute("""
                INSERT INTO nodes_directory (node_id, type, domain, sensor_type, protocol)
                VALUES (%s, %s, %s, %s, 'mqtt+coap')
                ON DUPLICATE KEY UPDATE 
                    type = VALUES(type),
                    domain = VALUES(domain),
                    sensor_type = VALUES(sensor_type)
                """, (node_id, node_type, domain, sensor_type))
                conn.commit()
                print(f"[MQTT] Registered node {node_id} (IP: {node_id_to_ip(node_id)}) in directory.")
                
            elif topic == "health/node/status":
                # Handle Vitals Status Telemetry Topic
                node_id = data.get("node")
                seq = data.get("seq", 0)
                heart_rate = data.get("heart_rate")
                anomaly = data.get("anomaly")
                alert = data.get("alert")
                
                # Perform PDR calculation
                pdr = track_pdr(node_id, seq)
                
                # Auto-register directory if missed
                cursor.execute("SELECT node_id FROM nodes_directory WHERE node_id = %s", (node_id,))
                if cursor.fetchone() is None:
                    cursor.execute("""
                    INSERT INTO nodes_directory (node_id, type, domain, sensor_type, protocol)
                    VALUES (%s, 'sensor', 'smart-health', 'heart-rate', 'mqtt+coap')
                    """, (node_id,))
                    conn.commit()
                    
                # Store telemetry record in MySQL history
                cursor.execute("""
                INSERT INTO telemetry_history (node_id, seq, heart_rate, anomaly, alert)
                VALUES (%s, %s, %s, %s, %s)
                """, (node_id, seq, heart_rate, anomaly, alert))
                conn.commit()
                
                # Dynamic Closed-Loop Decision: If anomaly detected, force Warning RED LED
                if anomaly == 1:
                    ip = node_id_to_ip(node_id)
                    trigger_event = f"Anomaly: High Heart Rate ({heart_rate} BPM)"
                    action_taken = f"Auto-latch Red Warning LED via CoAP"
                    
                    cursor.execute("""
                    INSERT INTO closed_loop_control_log (node_id, trigger_event, action_taken)
                    VALUES (%s, %s, %s)
                    """, (node_id, trigger_event, action_taken))
                    conn.commit()
                    
                    print(f"[CLOSED-LOOP] Automatic anomaly detected on {node_id} ({heart_rate} BPM). Latching LED to RED.")
                    # Spawn asynchronous thread to trigger remote CoAP command
                    threading.Thread(target=coap_set_actuator, args=(ip, "on"), daemon=True).start()
                    
            elif topic == "health/node/alert":
                # Handle Button Emergency Alerts Topic
                node_id = data.get("node")
                alert_type = data.get("type", "EMERGENCY")
                message = data.get("message", "Patient pressed button!")
                
                trigger_event = f"Manual Alert: {message}"
                action_taken = "Logged emergency alert, notified nurses, and actuated RED indicator"
                
                cursor.execute("""
                INSERT INTO closed_loop_control_log (node_id, trigger_event, action_taken)
                VALUES (%s, %s, %s)
                """, (node_id, trigger_event, action_taken))
                conn.commit()
                
                print(f"[CLOSED-LOOP] EMERGENCY alert received from {node_id}: {message}")
                
        except db.mysql.connector.Error as err:
            print(f"[DB ERROR] {err}")
        finally:
            cursor.close()
            conn.close()
            
    except Exception as e:
        print(f"[MQTT CLIENT ERROR] {e}")

def run_mqtt_loop():
    """Connects to the local MQTT broker and starts the background listener loop."""
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        # Connect to local Mosquitto broker running on localhost
        client.connect("127.0.0.1", 1883, 60)
        client.loop_forever()
    except Exception as e:
        print(f"[MQTT ERROR] Failed to connect: {e}")

def start_mqtt_client():
    """Spawns the MQTT loop in a separate daemon background thread."""
    t = threading.Thread(target=run_mqtt_loop, daemon=True)
    t.start()
    print("[MQTT] Background subscriber thread started.")
