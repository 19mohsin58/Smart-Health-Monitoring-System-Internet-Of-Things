import threading
import asyncio
from aiocoap import *

# ==============================================================================
# CoAP Client Wrapper Interfaces (coap_client.py)
# ==============================================================================
# Objective:
#   Provides synchronous wrapper functions to interface with aiocoap,
#   allowing multi-threaded backend workers to perform GET and POST
#   operations over IPv6 without crashing on malformed addresses.
# ==============================================================================

# Thread-local storage to cache CoAP client contexts per background thread
_local = threading.local()

async def get_coap_context():
    """Retrieves or instantiates a cached CoAP client context for the calling thread."""
    if not hasattr(_local, "coap_context"):
        _local.coap_context = await Context.create_client_context()
    return _local.coap_context

async def _async_coap_get(ip, path="health/status"):
    """Performs an asynchronous CoAP GET request to query patient status."""
    protocol = await get_coap_context()
    uri = f"coap://[{ip}]/{path}"
    try:
        # Wrap message construction to capture malformed URL formatting exceptions
        request = Message(code=GET, uri=uri)
        response = await protocol.request(request).response
        return response.payload.decode('utf-8')
    except Exception as e:
        return f"Error: {e}"

async def _async_coap_post(ip, path="health/actuator", payload="mode=on"):
    """Performs an asynchronous CoAP POST request to modify actuator/alarm modes."""
    protocol = await get_coap_context()
    uri = f"coap://[{ip}]/{path}"
    try:
        # Wrap message construction to capture malformed URL formatting exceptions
        request = Message(code=POST, payload=payload.encode('utf-8'), uri=uri)
        response = await protocol.request(request).response
        return str(response.code)
    except Exception as e:
        return f"Error: {e}"


def coap_get_status(ip):
    """
    Synchronous wrapper to get health status from a node.
    Automatically handles event loop mapping for thread safety.
    """
    try:
        # Check if an event loop exists for this thread
        loop = asyncio.get_event_loop()
    except RuntimeError:
        # If not, create and mount a new thread event loop
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        
    if loop.is_running():
        # If the loop is already running, schedule the task from threadsafe executor
        future = asyncio.run_coroutine_threadsafe(_async_coap_get(ip), loop)
        return future.result()
    else:
        # Otherwise, block until the loop executes the async query
        return loop.run_until_complete(_async_coap_get(ip))

def coap_set_actuator(ip, mode):
    """
    Synchronous wrapper to set actuator mode (on/off/congestion_on/congestion_off) on a node.
    Automatically handles event loop mapping for thread safety.
    """
    payload = f"mode={mode}"
    try:
        loop = asyncio.get_event_loop()
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        
    if loop.is_running():
        future = asyncio.run_coroutine_threadsafe(_async_coap_post(ip, payload=payload), loop)
        return future.result()
    else:
        return loop.run_until_complete(_async_coap_post(ip, payload=payload))

if __name__ == '__main__':
    # Local CLI diagnostics test
    print("Testing GET status...")
    res = coap_get_status("fd00::202:2:2:2")
    print(f"Result: {res}")
