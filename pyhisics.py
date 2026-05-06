import numpy as np

def gravitational_acceleration(pos, G, M):
    r_squared = np.dot(pos, pos) # r^2
    r_cubed = r_squared**1.5     # (r^2)^(3/2) = r^3
    return -G * M * pos / r_cubed

def gravitational_acceleration_two_body(pos1, pos2, G, M):
    r_vec = pos2 - pos1
    r_squared = np.dot(r_vec, r_vec)
    r_cubed = r_squared**1.5
    acc1 = G * M * r_vec / r_cubed  # Aceleración sobre el cuerpo 1 por el cuerpo 2
    return acc1

def acceleration(pos_i, pos_j, G, M_j):
    r = pos_j - pos_i
    # Evitar división por cero si colisionan
    norm_r = np.linalg.norm(r)
    if norm_r == 0: return np.zeros_like(r)
    return G * M_j * r / norm_r**3

def get_total_acceleration(pos, pos_tierra, pos_luna, G, M, M_LUNA):
    acc_tierra = acceleration(pos, pos_tierra, G, M)
    acc_luna = acceleration(pos, pos_luna, G, M_LUNA)
    #print(f"Debug: Acc. Tierra = {acc_tierra}, Acc. Luna = {acc_luna}")
    return acc_tierra + acc_luna

def rk4_step(pos, vel, dt, pos_tierra, pos_luna, G, M, M_LUNA):
    # k1
    a1 = get_total_acceleration(pos, pos_tierra, pos_luna, G, M, M_LUNA)
    k1_v = a1 * dt
    k1_p = vel * dt
    
    # k2
    a2 = get_total_acceleration(pos + k1_p/2, pos_tierra, pos_luna, G, M, M_LUNA)
    k2_v = a2 * dt
    k2_p = (vel + k1_v/2) * dt
    
    # k3
    a3 = get_total_acceleration(pos + k2_p/2, pos_tierra, pos_luna, G, M, M_LUNA)
    k3_v = a3 * dt
    k3_p = (vel + k2_v/2) * dt
    
    # k4
    a4 = get_total_acceleration(pos + k3_p, pos_tierra, pos_luna, G, M, M_LUNA)
    k4_v = a4 * dt
    k4_p = (vel + k3_v) * dt
    
    new_p = pos + (k1_p + 2*k2_p + 2*k3_p + k4_p) / 6
    new_v = vel + (k1_v + 2*k2_v + 2*k3_v + k4_v) / 6
    return new_p, new_v

def kinetic_energy(m, vel):
    return 0.5 * m * np.linalg.norm(vel)**2

def potential_energy(m, G, M, pos):
    r = np.linalg.norm(pos)  # Distancia desde el centro de la Tierra
    return -G * M * m / r
