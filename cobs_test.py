import serial
import time
from cobs import cobs

# Configuración del puerto serial
# CAMBIA ESTO POR TU PUERTO COM (ej: 'COM3', 'COM5')
SERIAL_PORT = 'COM9' 
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

def read_response(ser):
    try:
        # Leer todo lo que haya en el buffer (con un pequeño timeout implícito del puerto)
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
        else:
            # Si no hay nada, esperar un poco a ver si llega algo
            time.sleep(0.1)
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
            else:
                print("No hubo respuesta (timeout)")
                return

        # Intentar decodificar como COBS primero
        # COBS termina en 0x00
        if b'\x00' in data:
            try:
                # Tomamos hasta el primer 0x00
                cobs_packet = data.split(b'\x00')[0]
                decoded = cobs.decode(cobs_packet)
                print(f"Respuesta [COBS]: {decoded.decode('utf-8', errors='replace')}")
                return
            except Exception:
                # Si falla, seguimos al fallback de texto
                pass

        # Fallback: Texto Plano
        try:
            text = data.decode('utf-8').strip()
            print(f"Respuesta [TEXTO]: {text}")
        except UnicodeDecodeError:
            print(f"Respuesta [RAW]: {data.hex()}")
            
    except Exception as e:
        print(f"Error leyendo respuesta: {e}")

if __name__ == "__main__":
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Conectado a {SERIAL_PORT} a {BAUD_RATE} baudios")
        
        while True:
            msg = input("Escribe un mensaje (o 'q' para salir): ")
            if msg.lower() == 'q':
                break
                
            send_cobs_message(ser, msg)
            time.sleep(0.5) # Dar tiempo al micro para procesar y responder
            read_response(ser)
            
    except serial.SerialException:
        print(f"No se pudo abrir el puerto {SERIAL_PORT}. Verifica que sea el correcto y esté libre.")
    except ImportError:
        print("Necesitas instalar la librería cobs: pip install cobs")
