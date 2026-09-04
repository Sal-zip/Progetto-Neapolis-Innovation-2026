#!/usr/bin/env python3
import argparse
import csv
import math
import queue
import sys
import threading
import time
from datetime import datetime
from pathlib import Path
import tkinter as tk
import tkintermapview

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None  # gestito più sotto: serve solo se non si usa --demo


# Raggio medio della Terra e fattori di conversione gradi -> metri
# (approssimazione equirettangolare, ottima su scale di poche decine di km).
METERS_PER_DEG_LAT = 110_540.0


def meters_per_deg_lon(lat_deg: float) -> float:
    return 111_320.0 * math.cos(math.radians(lat_deg))


def parse_line(raw: str):
    """Prova a interpretare una riga dalla seriale. Ritorna (lat, lon, bpm, co2, gesture) o None."""
    line = raw.strip()
    if not line or "," not in line:
        return None
    parts = line.split(",")
    
    try:
        lat = float(parts[0].strip())
        lon = float(parts[1].strip())
    except (ValueError, IndexError):
        return None
        
    if not (-90.0 <= lat <= 90.0) or not (-180.0 <= lon <= 180.0):
        return None
        
    bpm = 0
    co2 = 0
    gesture = False
    
    # Se il tuo STM32 invia anche i dati dei sensori (es: lat,lon,bpm,co2,gesture)
    if len(parts) >= 5:
        try:
            bpm = int(parts[2].strip())
            co2 = int(parts[3].strip())
            gesture = int(parts[4].strip()) > 0
        except ValueError:
            pass
            
    return lat, lon, bpm, co2, gesture


def list_ports():
    if serial is None:
        print("pyserial non installato: 'pip install pyserial'")
        return
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("Nessuna porta seriale trovata.")
        return
    print("Porte seriali disponibili:")
    for p in ports:
        print(f"  {p.device}  -  {p.description}")


def guess_port() -> str | None:
    """Tenta di indovinare la porta giusta cercando descrizioni tipiche
    (ST-Link VCP, STM32, USB Serial, ...). Ritorna None se non trova nulla
    di ovvio: in quel caso l'utente deve specificare --port esplicitamente."""
    if serial is None:
        return None
    candidates = list(serial.tools.list_ports.comports())
    keywords = ("stlink", "st-link", "stm32", "usb serial", "usb-serial", "cdc")
    for p in candidates:
        desc = (p.description or "").lower()
        if any(k in desc for k in keywords):
            return p.device
    return None


class SerialReader(threading.Thread):
    """Legge righe dalla seriale in un thread separato e mette i fix validi
    in una queue, così il thread principale (matplotlib) non si blocca mai
    in attesa della UART."""

    def __init__(self, port: str, baud: int, out_queue: "queue.Queue"):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.out_queue = out_queue
        self._stop = threading.Event()

    def run(self):
        try:
            ser = serial.Serial(self.port, self.baud, timeout=1)
        except serial.SerialException as exc:
            self.out_queue.put(("error", f"Impossibile aprire {self.port}: {exc}"))
            return

        with ser:
            while not self._stop.is_set():
                try:
                    raw = ser.readline().decode("ascii", errors="ignore")
                except serial.SerialException as exc:
                    self.out_queue.put(("error", f"Errore di lettura seriale: {exc}"))
                    return
                fix = parse_line(raw)
                if fix is not None:
                    self.out_queue.put(("fix", fix))

    def stop(self):
        self._stop.set()


class DemoReader(threading.Thread):
    """Sorgente finta per testare la mappa senza hardware collegato: simula
    un punto che si muove lentamente attorno a un centro fisso."""

    def __init__(self, out_queue: "queue.Queue"):
        super().__init__(daemon=True)
        self.out_queue = out_queue
        self._stop = threading.Event()

    def run(self):
        import random
        lat0, lon0 = 45.4642, 9.1900  # Milano, giusto per avere numeri plausibili
        t = 0.0
        while not self._stop.is_set():
            lat = lat0 + 0.0006 * math.sin(t / 8.0)
            lon = lon0 + 0.0006 * math.cos(t / 6.0)
            bpm = random.randint(60, 100)
            co2 = random.randint(400, 800)
            gesture = random.random() < 0.1
            self.out_queue.put(("fix", (lat, lon, bpm, co2, gesture)))
            t += 1.0
            time.sleep(0.5)

    def stop(self):
        self._stop.set()

