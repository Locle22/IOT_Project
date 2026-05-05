from amqtt.broker import Broker
import asyncio
import logging

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")

broker_config = {
    'listeners': {
        'default': {
            'type': 'tcp',
            'bind': '0.0.0.0:1883'
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

async def main():
    broker = Broker(broker_config)
    await broker.start()
    print("=" * 50)
    print("  ✅ MQTT Broker (TinyBroker) đang chạy!")
    print("  📡 Lắng nghe tại: 0.0.0.0:1883")
    print("  🛑 Nhấn Ctrl+C để dừng")
    print("=" * 50)

    try:
        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        print("\n🛑 Đang tắt broker...")
        await broker.shutdown()
        print("✅ Broker đã tắt.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n🛑 TinyBroker stopped.")
