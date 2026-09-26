"""
===================================================================
AIoT Cloud Server Agent (Python)
Lắng nghe MQTT Telemetry từ ESP32 -> Gọi Gemini -> Trả lời qua MQTT
===================================================================
Cài đặt thư viện:
pip install paho-mqtt google-generativeai
"""

import json
import os
import paho.mqtt.client as mqtt
import google.generativeai as genai

# 1. Cấu hình Gemini & HiveMQ Cloud
GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "YOUR_GEMINI_API_KEY_HERE")
MQTT_BROKER = "65ca77a331134a66a1a44e5df8ef9a09.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "IoT_TEST"
MQTT_PASS = "mt21062005"

genai.configure(api_key=GEMINI_API_KEY)
model = genai.GenerativeModel("gemini-3.6-flash")

def on_connect(client, userdata, flags, rc):
    print(f"[INFO] Connected to HiveMQ Cloud Broker (RC: {rc})")
    # Lắng nghe telemetry từ tất cả các thiết bị ESP32
    client.subscribe("device/+/telemetry")
    print("[INFO] Subscribed to topic: device/+/telemetry")

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
        mac = payload.get("mac_address", "")
        data = payload.get("data", {})
        
        user_prompt = data.get("user_prompt")
        if user_prompt:
            print(f"\n[USER_PROMPT] [FROM ESP32 {mac}]: {user_prompt}")
            
            # Ngữ cảnh cảm biến & Edge AI từ ESP32
            context = f"Dữ liệu thiết bị {mac}: Rung động={data.get('live_vibr_rms', 'N/A')} mm/s, Nhiệt độ={data.get('live_temp', 'N/A')} C. Câu hỏi: {user_prompt}"
            
            # Hỏi Gemini
            response = model.generate_content(context)
            ai_reply = response.text.strip()
            
            print(f"[GEMINI_REPLY]: {ai_reply}")
            
            # Gửi câu trả lời về lại ESP32 qua topic control
            reply_topic = f"device/{mac}/control"
            control_payload = {
                "mac_address": mac,
                "data": {
                    "ai_chat": ai_reply
                }
            }
            client.publish(reply_topic, json.dumps(control_payload))
            print(f"[INFO] Published response to: {reply_topic}")
            
    except Exception as e:
        print(f"[ERROR] Message handling exception: {e}")

if __name__ == "__main__":
    client = mqtt.Client()
    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.tls_set()
    client.on_connect = on_connect
    client.on_message = on_message

    print("[INFO] Starting AIoT Cloud AI Agent...")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever()

