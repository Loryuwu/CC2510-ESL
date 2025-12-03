import serial
import time
from cobs import cobs

# Configuración del puerto serial
# CAMBIA ESTO POR TU PUERTO COM (ej: 'COM3', 'COM5')
SERIAL_PORT = 'COM6' 
BAUD_RATE = 115200

def send_cobs_message(ser, message):
    # 1. Codificar el mensaje usando COBS
    encoded = cobs.encode(message.encode('utf-8'))
    
    # 2. Añadir el byte 0x00 al final (delimitador)
    packet = encoded + b'\x00'
    
    print(f"Enviando: {message}")
    print(f"Bytes RAW: {packet.hex()}")
    
    # 3. Enviar por serial
    ser.write(packet)

def read_cobs_response(ser):
    # Leer hasta encontrar un 0x00
    data = ser.read_until(b'\x00')
    
    if len(data) > 0:
        # Quitar el 0x00 final
        encoded_data = data[:-1]
        
        try:
            # Decodificar
            decoded = cobs.decode(encoded_data)
            print(f"Recibido: {decoded.decode('utf-8')}")
        except Exception as e:
            print(f"Error decodificando: {e}")
            print(f"Data RAW recibida: {data.hex()}")

if __name__ == "__main__":
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Conectado a {SERIAL_PORT} a {BAUD_RATE} baudios")
        
        while True:
            msg = input("Escribe un mensaje (o 'q' para salir): ")
            if msg.lower() == 'q':
                break
                
            send_cobs_message(ser, msg)
            time.sleep(0.1) # Dar tiempo al micro para responder
            read_cobs_response(ser)
            
    except serial.SerialException:
        print(f"No se pudo abrir el puerto {SERIAL_PORT}. Verifica que sea el correcto y esté libre.")
    except ImportError:
        print("Necesitas instalar la librería cobs: pip install cobs")
