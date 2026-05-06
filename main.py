import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from pyhisics import gravitational_acceleration, kinetic_energy, potential_energy, gravitational_acceleration_two_body, acceleration, get_total_acceleration, rk4_step

# Definimos los ejes de energía como objetos "fantasma" (para no romper tu update)
class FakeAx:
    def plot(self, *args, **kwargs): return plt.plot([], [])[0]
    def set_xlim(self, *args, **kwargs): pass
    def set_ylim(self, *args, **kwargs): pass
    def set_title(self, *args, **kwargs): pass

# --- LÓGICA DE MISIÓN ---
class Mision:
    def __init__(self):
        self.fase = 'ORBITA_1'
        self.r_inicial = 370e6
        self.r_objetivo = 12e6
        self.dv1 = 0
        self.dv2 = 0
        self.coords = {'ORBITA_1': [[], []], 'EN_TRANSFERENCIA': [[], []], 'ORBITA_2': [[], []]}

mision = Mision()

# --- CONSTANTES ---
G = 6.67430e-11
M = 5.972e24
R_EARTH = 6.371e6
M_LUNA = 7.34767309e22
R_LUNA = 1.737e6
dt = 1.0 
m = 1000.0

# --- ESTADO INICIAL ---

pos_luna = np.array([384.4e6, 0.0]) 
vel_luna = np.array([0.0, 1022.0]) 
pos_tierra = np.array([0.0, 0.0]) 
pos = np.array([mision.r_inicial, 0.0])
vel = np.array([0.0, np.sqrt(G * M / mision.r_inicial) + vel_luna[1] - 400.0])

# --- CONFIGURACIÓN GRÁFICA ---
fig, ax_orbit = plt.subplots(figsize=(9, 9))
ax_energy_total = ax_energy_kin = ax_energy_pot = FakeAx()

ax_orbit.set_xlim(-1.5e7, 1.5e7) # Zoom inicial en la Tierra
ax_orbit.set_ylim(-1.5e7, 1.5e7)
ax_orbit.set_aspect('equal')
ax_orbit.add_artist(plt.Circle((0, 0), 6.371e6, color='#1E90FF', alpha=0.2))
ax_orbit.set_facecolor('#0B0D17') 
fig.patch.set_facecolor('#0B0D17') 
ax_orbit.grid(color='white', alpha=0.1, linestyle=':') 
ax_orbit.tick_params(colors='white') 

# Elementos visuales
path_fantasma, = ax_orbit.plot([], [], color='cyan', linestyle=':', alpha=0.6, label='Predicción')
path_trans, = ax_orbit.plot([], [], 'k-', alpha=0.8, label='Hohmann')
path_2, = ax_orbit.plot([], [], 'g-', alpha=0.8, label='Final')
satellite, = ax_orbit.plot([], [], 'ro', markersize=5)
luna, = ax_orbit.plot([], [], 'o', color='lightgray', markersize=8, label='Luna')

# Telemetría
telemetria = ax_orbit.text(0.02, 0.95, '', transform=ax_orbit.transAxes, 
                           color='white', fontsize=10, verticalalignment='top',
                           family='monospace',
                           bbox=dict(boxstyle='round', facecolor='black', alpha=0.5))

times, e_total, e_kin, e_pot = [], [], [], []
t = 0

# --- MANIOBRA ---
def on_key(event):
    global vel, dt 
    
    if event.key == ' ' and mision.fase == 'ORBITA_1':
        r1, r2 = mision.r_inicial, mision.r_objetivo
        v_c1 = np.sqrt(G*M/r1)
        v_p_trans = v_c1 * np.sqrt((2*r2)/(r1+r2))
        mision.dv1 = v_p_trans - v_c1
        
        v_c2 = np.sqrt(G*M/r2)
        v_a_trans = v_c2 * np.sqrt((2*r1)/(r1+r2))
        mision.dv2 = v_c2 - v_a_trans
        
        mision.fase = 'TRANSF_INICIO'
        print("🚀 Ignición 1 (Hohmann) aplicada")

    else:
        v_norm = np.linalg.norm(vel)
        v_unitario = vel / v_norm
        impulso_manual = 200.0  
        
        if event.key == 'up':
            vel += v_unitario * impulso_manual
            mision.fase = 'MODO_LIBRE' 
            print(f"🔥 Prógrado (+{impulso_manual} m/s)")
            
        elif event.key == 'down':
            vel -= v_unitario * impulso_manual
            mision.fase = 'MODO_LIBRE'
            print(f"❄️ Retrógrado (-{impulso_manual} m/s)")
        
        elif event.key == '.':
            dt *= 2
            print(f"⏩ Factor de tiempo: {dt}s/frame")
        elif event.key == ',':
            dt /= 2
            print(f"⏪ Factor de tiempo: {dt}s/frame")

fig.canvas.mpl_connect('key_press_event', on_key)

