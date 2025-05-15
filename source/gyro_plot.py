import subprocess
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import math

# start C++ program
proc = subprocess.Popen(["./gyro_kalman"], stdout=subprocess.PIPE, text=True)

fig, ax = plt.subplots()
point, = ax.plot([], [], lw=3)
ax.set_xlim(-40, 40)
ax.set_ylim(-40, 40)
#ax.set_aspect('equal')
ax.grid()

def init():
    point.set_data([], [])
    return point,

def update(frame):
    global proc
    try:
        line_out = proc.stdout.readline()
        if not line_out:
            return point,

        pitch, roll = map(float, line_out.strip().split(", "))
        '''
        length = 0.8
        x = length * math.sin(math.radians(roll))
        y = length * math.sin(math.radians(pitch))
        line.set_data([0, x], [0, y])
        ax.set_title(f"Pitch: {pitch:.1f}°, Roll: {roll:.1f}°")'''
        
        point, = ax.plot([], [], 'ro', markersize=8)  # red point
        print(f"Pitch: {pitch:.1f}°, Roll: {roll:.1f}°")
        if i < 100:
            t = i / 100
            x = t * roll
            y = t * pitch
        else:
            x = roll
            y = pitch

        point.set_data(x, y)
        return point,
    except:
        pass
    return point,

ani = animation.FuncAnimation(fig, update, frames=120, init_func=init, blit=True, interval=30)
plt.show()

proc.kill()  # kill the C++ process when done
