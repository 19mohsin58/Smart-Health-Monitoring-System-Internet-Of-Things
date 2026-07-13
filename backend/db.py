import mysql.connector
import sys

DB_CONFIG = {
    'host': '127.0.0.1',
    'user': 'health_admin',
    'password': 'health_password',
    'database': 'smart_health_db'
}

def get_connection():
    try:
        return mysql.connector.connect(**DB_CONFIG)
    except mysql.connector.Error as err:
        print(f"[DB ERROR] Failed to connect to MySQL: {err}")
        return None

def init_db():
    conn = get_connection()
    if conn is None:
        print("[DB ERROR] Could not initialize database tables. Exiting.")
        sys.exit(1)
    
    cursor = conn.cursor()
    try:
        # 1. Directory of sensors and actuators
        cursor.execute("""
        CREATE TABLE IF NOT EXISTS nodes_directory (
            node_id VARCHAR(50) PRIMARY KEY,
            type VARCHAR(50) NOT NULL,
            domain VARCHAR(50) NOT NULL,
            sensor_type VARCHAR(50) NOT NULL,
            protocol VARCHAR(20) NOT NULL,
            registered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
        """)
        
        # 2. Telemetry logs (heart rate and anomalies)
        cursor.execute("""
        CREATE TABLE IF NOT EXISTS telemetry_history (
            id INT AUTO_INCREMENT PRIMARY KEY,
            node_id VARCHAR(50) NOT NULL,
            seq INT DEFAULT 0,
            heart_rate INT NOT NULL,
            anomaly INT NOT NULL,
            alert INT NOT NULL,
            received_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (node_id) REFERENCES nodes_directory(node_id) ON DELETE CASCADE
        )
        """)
        
        # Check if 'seq' column exists, if not, add it
        cursor.execute("SHOW COLUMNS FROM telemetry_history LIKE 'seq'")
        if not cursor.fetchone():
            cursor.execute("ALTER TABLE telemetry_history ADD COLUMN seq INT DEFAULT 0 AFTER node_id")
        
        # 3. Closed-loop control logs (actions executed based on alerts)
        cursor.execute("""
        CREATE TABLE IF NOT EXISTS closed_loop_control_log (
            id INT AUTO_INCREMENT PRIMARY KEY,
            node_id VARCHAR(50) NOT NULL,
            trigger_event VARCHAR(255) NOT NULL,
            action_taken VARCHAR(255) NOT NULL,
            executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (node_id) REFERENCES nodes_directory(node_id) ON DELETE CASCADE
        )
        """)

        # 4. Network performance metrics (for stress test evaluation)
        cursor.execute("""
        CREATE TABLE IF NOT EXISTS network_metrics (
            id INT AUTO_INCREMENT PRIMARY KEY,
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            pdr FLOAT NOT NULL,
            latency_ms FLOAT NOT NULL,
            congestion_active INT NOT NULL
        )
        """)
        
        conn.commit()
        print("[DB] Database tables initialized successfully.")
    except mysql.connector.Error as err:
        print(f"[DB ERROR] Error creating schema: {err}")
        sys.exit(1)
    finally:
        cursor.close()
        conn.close()

if __name__ == '__main__':
    init_db()

