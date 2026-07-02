import json
import subprocess

def main():
    payload = {
        "D1": 1,
        "A0": 256,
        "dht_temp": 24.5,
        "dht_hum": 55.2
    }
    msg = json.dumps(payload)
    print(f"Publishing: {msg}")
    subprocess.run([
        "docker", "exec", "iot_mqtt", 
        "mosquitto_pub", 
        "-t", "cihaz/rapor/112233445566", 
        "-m", msg
    ])
    print("Done!")

if __name__ == "__main__":
    main()
