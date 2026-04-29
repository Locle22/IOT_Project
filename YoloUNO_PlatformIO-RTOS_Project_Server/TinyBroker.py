"""
TinyBroker.py — Local MQTT Broker cho ESP32-B (Server/Controller)
=================================================================
Chạy trên PC, nhận data từ TinyGateway → forward đến ESP32-B

Cách chạy: python TinyBroker.py
"""

from amqtt.broker import Broker
import asyncio
import logging
logging.basicConfig(level=logging.INFO)

broker_config = {
    'listeners': {
        'default': {
            'type': 'tcp',
            'bind': '0.0.0.0:1884'  # Port 1884 để không xung đột với Sensor broker (1883)
        }
    },
    'sys_interval': 10,
    'auth': {
        'allow-anonymous': True
    },
    'topic-check': {
        'enabled': True,
        'plugins': ['topic_taboo']
    }
}

async def start_broker():
    broker = Broker(broker_config)
    await broker.start()
    print("=" * 50)
    print("  TinyBroker (Server) — Port 1884")
    print("=" * 50)
    print("MQTT Broker started on 0.0.0.0:1884")

loop = asyncio.get_event_loop()
loop.run_until_complete(start_broker())
loop.run_forever()