def predecir_orbita_completa(p_ini, v_ini, p_luna_ini):
    p_fut = p_ini.copy()
    v_fut = v_ini.copy()
    pl_fut = p_luna_ini.copy()
    vl_fut = vel_luna.copy() 
    
    dist_luna = np.linalg.norm(p_ini - p_luna_ini)
    en_luna = dist_luna < 66e6
    
    # 1. Determinar el cuerpo dominante para el tiempo de simulación
    mu = (G * M_LUNA) if en_luna else (G * M)
    r_rel_ini = dist_luna if en_luna else np.linalg.norm(p_ini)
    v_rel_ini = np.linalg.norm(v_ini - vel_luna) if en_luna else np.linalg.norm(v_ini)
    
    epsilon = (v_rel_ini**2 / 2.0) - (mu / r_rel_ini)
    
    if epsilon < 0:
        a = -mu / (2.0 * epsilon)
        tiempo_total = 2 * np.pi * np.sqrt(abs(a**3) / mu)
    else:
        tiempo_total = 400000 # Trayectoria de escape

    trayectoria_para_dibujar = []
    pasos = 250
    dt_p = tiempo_total / pasos 
    
    for _ in range(pasos):
        # Avanzamos la física del satélite y de la luna en el futuro
        acc = get_total_acceleration(p_fut, pos_tierra, pl_fut, G, M, M_LUNA)
        v_fut += acc * dt_p
        p_fut += v_fut * dt_p
        
        # IMPORTANTE: La Luna también se mueve en la predicción
        acc_l = acceleration(pl_fut, pos_tierra, G, M)
        vl_fut += acc_l * dt_p
        pl_fut += vl_fut * dt_p
        
        if en_luna:
            # RESTAMOS la posición futura de la luna para obtener la forma circular
            # Esto nos da la posición relativa: (Satelite_futuro - Luna_futura)
            trayectoria_para_dibujar.append(p_fut - pl_fut)
        else:
            trayectoria_para_dibujar.append(p_fut.copy())
            
    return np.array(trayectoria_para_dibujar), en_luna



# --- UPDATE ---
def update(frame):
    global pos, vel, t, pos_luna, vel_luna, pos_tierra
    
    # 1. Bucle de Física (Sub-steps)
    # Metemos todo aquí dentro para que sea consistente
    for _ in range(25):
        # Satélite con RK4
        pos, vel = rk4_step(pos, vel, dt, pos_tierra, pos_luna, G, M, M_LUNA)
        
        # Luna (La movemos con la misma frecuencia que el satélite)
        acc_l = acceleration(pos_luna, pos_tierra, G, M)
        vel_luna += acc_l * dt
        pos_luna += vel_luna * dt
        
        t += dt
    
    # 2. CALCULAR LA ÓRBITA ACTUAL (La "geometría")
    puntos_relativos, en_luna = predecir_orbita_completa(pos, vel, pos_luna)
    
    if en_luna:
        # Sumamos la posición ACTUAL de la luna para que la órbita se mueva con ella
        puntos_visuales = puntos_relativos + pos_luna
        path_fantasma.set_color('#FF00FF')
    else:
        puntos_visuales = puntos_relativos
        path_fantasma.set_color('cyan')
    
    path_fantasma.set_data(puntos_visuales[:, 0], puntos_visuales[:, 1])


    # 5. Zoom Dinámico
    r_mag = np.linalg.norm(pos)
    limite = max(1.5e7, r_mag * 1.5) # Mantiene la Tierra a la vista, o se expande
    ax_orbit.set_xlim(-limite, limite)
    ax_orbit.set_ylim(-limite, limite)

    # --- CÁLCULO DE PARÁMETROS ORBITALES (Para UI) ---
    v_mag = np.linalg.norm(vel)
    altitud = r_mag - R_EARTH
    epsilon = (v_mag**2 / 2.0) - (G * M / r_mag)
    h_mag = pos[0] * vel[1] - pos[1] * vel[0]

    if epsilon < 0:
        a = -G * M / (2.0 * epsilon)
        e_arg = 1 + (2.0 * epsilon * h_mag**2) / (G * M)**2
        e = np.sqrt(max(0, e_arg)) 
        periapsis = a * (1.0 - e) - R_EARTH
        apoapsis = a * (1.0 + e) - R_EARTH
    else:
        periapsis = r_mag - R_EARTH 
        apoapsis = float('inf')
        e = 1.0 # Aproximación para visualización

    
    luna.set_data([pos_luna[0]], [pos_luna[1]])
    satellite.set_data([pos[0]], [pos[1]])
    
    # Impacto
    if r_mag < 6.371e6:
        print("💥 IMPACTO: El satélite se ha desintegrado.")
        ani.event_source.stop()
    r_mag_luna = np.linalg.norm(pos - pos_luna)
    ref = "DOMINIO: LUNA" if r_mag_luna < 66e6 else "DOMINIO: TIERRA"
    status = (
        f"ALTITUD:   {altitud/1000:8.2f} km\n"
        f"PERIAPSIS: {periapsis/1000:8.2f} km\n"
        f"APOAPSIS:  {apoapsis/1000:8.2f} km\n"
        f"------------------------\n"
        f"{ref}\n"
        f"Dist. Luna: {r_mag_luna/1000:.0f} km\n"
        f"------------------------\n"
        f"V: {v_mag:8.2f} m/s\n"
        f"Excentricidad: {e:.4f}\n" if epsilon < 0 else "Trayectoria de escape\n"
        
    )
    telemetria.set_text(status)
    t += dt

    # Fíjate que devolvemos path_fantasma para que se dibuje
    return satellite, path_fantasma, telemetria, luna

ani = FuncAnimation(fig, update, frames=10000, interval=10, blit=False)
plt.tight_layout()
plt.show()