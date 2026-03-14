import json
import mysql.connector
import paho.mqtt.client as mqtt

#Sql connection
db = mysql.connector.connect(
    host="localhost",
    user="root",
    password="",          
    database="mqtt_iot"
)

cursor = db.cursor()

# MQTT Callback 
def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())

        sql = """
        INSERT INTO sensor_data
        (node_id, temperature, humidity, light, prediction)
        VALUES (%s, %s, %s, %s, %s)
        """

        values = (
            data["node"],
            data["temp"],
            data["hum"],
            data["light"],
            data["pred"]
        )

        cursor.execute(sql, values)
        db.commit()

        print("Inserted:", values)

    except Exception as e:
        print("Error:", e)

#MQTT Setup 
client = mqtt.Client()
client.on_message = on_message

client.connect("localhost", 1883)
client.subscribe("farm/data")

print("MQTT → MySQL bridge running...")
client.loop_forever()
