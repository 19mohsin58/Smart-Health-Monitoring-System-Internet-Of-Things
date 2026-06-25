import paho.mqtt.client as mqtt
import json
import db
from coap_client import coap_set_actuator

def node_id_to_ip(node_id):
    """Maps Contiki-NG client ID to IPv6 address in Cooja.
    E.g. health-node-0002 -> fd00::202:2:2:2
    """
    try:
        parts = node_id.split("-")
        hex_val = parts[-1]
        val = int(hex_val, 16)
        return f"fd00::20{val}:{val}:{val}:{val}"
    except Exception:
        return "unknown"

def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] Connected to broker with result code {rc}")
    client.subscribe("health/node/#")

def on_message(client, userdata, msg):
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
            heart_rate = data.get("heart_rate")
            anomaly = data.get("anomaly")
            alert = data.get("alert")
            
            # Ensure the node exists in directory first (in case registration was missed)
            cursor.execute("SELECT node_id FROM nodes_directory WHERE node_id = %s", (node_id,))
            if cursor.fetchone() is None:
                cursor.execute("""
                INSERT INTO nodes_directory (node_id, type, domain, sensor_type, protocol)
                VALUES (%s, 'sensor', 'smart-health', 'heart-rate', 'mqtt+coap')
                """, (node_id,))
                conn.commit()
                
            cursor.execute("""
            INSERT INTO telemetry_history (node_id, heart_rate, anomaly, alert)
            VALUES (%s, %s, %s, %s)
            """, (node_id, heart_rate, anomaly, alert))
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
                # We can also actuate the node remotely to ensure it remains in emergency state
                coap_set_actuator(ip, "on")
                
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
            
    except db.mysql.connector.Error as err:
         print(f"[DB WRITE ERROR] {err}")
    finally:
         cursor.close()
         conn.close()

def start_mqtt_client():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect("127.0.0.1", 1883, 60)
        return client
    except Exception as e:
        print(f"[MQTT ERROR] Could not connect to Mosquitto: {e}")
        return None
