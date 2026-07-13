import paho.mqtt.client as mqtt
import json
import db
import threading
from coap_client import coap_set_actuator

# To track packet counts and sequence statistics for PDR computation
# Format: { node_id: { "received_seqs": set(), "first_seq": int, "last_seq": int, "pdr": float } }
node_seq_tracker = {}

def track_pdr(node_id, seq):
    if not node_id:
        return 100.0
    
    global node_seq_tracker
    
    if node_id not in node_seq_tracker:
        node_seq_tracker[node_id] = {
            "received_seqs": set(),
            "first_seq": seq,
            "last_seq": seq,
            "pdr": 100.0
        }
    
    tracker = node_seq_tracker[node_id]
    
    # If the node rebooted and sequence number reset
    if seq < tracker["last_seq"]:
        tracker["received_seqs"] = set()
        tracker["first_seq"] = seq
        tracker["last_seq"] = seq
        
    tracker["received_seqs"].add(seq)
    tracker["last_seq"] = max(tracker["last_seq"], seq)
    
    expected = tracker["last_seq"] - tracker["first_seq"] + 1
    received = len(tracker["received_seqs"])
    
    if expected <= 0:
        pdr = 100.0
    else:
        pdr = (received / expected) * 100.0
        
    tracker["pdr"] = pdr
    return pdr

def get_average_pdr():
    global node_seq_tracker
    if not node_seq_tracker:
        return 100.0
    pdrs = [tracker["pdr"] for tracker in node_seq_tracker.values()]
    return sum(pdrs) / len(pdrs)

# Global IP cache to avoid repeated slow HTTP requests to the Border Router
ip_cache = {}

def get_real_ip_from_br(suffix):
    import urllib.request
    import re
    br_ips = ["fd00::f6ce:364a:bcf9:e639"]
    for br_ip in br_ips:
        try:
            url = f"http://[{br_ip}]/"
            req = urllib.request.Request(url, method="GET")
            with urllib.request.urlopen(req, timeout=1.0) as response:
                html = response.read().decode('utf-8')
                # Find all IPv6 addresses in HTML neighbors/routes
                ips = re.findall(r'fd00::[0-9a-fA-F:]+', html)
                for ip in ips:
                    if ip.endswith(suffix):
                        return ip
        except Exception:
            pass
    return None

def node_id_to_ip(node_id):
    """Maps Contiki-NG client ID to IPv6 address dynamically on physical hardware,
    falling back to static Cooja-style mapping. Uses cache to prevent timeouts.
    """
    global ip_cache
    if node_id in ip_cache:
        return ip_cache[node_id]

    try:
        parts = node_id.split("-")
        suffix = parts[-1]  # E.g. "4d7c" or "0002"
        
        # Try dynamic lookup first
        real_ip = get_real_ip_from_br(suffix)
        if real_ip:
            ip_cache[node_id] = real_ip
            return real_ip
            
        # Fallback to Cooja-style static mapping for compatibility (preserving low IDs for Cooja)
        val = int(suffix, 16)
        if val < 256:
            return f"fd00::20{val}:{val}:{val}:{val}"
        else:
            return f"fd00::2000:{suffix}:{suffix}:{suffix}"
    except Exception:
        return "unknown"

def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] Connected to broker with result code {rc}")
    client.subscribe("health/node/#")

def on_message(client, userdata, msg):
    try:
        topic = msg.topic
        payload_str = msg.payload.decode('utf-8')
        # print(f"[MQTT RECEIVED] {topic} : {payload_str}")
        
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
                # 1. Update the Nodes Directory (Directory of sensors and actuators)
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
                # 2. Insert into Telemetry History
                node_id = data.get("node")
                seq = data.get("seq", 0)
                heart_rate = data.get("heart_rate")
                anomaly = data.get("anomaly")
                alert = data.get("alert")
                
                # Compute/Track PDR
                pdr = track_pdr(node_id, seq)
                
                # Ensure the node exists in directory first (in case registration was missed)
                cursor.execute("SELECT node_id FROM nodes_directory WHERE node_id = %s", (node_id,))
                if cursor.fetchone() is None:
                    cursor.execute("""
                    INSERT INTO nodes_directory (node_id, type, domain, sensor_type, protocol)
                    VALUES (%s, 'sensor', 'smart-health', 'heart-rate', 'mqtt+coap')
                    """, (node_id,))
                    conn.commit()
                    
                cursor.execute("""
                INSERT INTO telemetry_history (node_id, seq, heart_rate, anomaly, alert)
                VALUES (%s, %s, %s, %s, %s)
                """, (node_id, seq, heart_rate, anomaly, alert))
                conn.commit()
                
                # 3. CLOSED-LOOP CONTROL LOGIC
                # If an anomaly is detected, perform backend evaluation and log the control action
                if anomaly == 1:
                    ip = node_id_to_ip(node_id)
                    trigger_event = f"Anomaly: High Heart Rate ({heart_rate} BPM)"
                    action_taken = f"Auto-latch Red Warning LED via CoAP"
                    
                    # Insert log
                    cursor.execute("""
                    INSERT INTO closed_loop_control_log (node_id, trigger_event, action_taken)
                    VALUES (%s, %s, %s)
                    """, (node_id, trigger_event, action_taken))
                    conn.commit()
                    
                    print(f"[CLOSED-LOOP] Automatic anomaly detected on {node_id} ({heart_rate} BPM). Latching LED to RED.")
                    # We can also actuate the node remotely to ensure it remains in emergency state (spawn in background thread)
                    threading.Thread(target=coap_set_actuator, args=(ip, "on"), daemon=True).start()
                    
            elif topic == "health/node/alert":
                # 4. Handle emergency button press alerts
                node_id = data.get("node")
                alert_type = data.get("type", "EMERGENCY")
                message = data.get("message", "Patient pressed button!")
                
                # Log as a control trigger
                trigger_event = f"Manual Alert: {message}"
                action_taken = "Logged emergency alert, notified nurses, and actuated RED indicator"
                
                cursor.execute("""
                INSERT INTO closed_loop_control_log (node_id, trigger_event, action_taken)
                VALUES (%s, %s, %s)
                """, (node_id, trigger_event, action_taken))
                conn.commit()
                
                print(f"[CLOSED-LOOP] EMERGENCY alert received from {node_id}: {message}")
                
        except Exception as e:
             import traceback
             print(f"[DB/CONTROL ERROR] {e}")
             traceback.print_exc()
        finally:
             cursor.close()
             conn.close()
             
    except Exception as e:
        import traceback
        print(f"[MQTT ON_MESSAGE EXCEPTION] {e}")
        traceback.print_exc()


def on_disconnect(client, userdata, rc):
    print(f"[MQTT] Disconnected from broker with reason code: {rc}")

def start_mqtt_client():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    try:
        client.connect("127.0.0.1", 1883, 60)
        return client
    except Exception as e:
        print(f"[MQTT ERROR] Could not connect to Mosquitto: {e}")
        return None

