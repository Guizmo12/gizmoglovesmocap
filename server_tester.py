import socket
import matplotlib.pyplot as plt

# UDP settings
UDP_IP = "10.0.0.245"  # écoute sur toutes les interfaces
UDP_PORT = 12345

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# Initialisation des listes pour stocker les données
sensor_data = [[] for _ in range(5)]  # 5 capteurs

plt.ion()  # Mode interactif pour la mise à jour en temps réel du graphique

fig, ax = plt.subplots()

# Configuration initiale du graphique
ax.set_title('Données des capteurs de flexion')
ax.set_xlabel('Temps')
ax.set_ylabel('Angle de flexion')

lines = [ax.plot([], [], label=f'Capteur {i+1}')[0] for i in range(5)]
ax.legend()

# Fonction pour mettre à jour le graphique
def update_plot(data):
    for i, line in enumerate(lines):
        line.set_xdata(range(len(data[i])))
        line.set_ydata(data[i])
    ax.relim()
    ax.autoscale_view()
    fig.canvas.draw()
    fig.canvas.flush_events()

print("Attente de données UDP...")

while True:
    data, addr = sock.recvfrom(1024)  # Taille du tampon est de 1024 octets, ajustez si nécessaire
    
    try:
        data = data.decode().strip()
        angles = list(map(float, data.split('\t')))
        
        if len(angles) == 5:
            for i in range(5):
                sensor_data[i].append(angles[i])
            
            # Mettre à jour le graphique
            update_plot(sensor_data)
    
    except ValueError as e:
        print(f"Erreur de valeur: {e}")
        continue
    except Exception as e:
        print(f"Erreur: {e}")
        continue
