import asyncio
import json
from aiocoap import *

async def _async_coap_get(ip, path="health/status"):
    protocol = await Context.create_client_context()
    # Format the CoAP URI using the node's IPv6 address
    uri = f"coap://[{ip}]/{path}"
    request = Message(code=GET, uri=uri)
    try:
        response = await protocol.request(request).response
        return response.payload.decode('utf-8')
    except Exception as e:
        return f"Error: {e}"

async def _async_coap_post(ip, path="health/actuator", payload="mode=on"):
    protocol = await Context.create_client_context()
    uri = f"coap://[{ip}]/{path}"
    request = Message(code=POST, payload=payload.encode('utf-8'), uri=uri)
    try:
        response = await protocol.request(request).response
        # Return a readable string of the CoAP response code (e.g. 2.04 Changed)
        return str(response.code)
    except Exception as e:
        return f"Error: {e}"

def coap_get_status(ip):
    """Synchronous wrapper to get health status from a node."""
    try:
        # Check if an event loop is already running in this thread
        loop = asyncio.get_event_loop()
    except RuntimeError:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        
    if loop.is_running():
        # If loop is running (e.g., inside an async application), schedule it
        future = asyncio.run_coroutine_threadsafe(_async_coap_get(ip), loop)
        return future.result()
    else:
        return loop.run_until_complete(_async_coap_get(ip))

def coap_set_actuator(ip, mode):
    """Synchronous wrapper to set actuator mode (on/off) on a node."""
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
    # Test queries if run directly
    print("Testing GET status...")
    res = coap_get_status("fd00::202:2:2:2")
    print(f"Result: {res}")
