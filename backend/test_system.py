import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import db
import mqtt_client
import coap_client
import unittest

class TestSmartHealthSystem(unittest.TestCase):
    
    def test_database_connection(self):
        """Verify we can connect to MySQL and query tables."""
        conn = db.get_connection()
        self.assertIsNotNone(conn)
        cursor = conn.cursor()
        cursor.execute("SHOW TABLES")
        tables = [row[0] for row in cursor.fetchall()]
        self.assertIn("nodes_directory", tables)
        self.assertIn("telemetry_history", tables)
        self.assertIn("closed_loop_control_log", tables)
        self.assertIn("network_metrics", tables)
        cursor.close()
        conn.close()

    def test_ip_address_mapping(self):
        """Verify node_id to IP mapping and fallback formatting."""
        # Test low value compatible with Cooja
        cooja_ip = mqtt_client.node_id_to_ip("health-node-0002")
        self.assertEqual(cooja_ip, "fd00::202:2:2:2")
        
        # Test high value compatible with hardware MACs
        hw_ip = mqtt_client.node_id_to_ip("health-node-4d7c")
        self.assertEqual(hw_ip, "fd00::2000:4d7c:4d7c:4d7c")

    def test_pdr_tracking(self):
        """Verify the packet delivery ratio computation logic."""
        # Reset tracker for a test node
        node_id = "test-node-1234"
        if node_id in mqtt_client.node_seq_tracker:
            del mqtt_client.node_seq_tracker[node_id]
            
        # First packet: seq 100
        pdr1 = mqtt_client.track_pdr(node_id, 100)
        self.assertEqual(pdr1, 100.0)
        
        # Second packet: seq 101 (continuous)
        pdr2 = mqtt_client.track_pdr(node_id, 101)
        self.assertEqual(pdr2, 100.0)
        
        # Third packet: seq 103 (gap of 1 packet)
        pdr3 = mqtt_client.track_pdr(node_id, 103)
        # Expected: [100, 101, 103] -> received 3 out of expected 4 (100, 101, 102, 103) -> 75%
        self.assertEqual(pdr3, 75.0)
        
        # Reboot simulation: seq 5 (seq < last_seq)
        pdr4 = mqtt_client.track_pdr(node_id, 5)
        # Reset tracker should give 100% PDR
        self.assertEqual(pdr4, 100.0)

    def test_coap_malformed_url_handling(self):
        """Verify CoAP client handles malformed URLs without throwing exceptions."""
        res_get = coap_client.coap_get_status("invalid-ip")
        self.assertTrue(res_get.startswith("Error") or res_get is None)
        
        res_post = coap_client.coap_set_actuator("invalid-ip", "on")
        self.assertTrue(res_post.startswith("Error") or res_post is None)

if __name__ == '__main__':
    unittest.main()
