import threading
import asyncio
from aiocoap import *

# ==============================================================================
# CoAP Client Wrapper Interfaces (coap_client.py)
# ==============================================================================
# Objective:
#   Provides synchronous wrapper functions to interface with aiocoap using a
#   single thread-safe background event loop and context. This design prevents
#   socket leaks, thread event loop leaks, and port exhaustion.
# ==============================================================================

# Shared single background event loop and CoAP context
_loop = None
_context = None
_loop_thread = None
_loop_ready = threading.Event()

def _run_background_loop():
    """Initializes and runs the permanent background asyncio event loop."""
    global _loop, _context
    _loop = asyncio.new_event_loop()
    asyncio.set_event_loop(_loop)
    
    # Initialize a single shared CoAP client context inside the event loop
    _context = _loop.run_until_complete(Context.create_client_context())
    
    # Signal main thread that loop is fully initialized
    _loop_ready.set()
    
    # Keep loop running forever to process background coroutines
    _loop.run_forever()

def start_coap_client():
    """Spawns the background loop thread and waits for initialization."""
    global _loop_thread
    if _loop_thread is None:
        _loop_thread = threading.Thread(target=_run_background_loop, daemon=True)
        _loop_thread.start()
        _loop_ready.wait()  # Block calling thread until background loop is ready

# Automatically start the background CoAP loop on module load
start_coap_client()

async def _async_coap_get(ip, path="health/status"):
    """Performs an asynchronous CoAP GET request to query patient status."""
    uri = f"coap://[{ip}]/{path}"
    try:
        request = Message(code=GET, uri=uri)
        response = await _context.request(request).response
        return response.payload.decode('utf-8')
    except Exception as e:
        return f"Error: {e}"

async def _async_coap_post(ip, path="health/actuator", payload="mode=on"):
    """Performs an asynchronous CoAP POST request to modify actuator/alarm modes."""
    uri = f"coap://[{ip}]/{path}"
    try:
        request = Message(code=POST, payload=payload.encode('utf-8'), uri=uri)
        response = await _context.request(request).response
        return str(response.code)
    except Exception as e:
        return f"Error: {e}"


def coap_get_status(ip):
    """
    Synchronous wrapper to get health status from a node.
    Dispatches task to the shared background event loop thread-safely.
    """
    future = asyncio.run_coroutine_threadsafe(_async_coap_get(ip), _loop)
    return future.result()

def coap_set_actuator(ip, mode):
    """
    Synchronous wrapper to set actuator mode (on/off/congestion_on/congestion_off) on a node.
    Dispatches task to the shared background event loop thread-safely.
    """
    payload = f"mode={mode}"
    future = asyncio.run_coroutine_threadsafe(_async_coap_post(ip, payload=payload), _loop)
    return future.result()

if __name__ == '__main__':
    # Local CLI diagnostics test
    print("Testing GET status...")
    res = coap_get_status("fd00::202:2:2:2")
    print(f"Result: {res}")