class DashboardApp:
    def __init__(self, root, q, log_path=None):
        self.root = root
        self.q = q
        self.root.title("GNSS & Sensors - Controllo Operatore")
        self.root.geometry("1000x700")

        # Configurazione file di log (opzionale)
        self._log_file = None
        self._log_writer = None
        if log_path is not None:
            self._log_file = open(log_path, "w", newline="")
            self._log_writer = csv.writer(self._log_file)
            self._log_writer.writerow(["timestamp", "lat", "lon", "bpm", "co2", "gesture"])

        # --- LAYOUT PRINCIPALE ---
        # Colonna di sinistra (Dashboard info)
        self.left_frame = tk.Frame(root, width=280, bg="#2c3e50")
        self.left_frame.pack(side="left", fill="y")
        self.left_frame.pack_propagate(False) # Impedisce al frame di restringersi

        # Colonna di destra (Mappa)
        self.right_frame = tk.Frame(root)
        self.right_frame.pack(side="right", fill="both", expand=True)

        # --- WIDGET COLONNA SINISTRA ---
        self.titolo = tk.Label(self.left_frame, text="STATUS OPERATORE", fg="#ecf0f1", bg="#2c3e50", font=("Arial", 16, "bold"))
        self.titolo.pack(pady=25)

        # 3) Modalità Segui vs Esplora
        self.follow_mode = True
        self.btn_follow = tk.Button(self.left_frame, text="🎯 SEGUI: ON", bg="#2ecc71", fg="white", font=("Arial", 12, "bold"), command=self.toggle_follow)
        self.btn_follow.pack(pady=5, padx=20, fill="x")

        # Variabili per la traccia Heatmap
        self.last_lat = None
        self.last_lon = None

        self.bpm_lbl = tk.Label(self.left_frame, text="BPM: --", fg="#e74c3c", bg="#2c3e50", font=("Arial", 14, "bold"))
        self.bpm_lbl.pack(pady=15, anchor="w", padx=20)

        self.co2_lbl = tk.Label(self.left_frame, text="CO2: -- ppm", fg="#f1c40f", bg="#2c3e50", font=("Arial", 14, "bold"))
        self.co2_lbl.pack(pady=15, anchor="w", padx=20)

        self.gesture_lbl = tk.Label(self.left_frame, text="Gesture: In attesa", fg="#ecf0f1", bg="#2c3e50", font=("Arial", 14))
        self.gesture_lbl.pack(pady=15, anchor="w", padx=20)

        self.gps_lbl = tk.Label(self.left_frame, text="GPS: Ricerca segnale...", fg="#bdc3c7", bg="#2c3e50", font=("Arial", 10))
        self.gps_lbl.pack(pady=25, anchor="w", padx=20)

        # --- WIDGET MAPPA (DESTRA) ---
        self.map_widget = tkintermapview.TkinterMapView(self.right_frame, corner_radius=0)
        self.map_widget.pack(fill="both", expand=True)
        # Usa il satellite di Google Maps
        self.map_widget.set_tile_server("https://mt0.google.com/vt/lyrs=s&hl=en&x={x}&y={y}&z={z}&s=Ga", max_zoom=22)
        
        self.marker = None
        self.path_coords = []
        self.path_layer = None
        
        # Variabili per la griglia delle gesture
        self.origin = None
        self.grid_size = 5.0 # grandezza del quadrettino in metri
        self.painted_cells = set()

        # Avvia il "loop" per controllare i nuovi dati in arrivo dalla Seriale
        self.root.after(100, self.poll_queue)

    def toggle_follow(self):
        self.follow_mode = not self.follow_mode
        if self.follow_mode:
            self.btn_follow.config(text="🎯 SEGUI: ON", bg="#2ecc71")
            if self.last_lat is not None:
                self.map_widget.set_position(self.last_lat, self.last_lon)
        else:
            self.btn_follow.config(text="🗺️ ESPLORA: OFF", bg="#95a5a6")

    def poll_queue(self):
        try:
            # Svuota la coda leggendo tutti i dati disponibili
            while True:
                kind, payload = self.q.get_nowait()
                if kind == "error":
                    print(payload, file=sys.stderr)
                elif kind == "fix":
                    lat, lon, bpm, co2, gesture = payload
                    self.update_ui(lat, lon, bpm, co2, gesture)
        except queue.Empty:
            pass
            
        # Richiama questa stessa funzione tra 100 millisecondi
        self.root.after(100, self.poll_queue)

    def update_ui(self, lat, lon, bpm, co2, gesture):
        # Imposta l'origine al primo punto GPS ricevuto
        if self.origin is None:
            self.origin = (lat, lon)

        # 2) ALLARMI VISIVI (Se superi soglie critiche)
        if bpm > 140 or co2 > 1000:
            self.titolo.config(text="⚠️ ALLARME OPERATORE ⚠️", fg="#e74c3c")
        else:
            self.titolo.config(text="STATUS OPERATORE", fg="#ecf0f1")

        # Aggiorna i testi nella colonna di sinistra
        self.bpm_lbl.config(text=f"BPM: {bpm}")
        self.co2_lbl.config(text=f"CO2: {co2} ppm")
        
        if gesture:
            self.gesture_lbl.config(text="Gesture: RILEVATA!", fg="#2ecc71")
            
            # --- Calcolo e disegno del quadrettino verde ---
            x = (lon - self.origin[1]) * (111320.0 * math.cos(math.radians(self.origin[0])))
            y = (lat - self.origin[0]) * 110540.0
            
            cell_x = math.floor(x / self.grid_size)
            cell_y = math.floor(y / self.grid_size)
            
            if (cell_x, cell_y) not in self.painted_cells:
                self.painted_cells.add((cell_x, cell_y))
                x_min, y_min = cell_x * self.grid_size, cell_y * self.grid_size
                x_max, y_max = x_min + self.grid_size, y_min + self.grid_size
                
                lat_min = self.origin[0] + (y_min / 110540.0)
                lat_max = self.origin[0] + (y_max / 110540.0)
                lon_min = self.origin[1] + (x_min / (111320.0 * math.cos(math.radians(self.origin[0]))))
                lon_max = self.origin[1] + (x_max / (111320.0 * math.cos(math.radians(self.origin[0]))))
                
                self.map_widget.set_polygon(
                    [(lat_min, lon_min), (lat_max, lon_min), (lat_max, lon_max), (lat_min, lon_max)],
                    fill_color="#2ecc71", outline_color="#27ae60", border_width=2
                )
        else:
            self.gesture_lbl.config(text="Gesture: Nessuna", fg="#ecf0f1")
            
        self.gps_lbl.config(text=f"Lat: {lat:.6f}\nLon: {lon:.6f}")

        # Aggiorna o crea il Marker sulla mappa
        if self.marker is None:
            self.marker = self.map_widget.set_marker(lat, lon, text="Operatore 1")
            self.map_widget.set_position(lat, lon)
            self.map_widget.set_zoom(18)
        else:
            self.marker.set_position(lat, lon)
            
        # Centra la telecamera SOLO SE la modalità "Segui" è attiva (Feature 3)
        if self.follow_mode:
            self.map_widget.set_position(lat, lon)

        # 1) HEATMAP SUL PERCORSO
        if self.last_lat is not None and self.last_lon is not None:
            # Colore dinamico basato sui BPM
            if bpm < 100:
                color = "#2ecc71" # Verde
            elif bpm < 130:
                color = "#f1c40f" # Giallo
            else:
                color = "#e74c3c" # Rosso
                
            # Disegna il segmento dal punto precedente al punto attuale
            self.map_widget.set_path([(self.last_lat, self.last_lon), (lat, lon)], color=color, width=4)

        # Aggiorna le coordinate precedenti per il prossimo step
        self.last_lat = lat
        self.last_lon = lon

        # Salva nel log se abilitato
        if self._log_writer is not None:
            self._log_writer.writerow([datetime.now().isoformat(timespec="seconds"), lat, lon, bpm, co2, gesture])
            self._log_file.flush()

    def chiudi(self):
        if self._log_file is not None:
            self._log_file.close()

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="Porta seriale (es. COM5 o /dev/ttyACM0). Se omessa, si tenta l'auto-rilevamento.")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--list-ports", action="store_true", help="Elenca le porte seriali disponibili ed esce")
    parser.add_argument("--demo", action="store_true", help="Usa una sorgente GPS simulata invece della seriale reale")
    parser.add_argument("--log", default=None, help="Percorso file CSV di log (default: gnss_log_<timestamp>.csv)")
    args = parser.parse_args()

    if args.list_ports:
        list_ports()
        return

    if not args.demo and serial is None:
        print("pyserial non è installato. Esegui: pip install pyserial", file=sys.stderr)
        sys.exit(1)

    port = args.port
    if not args.demo and port is None:
        port = guess_port()
        if port is None:
            print("Non riesco a indovinare la porta seriale. Usa --list-ports per vederle tutte,", file=sys.stderr)
            print("poi rilancia con --port <nome porta>.", file=sys.stderr)
            sys.exit(1)
        print(f"Porta auto-rilevata: {port}")

    if args.log:
        log_path = Path(args.log)
    else:
        log_dir = Path("log")
        log_dir.mkdir(exist_ok=True)
        log_path = log_dir / f"gnss_log_{datetime.now():%Y%m%d_%H%M%S}.csv"

    q: "queue.Queue" = queue.Queue()
    if args.demo:
        reader = DemoReader(q)
        print("Modalità demo: genero un fix GPS simulato ogni 0.5s (nessun hardware necessario).")
    else:
        reader = SerialReader(port, args.baud, q)
        print(f"Ascolto {port} a {args.baud} baud... (Ctrl+C per uscire)")
    reader.start()

    root = tk.Tk()
    app = DashboardApp(root, q, log_path)

    def on_closing():
        reader.stop()
        app.chiudi()
        root.destroy()
        
    root.protocol("WM_DELETE_WINDOW", on_closing)

    try:
        root.mainloop()  # Avvia l'interfaccia grafica
    except KeyboardInterrupt:
        pass
    finally:
        reader.stop()
        app.chiudi()

if __name__ == "__main__":
    main()